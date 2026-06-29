/* ZlefSDC — async album-art loader + cache (DE-agnostic).
 *
 * Resolves an MPRIS mpris:artUrl (file:// or http(s)://) to a GdkPixbuf scaled
 * to a square box, off the main loop, with a small in-memory cache keyed by
 * url@size. Network art is fetched through GIO's GVfs, so no extra HTTP dep.
 */
#ifndef ZLEFSDC_COVER_H
#define ZLEFSDC_COVER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ZLEFSDC_TYPE_COVER (zlefsdc_cover_get_type ())
G_DECLARE_FINAL_TYPE (ZlefsdcCover, zlefsdc_cover, ZLEFSDC, COVER, GObject)

ZlefsdcCover *zlefsdc_cover_new (void);

/* Callback receives a pixbuf (owned by the cache; ref it to keep) or NULL on
 * failure. Calls supersede each other: requesting a new url cancels the prior
 * pending load so a stale cover never lands after a track change. */
typedef void (*ZlefsdcCoverReady) (GdkPixbuf *pixbuf, gpointer user_data);

void zlefsdc_cover_request (ZlefsdcCover     *self,
                            const char       *url,
                            int               size,
                            ZlefsdcCoverReady cb,
                            gpointer          user_data);

G_END_DECLS

#endif /* ZLEFSDC_COVER_H */
