/* ZlefSDC — the reusable now-playing widget (DE-agnostic).
 *
 * This is the whole product as a single GtkWidget: cover, app icon, title/
 * artist/album, transport buttons and an optional progress bar, all driven by a
 * ZlefsdcPlayer (MPRIS) and a ZlefsdcSettings. A host integration is just:
 *
 *   w = zlefsdc_widget_new (settings);
 *   zlefsdc_widget_set_orientation (w, orientation);   // panel direction
 *   zlefsdc_widget_set_panel_size  (w, thickness_px);  // for auto cover size
 *   gtk_container_add (host_container, GTK_WIDGET (w));
 *
 * The widget owns its player + cover loader and live-rebuilds whenever the
 * settings emit "changed", so adding a new host (GNOME, Waybar, Plasma, a bare
 * window) needs no new rendering code.
 */
#ifndef ZLEFSDC_WIDGET_H
#define ZLEFSDC_WIDGET_H

#include <gtk/gtk.h>
#include "zlefsdc-settings.h"
#include "zlefsdc-player.h"

G_BEGIN_DECLS

#define ZLEFSDC_TYPE_WIDGET (zlefsdc_widget_get_type ())
G_DECLARE_FINAL_TYPE (ZlefsdcWidget, zlefsdc_widget, ZLEFSDC, WIDGET, GtkEventBox)

ZlefsdcWidget *zlefsdc_widget_new (ZlefsdcSettings *settings);

void zlefsdc_widget_set_orientation (ZlefsdcWidget *self, GtkOrientation o);
void zlefsdc_widget_set_panel_size  (ZlefsdcWidget *self, int thickness_px);

ZlefsdcSettings *zlefsdc_widget_get_settings (ZlefsdcWidget *self);
ZlefsdcPlayer   *zlefsdc_widget_get_player   (ZlefsdcWidget *self);

G_END_DECLS

#endif /* ZLEFSDC_WIDGET_H */
