/* ZlefSDC — reusable settings UI (DE-agnostic).
 *
 * Returns a GtkWidget (a notebook) bound to a ZlefsdcSettings. Every control
 * writes straight back to the model and persists it, so the live widget updates
 * instantly via the settings "changed" signal. Hosts just drop this into their
 * own preferences dialog.
 */
#ifndef ZLEFSDC_PREFS_H
#define ZLEFSDC_PREFS_H

#include <gtk/gtk.h>
#include "zlefsdc-settings.h"

G_BEGIN_DECLS

GtkWidget *zlefsdc_prefs_new (ZlefsdcSettings *settings);

G_END_DECLS

#endif /* ZLEFSDC_PREFS_H */
