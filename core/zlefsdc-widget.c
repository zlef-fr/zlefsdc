#include "zlefsdc-widget.h"
#include "zlefsdc-cover.h"
#include <math.h>

struct _ZlefsdcWidget {
  GtkEventBox    parent;

  ZlefsdcSettings *settings;
  ZlefsdcPlayer   *player;
  ZlefsdcCover    *cover_loader;

  GtkOrientation orientation;
  int            panel_size;

  GtkWidget     *box;            /* element container */
  GtkWidget     *cover;          /* GtkDrawingArea */
  GtkWidget     *icon;           /* GtkImage */
  GtkWidget     *info_evt;       /* GtkEventBox over the info column */
  GtkWidget     *title_lbl, *artist_lbl, *album_lbl;
  GtkWidget     *btn_prev, *btn_pp, *btn_next, *pp_img;
  GtkWidget     *progress;       /* GtkProgressBar */

  GdkPixbuf     *cover_pix;      /* current scaled art, or NULL */
  int            cover_px;       /* computed cover side */

  guint          marquee_id, progress_id;
  int            marquee_off;

  GtkCssProvider *css;
};

G_DEFINE_FINAL_TYPE (ZlefsdcWidget, zlefsdc_widget, GTK_TYPE_EVENT_BOX)

static void rebuild (ZlefsdcWidget *self);
static void apply_style (ZlefsdcWidget *self);
static void update_state (ZlefsdcWidget *self);

/* ===================== helpers ========================================== */

static int compute_cover_px (ZlefsdcWidget *self) {
  int s = zlefsdc_settings_get_int (self->settings, "cover.size");
  if (s <= 0) s = self->panel_size > 0 ? self->panel_size - 2 : 24;
  return CLAMP (s, 12, 512);
}

/* %t/%a/%b substitution */
static char *format_text (const char *fmt, const char *t, const char *a, const char *b) {
  GString *out = g_string_new (NULL);
  for (const char *p = fmt; *p; p++) {
    if (*p == '%' && p[1]) {
      switch (p[1]) {
        case 't': g_string_append (out, t ?: ""); break;
        case 'a': g_string_append (out, a ?: ""); break;
        case 'b': g_string_append (out, b ?: ""); break;
        case '%': g_string_append_c (out, '%'); break;
        default:  g_string_append_c (out, '%'); g_string_append_c (out, p[1]);
      }
      p++;
    } else g_string_append_c (out, *p);
  }
  return g_string_free (out, FALSE);
}

/* Set a label, honouring ellipsize/marquee. Marquee text is stashed and the
 * timer rotates it; otherwise we ellipsize to max_chars. */
static void set_label (ZlefsdcWidget *self, GtkWidget *lbl, const char *text) {
  if (!lbl) return;
  int maxc = zlefsdc_settings_get_int (self->settings, "text.max_chars");
  gboolean scroll = zlefsdc_settings_get_bool (self->settings, "text.scroll");

  g_object_set_data_full (G_OBJECT (lbl), "zl-full", g_strdup (text ?: ""), g_free);

  if (scroll && maxc > 0 && g_utf8_strlen (text ?: "", -1) > maxc) {
    gtk_label_set_ellipsize (GTK_LABEL (lbl), PANGO_ELLIPSIZE_NONE);
    gtk_label_set_width_chars (GTK_LABEL (lbl), maxc);
    gtk_label_set_max_width_chars (GTK_LABEL (lbl), maxc);
    /* leave actual text to the marquee tick */
  } else {
    gtk_label_set_ellipsize (GTK_LABEL (lbl), maxc > 0 ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE);
    gtk_label_set_width_chars (GTK_LABEL (lbl), maxc > 0 ? maxc : -1);
    gtk_label_set_max_width_chars (GTK_LABEL (lbl), maxc > 0 ? maxc : -1);
    gtk_label_set_text (GTK_LABEL (lbl), text ?: "");
  }
}

static gboolean marquee_tick (gpointer data) {
  ZlefsdcWidget *self = data;
  int maxc = zlefsdc_settings_get_int (self->settings, "text.max_chars");
  if (maxc <= 0) return G_SOURCE_CONTINUE;
  self->marquee_off++;
  GtkWidget *labels[] = { self->title_lbl, self->artist_lbl, self->album_lbl };
  for (guint i = 0; i < G_N_ELEMENTS (labels); i++) {
    GtkWidget *lbl = labels[i];
    if (!lbl || !gtk_widget_get_visible (lbl)) continue;
    const char *full = g_object_get_data (G_OBJECT (lbl), "zl-full");
    if (!full) continue;
    glong len = g_utf8_strlen (full, -1);
    if (len <= maxc) continue;
    char *padded = g_strconcat (full, "   ", NULL);   /* gap before wrap */
    glong plen = g_utf8_strlen (padded, -1);
    glong off = self->marquee_off % plen;
    /* build a maxc-wide window with wrap-around */
    GString *win = g_string_new (NULL);
    for (int k = 0; k < maxc; k++) {
      glong idx = (off + k) % plen;
      const char *cp = g_utf8_offset_to_pointer (padded, idx);
      gunichar c = g_utf8_get_char (cp);
      g_string_append_unichar (win, c);
    }
    gtk_label_set_text (GTK_LABEL (lbl), win->str);
    g_string_free (win, TRUE);
    g_free (padded);
  }
  return G_SOURCE_CONTINUE;
}

static void ensure_marquee (ZlefsdcWidget *self) {
  gboolean want = zlefsdc_settings_get_bool (self->settings, "text.scroll");
  if (want && !self->marquee_id)
    self->marquee_id = g_timeout_add (220, marquee_tick, self);
  else if (!want && self->marquee_id) {
    g_source_remove (self->marquee_id); self->marquee_id = 0;
  }
}

static gboolean progress_tick (gpointer data) {
  ZlefsdcWidget *self = data;
  if (!self->progress) return G_SOURCE_CONTINUE;
  gint64 len = zlefsdc_player_get_length_us (self->player);
  gint64 pos = zlefsdc_player_get_position_us (self->player);
  double frac = (len > 0) ? CLAMP ((double) pos / (double) len, 0.0, 1.0) : 0.0;
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress), frac);
  return G_SOURCE_CONTINUE;
}

static void ensure_progress_timer (ZlefsdcWidget *self) {
  gboolean want = self->progress &&
                  zlefsdc_player_get_playback (self->player) == ZLEFSDC_PLAYBACK_PLAYING;
  if (want && !self->progress_id)
    self->progress_id = g_timeout_add_seconds (1, progress_tick, self);
  else if (!want && self->progress_id) {
    g_source_remove (self->progress_id); self->progress_id = 0;
  }
}

/* ===================== cover drawing ==================================== */

static gboolean cover_draw (GtkWidget *w, cairo_t *cr, gpointer data) {
  ZlefsdcWidget *self = data;
  int side = self->cover_px;
  int r = CLAMP (zlefsdc_settings_get_int (self->settings, "cover.radius"), 0, side / 2);

  /* rounded rect clip */
  double deg = G_PI / 180.0;
  cairo_new_sub_path (cr);
  cairo_arc (cr, side - r, r,        r, -90 * deg,   0 * deg);
  cairo_arc (cr, side - r, side - r, r,   0 * deg,  90 * deg);
  cairo_arc (cr, r,        side - r, r,  90 * deg, 180 * deg);
  cairo_arc (cr, r,        r,        r, 180 * deg, 270 * deg);
  cairo_close_path (cr);
  cairo_clip (cr);

  if (self->cover_pix) {
    gdk_cairo_set_source_pixbuf (cr, self->cover_pix, 0, 0);
    cairo_paint (cr);
  } else {
    /* placeholder tile in the theme's selection colour */
    GtkStyleContext *ctx = gtk_widget_get_style_context (w);
    GdkRGBA c;
    gtk_style_context_get_color (ctx, GTK_STATE_FLAG_NORMAL, &c);
    cairo_set_source_rgba (cr, c.red, c.green, c.blue, 0.12);
    cairo_paint (cr);
    /* a little note glyph */
    cairo_set_source_rgba (cr, c.red, c.green, c.blue, 0.5);
    cairo_set_font_size (cr, side * 0.5);
    cairo_text_extents_t ext;
    cairo_text_extents (cr, "♪", &ext);
    cairo_move_to (cr, (side - ext.width) / 2 - ext.x_bearing,
                       (side - ext.height) / 2 - ext.y_bearing);
    cairo_show_text (cr, "♪");
  }
  return TRUE;
}

static void on_cover_ready (GdkPixbuf *pix, gpointer data) {
  ZlefsdcWidget *self = data;
  g_clear_object (&self->cover_pix);
  if (pix) self->cover_pix = g_object_ref (pix);
  if (self->cover) gtk_widget_queue_draw (self->cover);
}

static void request_cover (ZlefsdcWidget *self) {
  if (!self->cover) return;
  const char *url = zlefsdc_player_get_art_url (self->player);
  if (!url || !*url) { g_clear_object (&self->cover_pix); gtk_widget_queue_draw (self->cover); return; }
  zlefsdc_cover_request (self->cover_loader, url, self->cover_px, on_cover_ready, self);
}

/* ===================== quick actions =================================== */

static void do_action (ZlefsdcWidget *self, ZlefsdcAction a) {
  switch (a) {
    case ZLEFSDC_ACTION_PLAYPAUSE: zlefsdc_player_play_pause (self->player); break;
    case ZLEFSDC_ACTION_NEXT:      zlefsdc_player_next (self->player); break;
    case ZLEFSDC_ACTION_PREVIOUS:  zlefsdc_player_previous (self->player); break;
    case ZLEFSDC_ACTION_STOP:      zlefsdc_player_stop (self->player); break;
    case ZLEFSDC_ACTION_RAISE:     zlefsdc_player_raise (self->player); break;
    case ZLEFSDC_ACTION_COMMAND: {
      const char *cmd = zlefsdc_settings_get_string (self->settings, "action.command");
      if (cmd && *cmd) g_spawn_command_line_async (cmd, NULL);
      break;
    }
    default: break;
  }
}

static gboolean on_cover_press (GtkWidget *w, GdkEventButton *e, gpointer data) {
  (void) w;
  ZlefsdcWidget *self = data;
  if (e->button == 2) do_action (self, zlefsdc_settings_get_action (self->settings, "action.middle_click"));
  else if (e->button == 1) do_action (self, zlefsdc_settings_get_action (self->settings, "action.cover_click"));
  return TRUE;
}

static gboolean on_info_press (GtkWidget *w, GdkEventButton *e, gpointer data) {
  (void) w;
  ZlefsdcWidget *self = data;
  if (e->button == 2) do_action (self, zlefsdc_settings_get_action (self->settings, "action.middle_click"));
  else if (e->button == 1) do_action (self, zlefsdc_settings_get_action (self->settings, "action.info_click"));
  return TRUE;
}

static gboolean on_scroll (GtkWidget *w, GdkEventScroll *e, gpointer data) {
  (void) w;
  ZlefsdcWidget *self = data;
  GdkScrollDirection dir = e->direction;
  double dy = 0;
  if (dir == GDK_SCROLL_SMOOTH) { gdk_event_get_scroll_deltas ((GdkEvent *) e, NULL, &dy); if (dy == 0) return TRUE; }
  if (dir == GDK_SCROLL_UP || (dir == GDK_SCROLL_SMOOTH && dy < 0))
    do_action (self, zlefsdc_settings_get_action (self->settings, "action.scroll_up"));
  else if (dir == GDK_SCROLL_DOWN || (dir == GDK_SCROLL_SMOOTH && dy > 0))
    do_action (self, zlefsdc_settings_get_action (self->settings, "action.scroll_down"));
  return TRUE;
}

static void on_btn_prev (GtkButton *b, gpointer d) { (void) b; zlefsdc_player_previous (((ZlefsdcWidget *) d)->player); }
static void on_btn_pp   (GtkButton *b, gpointer d) { (void) b; zlefsdc_player_play_pause (((ZlefsdcWidget *) d)->player); }
static void on_btn_next (GtkButton *b, gpointer d) { (void) b; zlefsdc_player_next (((ZlefsdcWidget *) d)->player); }

/* ===================== construction of elements ======================== */

static const char *btn_icon (gboolean symbolic, const char *base) {
  static char buf[64];
  g_snprintf (buf, sizeof buf, "%s%s", base, symbolic ? "-symbolic" : "");
  return buf;
}

static GtkWidget *make_button (ZlefsdcWidget *self, const char *icon_base, GCallback cb) {
  gboolean symbolic = zlefsdc_settings_get_bool (self->settings, "buttons.symbolic");
  int sz = zlefsdc_settings_get_int (self->settings, "buttons.icon_size");
  GtkWidget *img = gtk_image_new_from_icon_name (btn_icon (symbolic, icon_base), GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (img), sz);
  GtkWidget *btn = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (btn), img);
  if (zlefsdc_settings_get_bool (self->settings, "buttons.flat")) {
    gtk_button_set_relief (GTK_BUTTON (btn), GTK_RELIEF_NONE);
    gtk_style_context_add_class (gtk_widget_get_style_context (btn), "flat");
  }
  gtk_widget_set_focus_on_click (btn, FALSE);
  gtk_widget_set_valign (btn, GTK_ALIGN_CENTER);
  g_signal_connect (btn, "clicked", cb, self);
  return btn;
}

/* Build the info column (title/artist/album) wrapped in a click event box. */
static GtkWidget *make_info (ZlefsdcWidget *self) {
  gboolean inline_mode = zlefsdc_settings_get_bool (self->settings, "layout.info_inline");
  GtkWidget *col = gtk_box_new (inline_mode ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL,
                                inline_mode ? 6 : 0);
  gtk_widget_set_valign (col, GTK_ALIGN_CENTER);

  if (zlefsdc_settings_get_bool (self->settings, "show.title")) {
    self->title_lbl = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (self->title_lbl), 0.0);
    gtk_style_context_add_class (gtk_widget_get_style_context (self->title_lbl), "zl-title");
    gtk_box_pack_start (GTK_BOX (col), self->title_lbl, FALSE, FALSE, 0);
  }
  if (zlefsdc_settings_get_bool (self->settings, "show.artist")) {
    self->artist_lbl = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (self->artist_lbl), 0.0);
    gtk_style_context_add_class (gtk_widget_get_style_context (self->artist_lbl), "zl-artist");
    gtk_box_pack_start (GTK_BOX (col), self->artist_lbl, FALSE, FALSE, 0);
  }
  if (zlefsdc_settings_get_bool (self->settings, "show.album")) {
    self->album_lbl = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (self->album_lbl), 0.0);
    gtk_style_context_add_class (gtk_widget_get_style_context (self->album_lbl), "zl-album");
    gtk_box_pack_start (GTK_BOX (col), self->album_lbl, FALSE, FALSE, 0);
  }

  self->info_evt = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (self->info_evt), FALSE);
  gtk_container_add (GTK_CONTAINER (self->info_evt), col);
  g_signal_connect (self->info_evt, "button-press-event", G_CALLBACK (on_info_press), self);
  return self->info_evt;
}

static GtkWidget *make_cover (ZlefsdcWidget *self) {
  self->cover_px = compute_cover_px (self);
  self->cover = gtk_drawing_area_new ();
  gtk_widget_set_size_request (self->cover, self->cover_px, self->cover_px);
  gtk_widget_set_valign (self->cover, GTK_ALIGN_CENTER);
  gtk_widget_add_events (self->cover, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (self->cover, "draw", G_CALLBACK (cover_draw), self);
  g_signal_connect (self->cover, "button-press-event", G_CALLBACK (on_cover_press), self);
  return self->cover;
}

static GtkWidget *make_icon (ZlefsdcWidget *self) {
  self->icon = gtk_image_new ();
  int sz = zlefsdc_settings_get_int (self->settings, "buttons.icon_size") + 4;
  gtk_image_set_pixel_size (GTK_IMAGE (self->icon), CLAMP (sz, 12, 64));
  gtk_widget_set_valign (self->icon, GTK_ALIGN_CENTER);
  return self->icon;
}

static GtkWidget *make_progress (ZlefsdcWidget *self) {
  self->progress = gtk_progress_bar_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (self->progress), "zl-progress");
  if (self->orientation == GTK_ORIENTATION_VERTICAL) {
    gtk_orientable_set_orientation (GTK_ORIENTABLE (self->progress), GTK_ORIENTATION_VERTICAL);
    gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR (self->progress), TRUE);
  }
  gtk_widget_set_valign (self->progress, GTK_ALIGN_CENTER);
  return self->progress;
}

/* ===================== rebuild / state ================================= */

static void clear_refs (ZlefsdcWidget *self) {
  self->cover = self->icon = self->info_evt = NULL;
  self->title_lbl = self->artist_lbl = self->album_lbl = NULL;
  self->btn_prev = self->btn_pp = self->btn_next = self->pp_img = NULL;
  self->progress = NULL;
}

static void rebuild (ZlefsdcWidget *self) {
  if (self->box) gtk_widget_destroy (self->box);
  clear_refs (self);

  int spacing = zlefsdc_settings_get_int (self->settings, "layout.spacing");
  self->box = gtk_box_new (self->orientation, spacing);
  gtk_container_add (GTK_CONTAINER (self), self->box);

  const char *order = zlefsdc_settings_get_string (self->settings, "layout.order");
  char **tokens = g_strsplit (order && *order ? order : "cover,icon,info,prev,playpause,next,progress", ",", -1);

  for (int i = 0; tokens[i]; i++) {
    char *tok = g_strstrip (tokens[i]);
    GtkWidget *el = NULL;
    if (g_strcmp0 (tok, "cover") == 0 && zlefsdc_settings_get_bool (self->settings, "show.cover"))
      el = make_cover (self);
    else if (g_strcmp0 (tok, "icon") == 0 && zlefsdc_settings_get_bool (self->settings, "show.icon"))
      el = make_icon (self);
    else if (g_strcmp0 (tok, "info") == 0 &&
             (zlefsdc_settings_get_bool (self->settings, "show.title") ||
              zlefsdc_settings_get_bool (self->settings, "show.artist") ||
              zlefsdc_settings_get_bool (self->settings, "show.album")))
      el = make_info (self);
    else if (g_strcmp0 (tok, "prev") == 0 && zlefsdc_settings_get_bool (self->settings, "show.prev"))
      el = self->btn_prev = make_button (self, "media-skip-backward", G_CALLBACK (on_btn_prev));
    else if (g_strcmp0 (tok, "playpause") == 0 && zlefsdc_settings_get_bool (self->settings, "show.playpause")) {
      el = self->btn_pp = make_button (self, "media-playback-start", G_CALLBACK (on_btn_pp));
      self->pp_img = gtk_button_get_image (GTK_BUTTON (self->btn_pp));  /* toggled in update_state */
    }
    else if (g_strcmp0 (tok, "next") == 0 && zlefsdc_settings_get_bool (self->settings, "show.next"))
      el = self->btn_next = make_button (self, "media-skip-forward", G_CALLBACK (on_btn_next));
    else if (g_strcmp0 (tok, "progress") == 0 && zlefsdc_settings_get_bool (self->settings, "show.progress"))
      el = make_progress (self);

    if (el) {
      gboolean expand = (g_strcmp0 (tok, "info") == 0 || g_strcmp0 (tok, "progress") == 0);
      gtk_box_pack_start (GTK_BOX (self->box), el, expand, expand, 0);
    }
  }
  g_strfreev (tokens);

  gtk_widget_show_all (self->box);
  apply_style (self);
  ensure_marquee (self);
  update_state (self);
}

static void apply_style (ZlefsdcWidget *self) {
  const char *font = zlefsdc_settings_get_string (self->settings, "text.font");
  const char *color = zlefsdc_settings_get_string (self->settings, "text.color");

  GString *css = g_string_new (NULL);
  g_string_append (css, ".zl-progress { min-height: 3px; min-width: 3px; }\n");
  g_string_append (css, ".zl-artist, .zl-album { opacity: 0.75; font-size: smaller; }\n");
  if (font && *font) {
    PangoFontDescription *d = pango_font_description_from_string (font);
    const char *fam = pango_font_description_get_family (d);
    int sz = pango_font_description_get_size (d) / PANGO_SCALE;
    g_string_append (css, ".zl-title, .zl-artist, .zl-album {");
    if (fam) g_string_append_printf (css, " font-family: \"%s\";", fam);
    if (sz > 0) g_string_append_printf (css, " font-size: %dpt;", sz);
    g_string_append (css, " }\n");
    pango_font_description_free (d);
  }
  if (color && *color)
    g_string_append_printf (css, ".zl-title, .zl-artist, .zl-album { color: %s; }\n", color);

  gtk_css_provider_load_from_data (self->css, css->str, -1, NULL);
  g_string_free (css, TRUE);
}

static void update_state (ZlefsdcWidget *self) {
  gboolean has = zlefsdc_player_has_player (self->player);
  ZlefsdcPlayback pb = zlefsdc_player_get_playback (self->player);
  gboolean idle = !has || pb == ZLEFSDC_PLAYBACK_STOPPED;

  if (zlefsdc_settings_get_bool (self->settings, "behavior.hide_when_idle") && idle) {
    if (self->box) gtk_widget_hide (self->box);
    ensure_progress_timer (self);
    return;
  }
  if (self->box) gtk_widget_show (self->box);

  const char *t = zlefsdc_player_get_title (self->player);
  const char *a = zlefsdc_player_get_artist (self->player);
  const char *b = zlefsdc_player_get_album (self->player);
  const char *placeholder = zlefsdc_settings_get_string (self->settings, "text.placeholder");

  if (self->title_lbl) {
    char *s = (has && *t) ? format_text (zlefsdc_settings_get_string (self->settings, "text.title_format"), t, a, b)
                          : g_strdup (placeholder);
    set_label (self, self->title_lbl, s); g_free (s);
  }
  if (self->artist_lbl) {
    char *s = (has && (*a || *t)) ? format_text (zlefsdc_settings_get_string (self->settings, "text.artist_format"), t, a, b)
                                  : g_strdup ("");
    set_label (self, self->artist_lbl, s); g_free (s);
  }
  if (self->album_lbl) set_label (self, self->album_lbl, has ? b : "");

  /* play/pause icon reflects state */
  if (self->pp_img) {
    gboolean symbolic = zlefsdc_settings_get_bool (self->settings, "buttons.symbolic");
    const char *base = (pb == ZLEFSDC_PLAYBACK_PLAYING) ? "media-playback-pause" : "media-playback-start";
    gtk_image_set_from_icon_name (GTK_IMAGE (self->pp_img), btn_icon (symbolic, base), GTK_ICON_SIZE_BUTTON);
    gtk_image_set_pixel_size (GTK_IMAGE (self->pp_img), zlefsdc_settings_get_int (self->settings, "buttons.icon_size"));
  }
  if (self->btn_next) gtk_widget_set_sensitive (self->btn_next, zlefsdc_player_can_next (self->player));
  if (self->btn_prev) gtk_widget_set_sensitive (self->btn_prev, zlefsdc_player_can_prev (self->player));

  /* app icon */
  if (self->icon) {
    const char *ic = zlefsdc_player_get_icon_name (self->player);
    if (!ic || !*ic) ic = "multimedia-player";
    gtk_image_set_from_icon_name (GTK_IMAGE (self->icon), ic, GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_image_set_pixel_size (GTK_IMAGE (self->icon),
        CLAMP (zlefsdc_settings_get_int (self->settings, "buttons.icon_size") + 4, 12, 64));
  }

  /* tooltip */
  if (zlefsdc_settings_get_bool (self->settings, "behavior.tooltip") && has && *t) {
    char *tip = g_strdup_printf ("%s\n%s%s%s", t, a, (b && *b) ? "\n" : "", b ?: "");
    gtk_widget_set_tooltip_text (GTK_WIDGET (self), tip);
    g_free (tip);
  } else {
    gtk_widget_set_tooltip_text (GTK_WIDGET (self), NULL);
  }

  request_cover (self);
  ensure_progress_timer (self);
}

/* ===================== signal glue ===================================== */

static void on_settings_changed (ZlefsdcSettings *s, gpointer data) {
  (void) s;
  ZlefsdcWidget *self = data;
  zlefsdc_player_set_target (self->player, zlefsdc_settings_get_string (self->settings, "player.target"));
  zlefsdc_player_set_artist_separator (self->player, zlefsdc_settings_get_string (self->settings, "text.artist_sep"));
  rebuild (self);
}

static void on_player_changed (ZlefsdcPlayer *p, gpointer data) {
  (void) p;
  update_state ((ZlefsdcWidget *) data);
}

/* ===================== public api ====================================== */

void zlefsdc_widget_set_orientation (ZlefsdcWidget *self, GtkOrientation o) {
  if (self->orientation == o && self->box) return;
  self->orientation = o;
  rebuild (self);
}

void zlefsdc_widget_set_panel_size (ZlefsdcWidget *self, int thickness_px) {
  if (self->panel_size == thickness_px) return;
  self->panel_size = thickness_px;
  if (self->cover) {
    self->cover_px = compute_cover_px (self);
    gtk_widget_set_size_request (self->cover, self->cover_px, self->cover_px);
    request_cover (self);
  }
}

ZlefsdcSettings *zlefsdc_widget_get_settings (ZlefsdcWidget *self) { return self->settings; }
ZlefsdcPlayer   *zlefsdc_widget_get_player   (ZlefsdcWidget *self) { return self->player; }

ZlefsdcWidget *zlefsdc_widget_new (ZlefsdcSettings *settings) {
  ZlefsdcWidget *self = g_object_new (ZLEFSDC_TYPE_WIDGET, NULL);
  self->settings = g_object_ref (settings);

  zlefsdc_player_set_target (self->player, zlefsdc_settings_get_string (settings, "player.target"));
  zlefsdc_player_set_artist_separator (self->player, zlefsdc_settings_get_string (settings, "text.artist_sep"));

  g_signal_connect (self->settings, "changed", G_CALLBACK (on_settings_changed), self);
  g_signal_connect (self->player, "changed", G_CALLBACK (on_player_changed), self);

  rebuild (self);
  return self;
}

/* ===================== gobject ========================================= */

static void zlefsdc_widget_dispose (GObject *o) {
  ZlefsdcWidget *self = ZLEFSDC_WIDGET (o);
  if (self->marquee_id)  { g_source_remove (self->marquee_id);  self->marquee_id = 0; }
  if (self->progress_id) { g_source_remove (self->progress_id); self->progress_id = 0; }
  g_clear_object (&self->cover_pix);
  g_clear_object (&self->cover_loader);
  if (self->css && gdk_screen_get_default ())
    gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (self->css));
  if (self->player)   { g_signal_handlers_disconnect_by_data (self->player, self);   g_clear_object (&self->player); }
  if (self->settings) { g_signal_handlers_disconnect_by_data (self->settings, self); g_clear_object (&self->settings); }
  g_clear_object (&self->css);
  G_OBJECT_CLASS (zlefsdc_widget_parent_class)->dispose (o);
}

static void zlefsdc_widget_class_init (ZlefsdcWidgetClass *klass) {
  G_OBJECT_CLASS (klass)->dispose = zlefsdc_widget_dispose;
}

static void zlefsdc_widget_init (ZlefsdcWidget *self) {
  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  self->panel_size = 0;
  self->player = zlefsdc_player_new ();
  self->cover_loader = zlefsdc_cover_new ();
  self->css = gtk_css_provider_new ();

  gtk_event_box_set_visible_window (GTK_EVENT_BOX (self), FALSE);
  gtk_widget_add_events (GTK_WIDGET (self), GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
  g_signal_connect (self, "scroll-event", G_CALLBACK (on_scroll), self);

  gtk_style_context_add_provider_for_screen (
      gdk_screen_get_default (),
      GTK_STYLE_PROVIDER (self->css),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
