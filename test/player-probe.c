/* Headless check of the core player: read now-playing, then drive transport,
 * proving the MPRIS read + control paths (not just the widget rendering). */
#include "zlefsdc.h"

static void pump (int ms) {
  gint64 end = g_get_monotonic_time () + ms * 1000;
  while (g_get_monotonic_time () < end)
    g_main_context_iteration (NULL, FALSE);
}

int main (void) {
  ZlefsdcPlayer *p = zlefsdc_player_new ();
  pump (600);
  g_print ("has_player=%d playback=%d\n", zlefsdc_player_has_player (p), zlefsdc_player_get_playback (p));
  g_print ("title=[%s] artist=[%s] album=[%s]\n",
           zlefsdc_player_get_title (p), zlefsdc_player_get_artist (p), zlefsdc_player_get_album (p));
  g_print ("art=[%s] icon=[%s]\n", zlefsdc_player_get_art_url (p), zlefsdc_player_get_icon_name (p));

  g_print ("-> Next\n");  zlefsdc_player_next (p);       pump (400);
  g_print ("title=[%s] artist=[%s]\n", zlefsdc_player_get_title (p), zlefsdc_player_get_artist (p));
  g_print ("-> PlayPause\n"); zlefsdc_player_play_pause (p); pump (400);
  g_print ("playback=%d\n", zlefsdc_player_get_playback (p));

  g_object_unref (p);
  return 0;
}
