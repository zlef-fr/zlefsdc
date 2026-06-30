#include "zlefsdc-prefs.h"
#include <glib/gi18n.h>

/* Each control writes back to the model then persists it; the bound widget
 * re-renders from the settings "changed" signal, giving a live preview. */

static void persist (ZlefsdcSettings *s) { zlefsdc_settings_save (s, NULL); }

/* ---- bound control builders -------------------------------------------- */

static void on_switch (GtkSwitch *sw, GParamSpec *p, gpointer data) {
  (void) p;
  ZlefsdcSettings *s = g_object_get_data (G_OBJECT (sw), "zl-settings");
  const char *key = data;
  zlefsdc_settings_set_bool (s, key, gtk_switch_get_active (sw));
  persist (s);
}
static GtkWidget *bool_row (ZlefsdcSettings *s, const char *key) {
  GtkWidget *sw = gtk_switch_new ();
  gtk_widget_set_halign (sw, GTK_ALIGN_START);
  gtk_switch_set_active (GTK_SWITCH (sw), zlefsdc_settings_get_bool (s, key));
  g_object_set_data (G_OBJECT (sw), "zl-settings", s);
  g_signal_connect (sw, "notify::active", G_CALLBACK (on_switch), (gpointer) key);
  return sw;
}

static void on_spin (GtkSpinButton *sp, gpointer data) {
  ZlefsdcSettings *s = g_object_get_data (G_OBJECT (sp), "zl-settings");
  zlefsdc_settings_set_int (s, data, gtk_spin_button_get_value_as_int (sp));
  persist (s);
}
static GtkWidget *int_row (ZlefsdcSettings *s, const char *key, int lo, int hi) {
  GtkWidget *sp = gtk_spin_button_new_with_range (lo, hi, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (sp), zlefsdc_settings_get_int (s, key));
  g_object_set_data (G_OBJECT (sp), "zl-settings", s);
  g_signal_connect (sp, "value-changed", G_CALLBACK (on_spin), (gpointer) key);
  return sp;
}

static void on_entry (GtkEditable *e, gpointer data) {
  ZlefsdcSettings *s = g_object_get_data (G_OBJECT (e), "zl-settings");
  zlefsdc_settings_set_string (s, data, gtk_entry_get_text (GTK_ENTRY (e)));
  persist (s);
}
static GtkWidget *str_row (ZlefsdcSettings *s, const char *key) {
  GtkWidget *e = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (e), zlefsdc_settings_get_string (s, key));
  gtk_widget_set_hexpand (e, TRUE);
  g_object_set_data (G_OBJECT (e), "zl-settings", s);
  g_signal_connect (e, "changed", G_CALLBACK (on_entry), (gpointer) key);
  return e;
}

/* action combo ---------------------------------------------------------- */
static const struct { ZlefsdcAction a; const char *label; } ACTIONS[] = {
  { ZLEFSDC_ACTION_NONE,      N_("Nothing") },
  { ZLEFSDC_ACTION_PLAYPAUSE, N_("Play / Pause") },
  { ZLEFSDC_ACTION_NEXT,      N_("Next track") },
  { ZLEFSDC_ACTION_PREVIOUS,  N_("Previous track") },
  { ZLEFSDC_ACTION_STOP,      N_("Stop") },
  { ZLEFSDC_ACTION_RAISE,     N_("Raise player window") },
  { ZLEFSDC_ACTION_COMMAND,   N_("Run custom command") },
};
static void on_action_combo (GtkComboBox *c, gpointer data) {
  ZlefsdcSettings *s = g_object_get_data (G_OBJECT (c), "zl-settings");
  const char *id = gtk_combo_box_get_active_id (c);
  if (id) { zlefsdc_settings_set_string (s, data, id); persist (s); }
}
static GtkWidget *action_row (ZlefsdcSettings *s, const char *key) {
  GtkWidget *c = gtk_combo_box_text_new ();
  for (guint i = 0; i < G_N_ELEMENTS (ACTIONS); i++)
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (c),
        zlefsdc_action_nick (ACTIONS[i].a), _(ACTIONS[i].label));
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (c), zlefsdc_settings_get_string (s, key));
  g_object_set_data (G_OBJECT (c), "zl-settings", s);
  g_signal_connect (c, "changed", G_CALLBACK (on_action_combo), (gpointer) key);
  return c;
}

/* ---- grid plumbing ----------------------------------------------------- */

static GtkWidget *new_page (GtkWidget *notebook, const char *title) {
  GtkWidget *grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 8);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 14);
  g_object_set (grid, "margin", 14, NULL);
  GtkWidget *sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (sw), grid);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sw, gtk_label_new (title));
  g_object_set_data (G_OBJECT (grid), "row", GINT_TO_POINTER (0));
  return grid;
}
static int next_row (GtkWidget *grid) {
  int r = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (grid), "row"));
  g_object_set_data (G_OBJECT (grid), "row", GINT_TO_POINTER (r + 1));
  return r;
}
static void add_row (GtkWidget *grid, const char *label, GtkWidget *control) {
  int r = next_row (grid);
  GtkWidget *l = gtk_label_new (label);
  gtk_label_set_xalign (GTK_LABEL (l), 0.0);
  gtk_grid_attach (GTK_GRID (grid), l, 0, r, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), control, 1, r, 1, 1);
}
static void add_section (GtkWidget *grid, const char *title) {
  int r = next_row (grid);
  GtkWidget *l = gtk_label_new (NULL);
  char *m = g_markup_printf_escaped ("<b>%s</b>", title);
  gtk_label_set_markup (GTK_LABEL (l), m);
  g_free (m);
  gtk_label_set_xalign (GTK_LABEL (l), 0.0);
  gtk_widget_set_margin_top (l, r ? 10 : 0);
  gtk_grid_attach (GTK_GRID (grid), l, 0, r, 2, 1);
}

/* ---- public ------------------------------------------------------------ */

GtkWidget *zlefsdc_prefs_new (ZlefsdcSettings *settings) {
  GtkWidget *nb = gtk_notebook_new ();
  gtk_widget_set_size_request (nb, 460, 420);

  /* Elements */
  GtkWidget *p = new_page (nb, _("Elements"));
  add_section (p, _("Show"));
  add_row (p, _("Album cover"),    bool_row (settings, "show.cover"));
  add_row (p, _("App icon"),       bool_row (settings, "show.icon"));
  add_row (p, _("Title"),          bool_row (settings, "show.title"));
  add_row (p, _("Artist"),         bool_row (settings, "show.artist"));
  add_row (p, _("Album"),          bool_row (settings, "show.album"));
  add_row (p, _("Previous button"),bool_row (settings, "show.prev"));
  add_row (p, _("Play/Pause button"), bool_row (settings, "show.playpause"));
  add_row (p, _("Next button"),    bool_row (settings, "show.next"));
  add_row (p, _("Progress bar"),   bool_row (settings, "show.progress"));

  /* Layout */
  p = new_page (nb, _("Layout"));
  add_section (p, _("Arrangement"));
  add_row (p, _("Order — “,” = same line, “[ ]” = new row/column:\ne.g. cover, [ info, [ prev, playpause, next ], progress ]"),
           str_row (settings, "layout.order"));
  add_row (p, _("Spacing (px)"),     int_row (settings, "layout.spacing", 0, 40));
  add_row (p, _("Title & artist inline"), bool_row (settings, "layout.info_inline"));
  add_section (p, _("Cover"));
  add_row (p, _("Size (px, 0 = fit panel)"), int_row (settings, "cover.size", 0, 512));
  add_row (p, _("Corner radius (px)"), int_row (settings, "cover.radius", 0, 64));

  /* Text */
  p = new_page (nb, _("Text"));
  add_section (p, _("Format  (%t title · %a artist · %b album)"));
  add_row (p, _("Title format"),   str_row (settings, "text.title_format"));
  add_row (p, _("Artist format"),  str_row (settings, "text.artist_format"));
  add_row (p, _("Artist separator"), str_row (settings, "text.artist_sep"));
  add_row (p, _("Placeholder (idle)"), str_row (settings, "text.placeholder"));
  add_section (p, _("Style"));
  add_row (p, _("Max characters (0 = unlimited)"), int_row (settings, "text.max_chars", 0, 120));
  add_row (p, _("Scroll long text (marquee)"), bool_row (settings, "text.scroll"));
  add_row (p, _("Font (e.g. \"Sans Bold 10\")"), str_row (settings, "text.font"));
  add_row (p, _("Colour (CSS, blank = theme)"), str_row (settings, "text.color"));

  /* Buttons */
  p = new_page (nb, _("Buttons"));
  add_row (p, _("Icon size (px)"), int_row (settings, "buttons.icon_size", 8, 64));
  add_row (p, _("Symbolic icons"), bool_row (settings, "buttons.symbolic"));
  add_row (p, _("Flat (no frame)"), bool_row (settings, "buttons.flat"));

  /* Actions */
  p = new_page (nb, _("Actions"));
  add_section (p, _("Player"));
  add_row (p, _("Target (\"spotify\", \"auto\", or bus name)"), str_row (settings, "player.target"));
  add_section (p, _("Quick actions"));
  add_row (p, _("Click cover"),   action_row (settings, "action.cover_click"));
  add_row (p, _("Click title"),   action_row (settings, "action.info_click"));
  add_row (p, _("Middle click"),  action_row (settings, "action.middle_click"));
  add_row (p, _("Scroll up"),     action_row (settings, "action.scroll_up"));
  add_row (p, _("Scroll down"),   action_row (settings, "action.scroll_down"));
  add_row (p, _("Custom command"),str_row (settings, "action.command"));
  add_section (p, _("Behaviour"));
  add_row (p, _("Hide when nothing plays"), bool_row (settings, "behavior.hide_when_idle"));
  add_row (p, _("Show tooltip"),  bool_row (settings, "behavior.tooltip"));

  gtk_widget_show_all (nb);
  return nb;
}
