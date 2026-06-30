#include "zlefsdc-player.h"
#include <gio/gio.h>
#include <string.h>

#define MPRIS_PREFIX  "org.mpris.MediaPlayer2."
#define MPRIS_NS      "org.mpris.MediaPlayer2"   /* arg0namespace: NO trailing dot */
#define MPRIS_PATH    "/org/mpris/MediaPlayer2"
#define IFACE_ROOT    "org.mpris.MediaPlayer2"
#define IFACE_PLAYER  "org.mpris.MediaPlayer2.Player"

struct _ZlefsdcPlayer {
  GObject       parent;

  GDBusConnection *bus;
  char          *target;        /* "auto" | "spotify" | full name */
  char          *artist_sep;

  char          *bus_name;      /* currently bound name, or NULL */
  GDBusProxy    *root;          /* IFACE_ROOT   */
  GDBusProxy    *player;        /* IFACE_PLAYER */
  guint          name_watch;    /* NameOwnerChanged subscription */
  guint          retry_id;      /* re-resolve poll while unbound (startup race) */

  /* snapshot */
  ZlefsdcPlayback playback;
  char           *title, *artist, *album, *art_url, *app_name, *icon_name;
  gboolean        can_next, can_prev;
  gint64          length_us;
};

G_DEFINE_FINAL_TYPE (ZlefsdcPlayer, zlefsdc_player, G_TYPE_OBJECT)

enum { SIG_CHANGED, SIG_APPEARED, SIG_VANISHED, N_SIGNALS };
static guint signals[N_SIGNALS];

static void rebind (ZlefsdcPlayer *self);

/* --- snapshot helpers ---------------------------------------------------- */

static gboolean set_str (char **slot, const char *v) {
  if (g_strcmp0 (*slot, v) == 0) return FALSE;
  g_free (*slot); *slot = g_strdup (v); return TRUE;
}

static void clear_snapshot (ZlefsdcPlayer *self) {
  self->playback = ZLEFSDC_PLAYBACK_STOPPED;
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->artist, g_free);
  g_clear_pointer (&self->album, g_free);
  g_clear_pointer (&self->art_url, g_free);
  g_clear_pointer (&self->app_name, g_free);
  g_clear_pointer (&self->icon_name, g_free);
  self->can_next = self->can_prev = FALSE;
  self->length_us = 0;
}

/* MPRIS is loose about integer signedness: mpris:length and Position come back
 * as int64 (x) from some players and uint64 (t) from others (Spotify). Coerce
 * whatever numeric type we get to a gint64 of microseconds. */
static gint64 as_int64 (GVariant *v) {
  if (!v) return 0;
  if (g_variant_is_of_type (v, G_VARIANT_TYPE_INT64))  return g_variant_get_int64 (v);
  if (g_variant_is_of_type (v, G_VARIANT_TYPE_UINT64)) return (gint64) g_variant_get_uint64 (v);
  if (g_variant_is_of_type (v, G_VARIANT_TYPE_INT32))  return g_variant_get_int32 (v);
  if (g_variant_is_of_type (v, G_VARIANT_TYPE_UINT32)) return (gint64) g_variant_get_uint32 (v);
  if (g_variant_is_of_type (v, G_VARIANT_TYPE_DOUBLE)) return (gint64) g_variant_get_double (v);
  return 0;
}

/* Pull the Metadata a{sv} + scalar props off the player proxy into the snapshot.
 * Returns TRUE if anything user-visible changed. */
static gboolean refresh_from_proxies (ZlefsdcPlayer *self) {
  gboolean changed = FALSE;
  if (!self->player) return FALSE;

  GVariant *status = g_dbus_proxy_get_cached_property (self->player, "PlaybackStatus");
  ZlefsdcPlayback pb = ZLEFSDC_PLAYBACK_STOPPED;
  if (status) {
    const char *s = g_variant_get_string (status, NULL);
    if (g_strcmp0 (s, "Playing") == 0) pb = ZLEFSDC_PLAYBACK_PLAYING;
    else if (g_strcmp0 (s, "Paused") == 0) pb = ZLEFSDC_PLAYBACK_PAUSED;
    g_variant_unref (status);
  }
  if (pb != self->playback) { self->playback = pb; changed = TRUE; }

  GVariant *cn = g_dbus_proxy_get_cached_property (self->player, "CanGoNext");
  if (cn) { gboolean b = g_variant_get_boolean (cn); if (b != self->can_next) { self->can_next = b; changed = TRUE; } g_variant_unref (cn); }
  GVariant *cp = g_dbus_proxy_get_cached_property (self->player, "CanGoPrevious");
  if (cp) { gboolean b = g_variant_get_boolean (cp); if (b != self->can_prev) { self->can_prev = b; changed = TRUE; } g_variant_unref (cp); }

  GVariant *meta = g_dbus_proxy_get_cached_property (self->player, "Metadata");
  const char *title = NULL, *album = NULL, *art = NULL;
  char *artists_joined = NULL;
  gint64 length = 0;
  if (meta) {
    g_variant_lookup (meta, "xesam:title", "&s", &title);
    g_variant_lookup (meta, "xesam:album", "&s", &album);
    g_variant_lookup (meta, "mpris:artUrl", "&s", &art);
    GVariant *lv = g_variant_lookup_value (meta, "mpris:length", NULL); /* x or t */
    if (lv) { length = as_int64 (lv); g_variant_unref (lv); }

    GVariant *artv = g_variant_lookup_value (meta, "xesam:artist", G_VARIANT_TYPE_STRING_ARRAY);
    if (!artv) artv = g_variant_lookup_value (meta, "xesam:albumArtist", G_VARIANT_TYPE_STRING_ARRAY);
    if (artv) {
      GPtrArray *parts = g_ptr_array_new ();
      GVariantIter it; const char *a;
      g_variant_iter_init (&it, artv);
      while (g_variant_iter_next (&it, "&s", &a))
        if (a && *a) g_ptr_array_add (parts, (gpointer) a);
      g_ptr_array_add (parts, NULL);
      artists_joined = g_strjoinv (self->artist_sep ?: ", ", (char **) parts->pdata);
      g_ptr_array_free (parts, TRUE);
      g_variant_unref (artv);
    }
  }
  if (set_str (&self->title, title)) changed = TRUE;
  if (set_str (&self->album, album)) changed = TRUE;
  if (set_str (&self->art_url, art)) changed = TRUE;
  if (set_str (&self->artist, artists_joined)) changed = TRUE;
  if (length != self->length_us) { self->length_us = length; changed = TRUE; }
  g_free (artists_joined);
  if (meta) g_variant_unref (meta);

  /* identity / desktop entry off the root proxy */
  if (self->root) {
    GVariant *id = g_dbus_proxy_get_cached_property (self->root, "Identity");
    if (id) { if (set_str (&self->app_name, g_variant_get_string (id, NULL))) changed = TRUE; g_variant_unref (id); }
    GVariant *de = g_dbus_proxy_get_cached_property (self->root, "DesktopEntry");
    if (de) { if (set_str (&self->icon_name, g_variant_get_string (de, NULL))) changed = TRUE; g_variant_unref (de); }
  }

  return changed;
}

static void on_player_props (GDBusProxy *proxy, GVariant *changed_props,
                             GStrv invalidated, gpointer user_data) {
  (void) proxy; (void) changed_props; (void) invalidated;
  ZlefsdcPlayer *self = user_data;
  if (refresh_from_proxies (self))
    g_signal_emit (self, signals[SIG_CHANGED], 0);
}

/* --- target resolution --------------------------------------------------- */

static gboolean name_matches_target (const char *name, const char *target) {
  if (!g_str_has_prefix (name, MPRIS_PREFIX)) return FALSE;
  if (!target || !*target || g_strcmp0 (target, "auto") == 0) return TRUE;
  if (g_strcmp0 (name, target) == 0) return TRUE;
  const char *suffix = name + strlen (MPRIS_PREFIX);
  /* match "spotify" against org.mpris.MediaPlayer2.spotify and ...spotify.instance123 */
  return g_str_has_prefix (suffix, target);
}

/* Pick the best currently-owned bus name for our target, or NULL. */
static char *resolve_name (ZlefsdcPlayer *self) {
  if (!self->bus) return NULL;
  GVariant *reply = g_dbus_connection_call_sync (
      self->bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "ListNames", NULL, G_VARIANT_TYPE ("(as)"),
      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
  if (!reply) return NULL;

  char *found = NULL;
  GVariantIter *it; const char *name;
  g_variant_get (reply, "(as)", &it);
  while (g_variant_iter_next (it, "&s", &name)) {
    if (name_matches_target (name, self->target)) {
      /* prefer an exact target match over a fuzzy/auto one */
      if (g_strcmp0 (name, self->target) == 0) { g_free (found); found = g_strdup (name); break; }
      if (!found) found = g_strdup (name);
    }
  }
  g_variant_iter_free (it);
  g_variant_unref (reply);
  return found;
}

static void unbind (ZlefsdcPlayer *self) {
  g_clear_object (&self->player);
  g_clear_object (&self->root);
  g_clear_pointer (&self->bus_name, g_free);
  clear_snapshot (self);
}

/* While no player is bound, re-resolve periodically: the panel often starts
 * before the media player (login race) and a missed NameOwnerChanged would
 * otherwise leave us blank until the user toggles the target. */
static gboolean on_retry (gpointer data) {
  ZlefsdcPlayer *self = data;
  rebind (self);
  if (self->bus_name) { self->retry_id = 0; return G_SOURCE_REMOVE; }
  return G_SOURCE_CONTINUE;
}
static void ensure_retry (ZlefsdcPlayer *self) {
  if (!self->bus_name && !self->retry_id)
    self->retry_id = g_timeout_add_seconds (2, on_retry, self);
}

/* (Re)connect proxies to whichever name resolves now. */
static void rebind (ZlefsdcPlayer *self) {
  char *name = resolve_name (self);

  if (g_strcmp0 (name, self->bus_name) == 0) { g_free (name); return; }

  gboolean had = (self->bus_name != NULL);
  unbind (self);

  if (name) {
    self->bus_name = name;
    self->root = g_dbus_proxy_new_sync (
        self->bus, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START, NULL,
        name, MPRIS_PATH, IFACE_ROOT, NULL, NULL);
    self->player = g_dbus_proxy_new_sync (
        self->bus, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START, NULL,
        name, MPRIS_PATH, IFACE_PLAYER, NULL, NULL);
    if (self->player)
      g_signal_connect (self->player, "g-properties-changed",
                        G_CALLBACK (on_player_props), self);
    refresh_from_proxies (self);
    g_signal_emit (self, signals[SIG_APPEARED], 0);
  } else if (had) {
    g_signal_emit (self, signals[SIG_VANISHED], 0);
  }
  ensure_retry (self);          /* keep looking if still unbound */
  g_signal_emit (self, signals[SIG_CHANGED], 0);
}

static void on_name_owner_changed (GDBusConnection *conn, const char *sender,
                                   const char *path, const char *iface,
                                   const char *signal, GVariant *params,
                                   gpointer user_data) {
  (void) conn; (void) sender; (void) path; (void) iface; (void) signal; (void) params;
  rebind (user_data);
}

/* --- public api ---------------------------------------------------------- */

void zlefsdc_player_set_target (ZlefsdcPlayer *self, const char *target) {
  if (set_str (&self->target, target && *target ? target : "auto"))
    rebind (self);
}

void zlefsdc_player_set_artist_separator (ZlefsdcPlayer *self, const char *sep) {
  if (set_str (&self->artist_sep, sep && *sep ? sep : ", "))
    if (refresh_from_proxies (self))
      g_signal_emit (self, signals[SIG_CHANGED], 0);
}

gboolean         zlefsdc_player_has_player   (ZlefsdcPlayer *s) { return s->player != NULL; }
ZlefsdcPlayback  zlefsdc_player_get_playback (ZlefsdcPlayer *s) { return s->playback; }
const char      *zlefsdc_player_get_title    (ZlefsdcPlayer *s) { return s->title ?: ""; }
const char      *zlefsdc_player_get_artist   (ZlefsdcPlayer *s) { return s->artist ?: ""; }
const char      *zlefsdc_player_get_album    (ZlefsdcPlayer *s) { return s->album ?: ""; }
const char      *zlefsdc_player_get_art_url  (ZlefsdcPlayer *s) { return s->art_url ?: ""; }
const char      *zlefsdc_player_get_app_name (ZlefsdcPlayer *s) { return s->app_name ?: ""; }
const char      *zlefsdc_player_get_icon_name(ZlefsdcPlayer *s) { return s->icon_name ?: ""; }
gboolean         zlefsdc_player_can_next     (ZlefsdcPlayer *s) { return s->can_next; }
gboolean         zlefsdc_player_can_prev     (ZlefsdcPlayer *s) { return s->can_prev; }
gint64           zlefsdc_player_get_length_us(ZlefsdcPlayer *s) { return s->length_us; }

gint64 zlefsdc_player_get_position_us (ZlefsdcPlayer *self) {
  if (!self->player || !self->bus_name) return 0;
  /* Real position straight from the player. Position is not in PropertiesChanged
   * (so the proxy cache is stale) — read it live each time. Accept int64/uint64. */
  GVariant *v = NULL;
  GVariant *r = g_dbus_connection_call_sync (
      self->bus, self->bus_name, MPRIS_PATH, "org.freedesktop.DBus.Properties",
      "Get", g_variant_new ("(ss)", IFACE_PLAYER, "Position"),
      G_VARIANT_TYPE ("(v)"), G_DBUS_CALL_FLAGS_NONE, 400, NULL, NULL);
  if (r) { g_variant_get (r, "(v)", &v); g_variant_unref (r); }
  gint64 pos = as_int64 (v);
  if (v) g_variant_unref (v);
  if (pos < 0) pos = 0;
  if (self->length_us > 0 && pos > self->length_us) pos = self->length_us;
  return pos;
}

static void call_player (ZlefsdcPlayer *self, const char *method) {
  if (self->player)
    g_dbus_proxy_call (self->player, method, NULL, G_DBUS_CALL_FLAGS_NONE,
                       -1, NULL, NULL, NULL);
}
void zlefsdc_player_play_pause (ZlefsdcPlayer *s) { call_player (s, "PlayPause"); }
void zlefsdc_player_next       (ZlefsdcPlayer *s) { if (s->can_next) call_player (s, "Next"); }
void zlefsdc_player_previous   (ZlefsdcPlayer *s) { if (s->can_prev) call_player (s, "Previous"); }
void zlefsdc_player_stop       (ZlefsdcPlayer *s) { call_player (s, "Stop"); }
void zlefsdc_player_raise (ZlefsdcPlayer *self) {
  if (self->root)
    g_dbus_proxy_call (self->root, "Raise", NULL, G_DBUS_CALL_FLAGS_NONE,
                       -1, NULL, NULL, NULL);
}

/* --- gobject ------------------------------------------------------------- */

ZlefsdcPlayer *zlefsdc_player_new (void) {
  return g_object_new (ZLEFSDC_TYPE_PLAYER, NULL);
}

static void zlefsdc_player_constructed (GObject *o) {
  ZlefsdcPlayer *self = ZLEFSDC_PLAYER (o);
  self->bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (self->bus) {
    self->name_watch = g_dbus_connection_signal_subscribe (
        self->bus, "org.freedesktop.DBus", "org.freedesktop.DBus",
        "NameOwnerChanged", "/org/freedesktop/DBus", MPRIS_NS,
        G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE,
        on_name_owner_changed, self, NULL);
    rebind (self);
  }
  G_OBJECT_CLASS (zlefsdc_player_parent_class)->constructed (o);
}

static void zlefsdc_player_finalize (GObject *o) {
  ZlefsdcPlayer *self = ZLEFSDC_PLAYER (o);
  if (self->retry_id) { g_source_remove (self->retry_id); self->retry_id = 0; }
  if (self->bus && self->name_watch)
    g_dbus_connection_signal_unsubscribe (self->bus, self->name_watch);
  unbind (self);
  g_clear_object (&self->bus);
  g_free (self->target);
  g_free (self->artist_sep);
  G_OBJECT_CLASS (zlefsdc_player_parent_class)->finalize (o);
}

static void zlefsdc_player_class_init (ZlefsdcPlayerClass *klass) {
  GObjectClass *oc = G_OBJECT_CLASS (klass);
  oc->constructed = zlefsdc_player_constructed;
  oc->finalize = zlefsdc_player_finalize;
  signals[SIG_CHANGED]  = g_signal_new ("changed",  G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  signals[SIG_APPEARED] = g_signal_new ("appeared", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  signals[SIG_VANISHED] = g_signal_new ("vanished", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void zlefsdc_player_init (ZlefsdcPlayer *self) {
  self->target = g_strdup ("spotify");
  self->artist_sep = g_strdup (", ");
  self->playback = ZLEFSDC_PLAYBACK_STOPPED;
}
