#include "zlefsdc-cover.h"
#include <gio/gio.h>

#define CACHE_MAX 16

struct _ZlefsdcCover {
  GObject       parent;
  GHashTable   *cache;        /* "url@size" -> GdkPixbuf */
  GQueue        order;        /* cache keys, LRU front->back */
  GCancellable *pending;      /* current in-flight load */
};

G_DEFINE_FINAL_TYPE (ZlefsdcCover, zlefsdc_cover, G_TYPE_OBJECT)

typedef struct {
  ZlefsdcCover     *self;
  char             *key;
  int               size;
  ZlefsdcCoverReady cb;
  gpointer          user_data;
} Req;

static void req_free (Req *r) {
  g_clear_object (&r->self);
  g_free (r->key);
  g_free (r);
}

static void cache_put (ZlefsdcCover *self, const char *key, GdkPixbuf *pix) {
  if (g_hash_table_contains (self->cache, key)) return;
  while (g_queue_get_length (&self->order) >= CACHE_MAX) {
    char *old = g_queue_pop_tail (&self->order);
    g_hash_table_remove (self->cache, old);
    g_free (old);
  }
  g_hash_table_insert (self->cache, g_strdup (key), g_object_ref (pix));
  g_queue_push_head (&self->order, g_strdup (key));
}

static GdkPixbuf *square_scale (GdkPixbuf *src, int size) {
  if (!src) return NULL;
  int w = gdk_pixbuf_get_width (src), h = gdk_pixbuf_get_height (src);
  if (w <= 0 || h <= 0) return NULL;
  /* cover-fit: scale so the short side fills, then centre-crop to a square */
  double scale = (double) size / MIN (w, h);
  int sw = (int) (w * scale + 0.5), sh = (int) (h * scale + 0.5);
  GdkPixbuf *scaled = gdk_pixbuf_scale_simple (src, sw, sh, GDK_INTERP_BILINEAR);
  if (!scaled) return NULL;
  int x = (sw - size) / 2, y = (sh - size) / 2;
  GdkPixbuf *crop = gdk_pixbuf_new_subpixbuf (scaled, x, y, size, size);
  GdkPixbuf *out = gdk_pixbuf_copy (crop);      /* detach from parent storage */
  g_object_unref (crop);
  g_object_unref (scaled);
  return out;
}

static void on_loaded (GObject *source, GAsyncResult *res, gpointer user_data) {
  Req *r = user_data;
  GFile *file = G_FILE (source);
  char *data = NULL; gsize len = 0;
  GError *err = NULL;

  gboolean ok = g_file_load_contents_finish (file, res, &data, &len, NULL, &err);
  if (!ok) {
    if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      r->cb (NULL, r->user_data);   /* genuine failure -> let caller fall back */
    g_clear_error (&err);
    req_free (r);
    return;
  }

  GdkPixbuf *raw = NULL;
  GInputStream *mem = g_memory_input_stream_new_from_data (data, len, g_free);
  raw = gdk_pixbuf_new_from_stream (mem, NULL, NULL);
  g_object_unref (mem);

  GdkPixbuf *fit = square_scale (raw, r->size);
  g_clear_object (&raw);

  if (fit) {
    cache_put (r->self, r->key, fit);
    r->cb (fit, r->user_data);
    g_object_unref (fit);
  } else {
    r->cb (NULL, r->user_data);
  }
  req_free (r);
}

void zlefsdc_cover_request (ZlefsdcCover *self, const char *url, int size,
                            ZlefsdcCoverReady cb, gpointer user_data) {
  g_return_if_fail (cb != NULL);

  /* cancel any prior in-flight load so a stale cover can't arrive late */
  if (self->pending) { g_cancellable_cancel (self->pending); g_clear_object (&self->pending); }

  if (!url || !*url || size <= 0) { cb (NULL, user_data); return; }

  char *key = g_strdup_printf ("%s@%d", url, size);
  GdkPixbuf *hit = g_hash_table_lookup (self->cache, key);
  if (hit) {
    /* bump LRU */
    GList *l = g_queue_find_custom (&self->order, key, (GCompareFunc) g_strcmp0);
    if (l) { g_free (l->data); g_queue_delete_link (&self->order, l); }
    g_queue_push_head (&self->order, g_strdup (key));
    cb (hit, user_data);
    g_free (key);
    return;
  }

  Req *r = g_new0 (Req, 1);
  r->self = g_object_ref (self);
  r->key = key;
  r->size = size;
  r->cb = cb;
  r->user_data = user_data;

  self->pending = g_cancellable_new ();
  GFile *file = g_file_new_for_uri (url);
  g_file_load_contents_async (file, self->pending, on_loaded, r);
  g_object_unref (file);
}

ZlefsdcCover *zlefsdc_cover_new (void) { return g_object_new (ZLEFSDC_TYPE_COVER, NULL); }

static void zlefsdc_cover_finalize (GObject *o) {
  ZlefsdcCover *self = ZLEFSDC_COVER (o);
  if (self->pending) g_cancellable_cancel (self->pending);
  g_clear_object (&self->pending);
  g_hash_table_destroy (self->cache);
  char *k; while ((k = g_queue_pop_head (&self->order))) g_free (k);
  G_OBJECT_CLASS (zlefsdc_cover_parent_class)->finalize (o);
}

static void zlefsdc_cover_class_init (ZlefsdcCoverClass *klass) {
  G_OBJECT_CLASS (klass)->finalize = zlefsdc_cover_finalize;
}

static void zlefsdc_cover_init (ZlefsdcCover *self) {
  self->cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  g_queue_init (&self->order);
}
