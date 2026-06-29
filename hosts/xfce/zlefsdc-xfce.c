/* ZlefSDC — xfce4-panel plugin host.
 *
 * A thin adapter: it gives the core a per-instance keyfile (the panel's rc
 * file), embeds a ZlefsdcWidget, and forwards the panel's orientation/size into
 * the widget. All rendering, controls and preferences live in libzlefsdc, so
 * this file stays tiny — the template for any future panel/DE host.
 */
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4ui/libxfce4ui.h>
/* libxfce4util (pulled in above) already provides _(); including glib/gi18n.h
 * here would redefine it against the wrong GETTEXT_PACKAGE. */
#include "zlefsdc.h"

typedef struct {
  XfcePanelPlugin *plugin;
  ZlefsdcSettings *settings;
  ZlefsdcWidget   *widget;
} ZlefsdcXfce;

static void apply_orientation (ZlefsdcXfce *zx) {
  GtkOrientation o = xfce_panel_plugin_get_orientation (zx->plugin);
  zlefsdc_widget_set_orientation (zx->widget, o);
}

static void on_orientation_changed (XfcePanelPlugin *p, GtkOrientation o, ZlefsdcXfce *zx) {
  (void) p; (void) o;
  apply_orientation (zx);
}

static gboolean on_size_changed (XfcePanelPlugin *p, gint size, ZlefsdcXfce *zx) {
  /* size is the panel thickness; row-count handles multi-row panels */
  gint rows = xfce_panel_plugin_get_nrows (p);
  zlefsdc_widget_set_panel_size (zx->widget, size / MAX (rows, 1));
  return TRUE;
}

static void on_configure (XfcePanelPlugin *p, ZlefsdcXfce *zx) {
  xfce_panel_plugin_block_menu (p);

  GtkWidget *dlg = xfce_titled_dialog_new_with_mixed_buttons (
      _("ZlefSDC"), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (p))),
      GTK_DIALOG_DESTROY_WITH_PARENT,
      "window-close-symbolic", _("_Close"), GTK_RESPONSE_CLOSE,
      NULL);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "multimedia-player");
  xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dlg),
      _("Spotify Display Controls"));

  GtkWidget *content = gtk_dialog_get_content_area (GTK_DIALOG (dlg));
  gtk_box_pack_start (GTK_BOX (content), zlefsdc_prefs_new (zx->settings), TRUE, TRUE, 0);

  g_signal_connect (dlg, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  g_signal_connect_swapped (dlg, "destroy", G_CALLBACK (xfce_panel_plugin_unblock_menu), p);

  gtk_widget_show_all (dlg);
}

static void on_save (XfcePanelPlugin *p, ZlefsdcXfce *zx) {
  (void) p;
  zlefsdc_settings_save (zx->settings, NULL);
}

static void on_free (XfcePanelPlugin *p, ZlefsdcXfce *zx) {
  (void) p;
  g_clear_object (&zx->settings);
  g_slice_free (ZlefsdcXfce, zx);
}

static void zlefsdc_construct (XfcePanelPlugin *plugin) {
  xfce_textdomain (GETTEXT_PACKAGE, ZLEFSDC_LOCALEDIR, "UTF-8");

  ZlefsdcXfce *zx = g_slice_new0 (ZlefsdcXfce);
  zx->plugin = plugin;

  /* per-instance keyfile owned by the panel (created if missing) */
  char *rc = xfce_panel_plugin_save_location (plugin, TRUE);
  zx->settings = zlefsdc_settings_new (rc);
  g_free (rc);

  zx->widget = zlefsdc_widget_new (zx->settings);
  gtk_container_add (GTK_CONTAINER (plugin), GTK_WIDGET (zx->widget));
  gtk_widget_show (GTK_WIDGET (zx->widget));

  /* let right-click raise the panel menu over our widget */
  xfce_panel_plugin_add_action_widget (plugin, GTK_WIDGET (zx->widget));
  xfce_panel_plugin_menu_show_configure (plugin);

  apply_orientation (zx);
  zlefsdc_widget_set_panel_size (zx->widget,
      xfce_panel_plugin_get_size (plugin) / MAX (xfce_panel_plugin_get_nrows (plugin), 1));

  g_signal_connect (plugin, "orientation-changed", G_CALLBACK (on_orientation_changed), zx);
  g_signal_connect (plugin, "size-changed",        G_CALLBACK (on_size_changed), zx);
  g_signal_connect (plugin, "configure-plugin",    G_CALLBACK (on_configure), zx);
  g_signal_connect (plugin, "save",                G_CALLBACK (on_save), zx);
  g_signal_connect (plugin, "free-data",           G_CALLBACK (on_free), zx);
}

XFCE_PANEL_PLUGIN_REGISTER (zlefsdc_construct)
