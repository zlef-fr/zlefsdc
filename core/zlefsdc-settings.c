#include "zlefsdc-settings.h"
#include <gio/gio.h>
#include <string.h>

/* The complete, documented key space. Adding a permissive option = one row
 * here; load/save/reset all iterate this table, so nothing else needs touching.
 */
typedef enum { T_BOOL, T_INT, T_STR } SpecType;
typedef struct {
  const char *key;
  SpecType    type;
  gboolean    bdef;
  int         idef;
  const char *sdef;
} Spec;

static const Spec SPECS[] = {
  /* --- target player ----------------------------------------------------- */
  /* "auto" = first MPRIS player found; else a bus suffix ("spotify") or full
     "org.mpris.MediaPlayer2.foo" name. */
  { "player.target",        T_STR,  0, 0, "spotify" },

  /* --- which elements show ---------------------------------------------- */
  { "show.cover",           T_BOOL, 1, 0, NULL },
  { "show.icon",            T_BOOL, 0, 0, NULL },
  { "show.title",           T_BOOL, 1, 0, NULL },
  { "show.artist",          T_BOOL, 1, 0, NULL },
  { "show.album",           T_BOOL, 0, 0, NULL },
  { "show.prev",            T_BOOL, 1, 0, NULL },
  { "show.playpause",       T_BOOL, 1, 0, NULL },
  { "show.next",            T_BOOL, 1, 0, NULL },
  { "show.progress",        T_BOOL, 0, 0, NULL },

  /* --- layout ------------------------------------------------------------ */
  /* render order; any subset/permutation of the element tokens */
  { "layout.order",         T_STR,  0, 0, "cover,icon,info,prev,playpause,next,progress" },
  { "layout.spacing",       T_INT,  0, 4, NULL },     /* px between elements   */
  { "layout.info_inline",   T_BOOL, 0, 0, NULL },     /* title & artist one row*/

  /* --- cover ------------------------------------------------------------- */
  { "cover.size",           T_INT,  0, 0, NULL },     /* px; 0 = fit panel     */
  { "cover.radius",         T_INT,  0, 4, NULL },     /* corner radius px      */

  /* --- text -------------------------------------------------------------- */
  { "text.title_format",    T_STR,  0, 0, "%t" },     /* %t %a %b              */
  { "text.artist_format",   T_STR,  0, 0, "%a" },
  { "text.artist_sep",      T_STR,  0, 0, ", " },     /* joins multiple artists*/
  { "text.max_chars",       T_INT,  0, 24, NULL },    /* ellipsize width; 0=off*/
  { "text.scroll",          T_BOOL, 0, 0, NULL },     /* marquee on overflow   */
  { "text.font",            T_STR,  0, 0, "" },       /* pango desc; ""=inherit*/
  { "text.color",           T_STR,  0, 0, "" },       /* css color; ""=theme   */
  { "text.placeholder",     T_STR,  0, 0, "—" },      /* shown when idle       */

  /* --- buttons ----------------------------------------------------------- */
  { "buttons.icon_size",    T_INT,  0, 16, NULL },
  { "buttons.symbolic",     T_BOOL, 1, 0, NULL },
  { "buttons.flat",         T_BOOL, 1, 0, NULL },

  /* --- behaviour --------------------------------------------------------- */
  { "behavior.hide_when_idle", T_BOOL, 0, 0, NULL },
  { "behavior.tooltip",     T_BOOL, 1, 0, NULL },

  /* --- quick actions (values are action nicks) -------------------------- */
  { "action.cover_click",   T_STR,  0, 0, "raise" },
  { "action.info_click",    T_STR,  0, 0, "playpause" },
  { "action.middle_click",  T_STR,  0, 0, "playpause" },
  { "action.scroll_up",     T_STR,  0, 0, "next" },
  { "action.scroll_down",   T_STR,  0, 0, "previous" },
  { "action.command",       T_STR,  0, 0, "" },       /* for ACTION_COMMAND    */
};
#define N_SPECS (G_N_ELEMENTS (SPECS))

static const Spec *spec_for (const char *key) {
  for (guint i = 0; i < N_SPECS; i++)
    if (g_strcmp0 (SPECS[i].key, key) == 0)
      return &SPECS[i];
  return NULL;
}

/* keyfile group derived from the dotted prefix, e.g. "show.cover" -> "show" */
static void split_key (const char *key, char **group, const char **name) {
  const char *dot = strchr (key, '.');
  if (dot) { *group = g_strndup (key, dot - key); *name = dot + 1; }
  else     { *group = g_strdup ("general");       *name = key;     }
}

struct _ZlefsdcSettings {
  GObject     parent;
  char       *path;
  GHashTable *values;    /* key -> GValue* (boxed) */
  guint       freeze;
  gboolean    dirty_while_frozen;
};

G_DEFINE_FINAL_TYPE (ZlefsdcSettings, zlefsdc_settings, G_TYPE_OBJECT)

enum { SIG_CHANGED, N_SIGNALS };
static guint signals[N_SIGNALS];

static void free_gvalue (gpointer v) { g_value_unset (v); g_free (v); }

static void emit_changed (ZlefsdcSettings *self) {
  if (self->freeze) { self->dirty_while_frozen = TRUE; return; }
  g_signal_emit (self, signals[SIG_CHANGED], 0);
}

static void apply_defaults (ZlefsdcSettings *self) {
  g_hash_table_remove_all (self->values);
  for (guint i = 0; i < N_SPECS; i++) {
    const Spec *s = &SPECS[i];
    GValue *v = g_new0 (GValue, 1);
    switch (s->type) {
      case T_BOOL: g_value_init (v, G_TYPE_BOOLEAN); g_value_set_boolean (v, s->bdef); break;
      case T_INT:  g_value_init (v, G_TYPE_INT);     g_value_set_int     (v, s->idef); break;
      case T_STR:  g_value_init (v, G_TYPE_STRING);  g_value_set_string  (v, s->sdef); break;
    }
    g_hash_table_insert (self->values, g_strdup (s->key), v);
  }
}

static GValue *value_for (ZlefsdcSettings *self, const char *key, SpecType want) {
  GValue *v = g_hash_table_lookup (self->values, key);
  const Spec *s = spec_for (key);
  if (!v || !s) { g_warning ("zlefsdc: unknown setting '%s'", key); return NULL; }
  if (s->type != want) { g_warning ("zlefsdc: type mismatch for '%s'", key); return NULL; }
  return v;
}

/* --- accessors ----------------------------------------------------------- */

gboolean zlefsdc_settings_get_bool (ZlefsdcSettings *self, const char *key) {
  GValue *v = value_for (self, key, T_BOOL);
  return v ? g_value_get_boolean (v) : FALSE;
}
void zlefsdc_settings_set_bool (ZlefsdcSettings *self, const char *key, gboolean nv) {
  GValue *v = value_for (self, key, T_BOOL);
  if (!v || g_value_get_boolean (v) == nv) return;
  g_value_set_boolean (v, nv); emit_changed (self);
}
int zlefsdc_settings_get_int (ZlefsdcSettings *self, const char *key) {
  GValue *v = value_for (self, key, T_INT);
  return v ? g_value_get_int (v) : 0;
}
void zlefsdc_settings_set_int (ZlefsdcSettings *self, const char *key, int nv) {
  GValue *v = value_for (self, key, T_INT);
  if (!v || g_value_get_int (v) == nv) return;
  g_value_set_int (v, nv); emit_changed (self);
}
const char *zlefsdc_settings_get_string (ZlefsdcSettings *self, const char *key) {
  GValue *v = value_for (self, key, T_STR);
  return v ? g_value_get_string (v) : "";
}
void zlefsdc_settings_set_string (ZlefsdcSettings *self, const char *key, const char *nv) {
  GValue *v = value_for (self, key, T_STR);
  if (!v) return;
  if (g_strcmp0 (g_value_get_string (v), nv) == 0) return;
  g_value_set_string (v, nv ? nv : ""); emit_changed (self);
}

ZlefsdcAction zlefsdc_settings_get_action (ZlefsdcSettings *self, const char *key) {
  return zlefsdc_action_from_nick (zlefsdc_settings_get_string (self, key));
}

/* --- action nicks -------------------------------------------------------- */

static const char *ACTION_NICKS[ZLEFSDC_ACTION_N] = {
  [ZLEFSDC_ACTION_NONE]      = "none",
  [ZLEFSDC_ACTION_PLAYPAUSE] = "playpause",
  [ZLEFSDC_ACTION_NEXT]      = "next",
  [ZLEFSDC_ACTION_PREVIOUS]  = "previous",
  [ZLEFSDC_ACTION_STOP]      = "stop",
  [ZLEFSDC_ACTION_RAISE]     = "raise",
  [ZLEFSDC_ACTION_COMMAND]   = "command",
};
const char *zlefsdc_action_nick (ZlefsdcAction a) {
  return (a >= 0 && a < ZLEFSDC_ACTION_N) ? ACTION_NICKS[a] : "none";
}
ZlefsdcAction zlefsdc_action_from_nick (const char *nick) {
  if (nick)
    for (int i = 0; i < ZLEFSDC_ACTION_N; i++)
      if (g_strcmp0 (ACTION_NICKS[i], nick) == 0) return i;
  return ZLEFSDC_ACTION_NONE;
}

/* --- persistence --------------------------------------------------------- */

gboolean zlefsdc_settings_load (ZlefsdcSettings *self) {
  apply_defaults (self);
  if (!self->path) { emit_changed (self); return TRUE; }

  GKeyFile *kf = g_key_file_new ();
  GError *err = NULL;
  if (!g_key_file_load_from_file (kf, self->path, G_KEY_FILE_NONE, &err)) {
    /* missing file is fine: we keep defaults */
    g_clear_error (&err);
    g_key_file_free (kf);
    emit_changed (self);
    return TRUE;
  }
  for (guint i = 0; i < N_SPECS; i++) {
    const Spec *s = &SPECS[i];
    char *group; const char *name;
    split_key (s->key, &group, &name);
    if (g_key_file_has_key (kf, group, name, NULL)) {
      GValue *v = g_hash_table_lookup (self->values, s->key);
      switch (s->type) {
        case T_BOOL: g_value_set_boolean (v, g_key_file_get_boolean (kf, group, name, NULL)); break;
        case T_INT:  g_value_set_int     (v, g_key_file_get_integer (kf, group, name, NULL)); break;
        case T_STR: {
          char *str = g_key_file_get_string (kf, group, name, NULL);
          g_value_set_string (v, str ? str : s->sdef); g_free (str); break;
        }
      }
    }
    g_free (group);
  }
  g_key_file_free (kf);
  emit_changed (self);
  return TRUE;
}

gboolean zlefsdc_settings_save (ZlefsdcSettings *self, GError **error) {
  if (!self->path) return TRUE;
  GKeyFile *kf = g_key_file_new ();
  for (guint i = 0; i < N_SPECS; i++) {
    const Spec *s = &SPECS[i];
    char *group; const char *name;
    split_key (s->key, &group, &name);
    GValue *v = g_hash_table_lookup (self->values, s->key);
    switch (s->type) {
      case T_BOOL: g_key_file_set_boolean (kf, group, name, g_value_get_boolean (v)); break;
      case T_INT:  g_key_file_set_integer (kf, group, name, g_value_get_int (v));     break;
      case T_STR:  g_key_file_set_string  (kf, group, name, g_value_get_string (v) ?: ""); break;
    }
    g_free (group);
  }
  char *dir = g_path_get_dirname (self->path);
  g_mkdir_with_parents (dir, 0755);
  g_free (dir);
  gboolean ok = g_key_file_save_to_file (kf, self->path, error);
  g_key_file_free (kf);
  return ok;
}

void zlefsdc_settings_freeze (ZlefsdcSettings *self) { self->freeze++; }
void zlefsdc_settings_thaw (ZlefsdcSettings *self) {
  if (self->freeze && --self->freeze == 0 && self->dirty_while_frozen) {
    self->dirty_while_frozen = FALSE;
    g_signal_emit (self, signals[SIG_CHANGED], 0);
  }
}

/* --- gobject ------------------------------------------------------------- */

ZlefsdcSettings *zlefsdc_settings_new (const char *keyfile_path) {
  ZlefsdcSettings *self = g_object_new (ZLEFSDC_TYPE_SETTINGS, NULL);
  self->path = g_strdup (keyfile_path);
  zlefsdc_settings_load (self);
  return self;
}

static void zlefsdc_settings_finalize (GObject *o) {
  ZlefsdcSettings *self = ZLEFSDC_SETTINGS (o);
  g_clear_pointer (&self->values, g_hash_table_unref);
  g_clear_pointer (&self->path, g_free);
  G_OBJECT_CLASS (zlefsdc_settings_parent_class)->finalize (o);
}

static void zlefsdc_settings_class_init (ZlefsdcSettingsClass *klass) {
  G_OBJECT_CLASS (klass)->finalize = zlefsdc_settings_finalize;
  signals[SIG_CHANGED] = g_signal_new (
    "changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
    0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void zlefsdc_settings_init (ZlefsdcSettings *self) {
  self->values = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, free_gvalue);
  apply_defaults (self);
}
