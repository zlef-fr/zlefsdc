/* ZlefSDC — settings model (DE-agnostic).
 *
 * A keyfile-backed property bag describing exactly how the widget renders and
 * what its quick actions do. Emits "changed" on any mutation so a live widget
 * can re-render immediately. Hosts decide *where* the keyfile lives (xfce gives
 * one path, the standalone harness another); the model itself is host-neutral.
 */
#ifndef ZLEFSDC_SETTINGS_H
#define ZLEFSDC_SETTINGS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ZLEFSDC_TYPE_SETTINGS (zlefsdc_settings_get_type ())
G_DECLARE_FINAL_TYPE (ZlefsdcSettings, zlefsdc_settings, ZLEFSDC, SETTINGS, GObject)

/* Quick-action vocabulary, reused for clicks and scroll. */
typedef enum {
  ZLEFSDC_ACTION_NONE,
  ZLEFSDC_ACTION_PLAYPAUSE,
  ZLEFSDC_ACTION_NEXT,
  ZLEFSDC_ACTION_PREVIOUS,
  ZLEFSDC_ACTION_STOP,
  ZLEFSDC_ACTION_RAISE,        /* bring the player window to front */
  ZLEFSDC_ACTION_COMMAND,      /* run settings->command_* shell string */
  ZLEFSDC_ACTION_N
} ZlefsdcAction;

const char *zlefsdc_action_nick (ZlefsdcAction a);   /* stable keyfile token */
ZlefsdcAction zlefsdc_action_from_nick (const char *nick);

/* Element tokens for the order string (comma-separated). */
/* cover, icon, info, progress, prev, playpause, next */

ZlefsdcSettings *zlefsdc_settings_new (const char *keyfile_path);

/* Persistence. load() resets to defaults then overlays the file (missing file
 * is fine). save() writes atomically. Both are no-ops without a path. */
gboolean zlefsdc_settings_load (ZlefsdcSettings *self);
gboolean zlefsdc_settings_save (ZlefsdcSettings *self, GError **error);

/* Batch a series of mutations into a single "changed" emission. */
void zlefsdc_settings_freeze (ZlefsdcSettings *self);
void zlefsdc_settings_thaw   (ZlefsdcSettings *self);

/* --- typed accessors over a flat key space ------------------------------- */
gboolean    zlefsdc_settings_get_bool   (ZlefsdcSettings *self, const char *key);
void        zlefsdc_settings_set_bool   (ZlefsdcSettings *self, const char *key, gboolean v);
int         zlefsdc_settings_get_int    (ZlefsdcSettings *self, const char *key);
void        zlefsdc_settings_set_int    (ZlefsdcSettings *self, const char *key, int v);
const char *zlefsdc_settings_get_string (ZlefsdcSettings *self, const char *key);
void        zlefsdc_settings_set_string (ZlefsdcSettings *self, const char *key, const char *v);

ZlefsdcAction zlefsdc_settings_get_action (ZlefsdcSettings *self, const char *key);

G_END_DECLS

#endif /* ZLEFSDC_SETTINGS_H */
