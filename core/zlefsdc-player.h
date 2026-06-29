/* ZlefSDC — MPRIS player backend (DE-agnostic).
 *
 * Wraps the MPRIS2 D-Bus interface of a media player. Despite the product name
 * ("Spotify Display Controls") this speaks plain MPRIS, so it drives *any*
 * compliant player; the target is chosen by settings ("spotify" by default, or
 * "auto" to grab the first one that appears). Exposes a flat snapshot of the
 * now-playing state and a handful of control methods; emits "changed" whenever
 * the snapshot moves, and "appeared"/"vanished" as players come and go.
 */
#ifndef ZLEFSDC_PLAYER_H
#define ZLEFSDC_PLAYER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ZLEFSDC_TYPE_PLAYER (zlefsdc_player_get_type ())
G_DECLARE_FINAL_TYPE (ZlefsdcPlayer, zlefsdc_player, ZLEFSDC, PLAYER, GObject)

typedef enum {
  ZLEFSDC_PLAYBACK_STOPPED,
  ZLEFSDC_PLAYBACK_PAUSED,
  ZLEFSDC_PLAYBACK_PLAYING
} ZlefsdcPlayback;

ZlefsdcPlayer *zlefsdc_player_new (void);

/* Which player to track. @target is "auto", a bus suffix ("spotify"), or a full
 * "org.mpris.MediaPlayer2.x" name. Re-resolves immediately. */
void zlefsdc_player_set_target (ZlefsdcPlayer *self, const char *target);

gboolean         zlefsdc_player_has_player   (ZlefsdcPlayer *self);
ZlefsdcPlayback  zlefsdc_player_get_playback (ZlefsdcPlayer *self);
const char      *zlefsdc_player_get_title    (ZlefsdcPlayer *self);
const char      *zlefsdc_player_get_artist   (ZlefsdcPlayer *self); /* pre-joined */
const char      *zlefsdc_player_get_album    (ZlefsdcPlayer *self);
const char      *zlefsdc_player_get_art_url  (ZlefsdcPlayer *self); /* file:// or http(s) */
const char      *zlefsdc_player_get_app_name (ZlefsdcPlayer *self); /* Identity   */
const char      *zlefsdc_player_get_icon_name(ZlefsdcPlayer *self); /* DesktopEntry */
gboolean         zlefsdc_player_can_next     (ZlefsdcPlayer *self);
gboolean         zlefsdc_player_can_prev     (ZlefsdcPlayer *self);
gint64           zlefsdc_player_get_length_us(ZlefsdcPlayer *self);
gint64           zlefsdc_player_get_position_us (ZlefsdcPlayer *self); /* fetched live */

/* How artists are joined (default ", "). */
void zlefsdc_player_set_artist_separator (ZlefsdcPlayer *self, const char *sep);

/* Controls (no-ops when no player / capability missing). */
void zlefsdc_player_play_pause (ZlefsdcPlayer *self);
void zlefsdc_player_next       (ZlefsdcPlayer *self);
void zlefsdc_player_previous   (ZlefsdcPlayer *self);
void zlefsdc_player_stop       (ZlefsdcPlayer *self);
void zlefsdc_player_raise      (ZlefsdcPlayer *self);

G_END_DECLS

#endif /* ZLEFSDC_PLAYER_H */
