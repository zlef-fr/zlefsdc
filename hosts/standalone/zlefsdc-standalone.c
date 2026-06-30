/* ZlefSDC — standalone host.
 *
 * A bare GTK window that hosts the ZlefsdcWidget. Useful on desktops without a
 * supported panel, and as the reference for how little a host has to do: make
 * settings, make the widget, show it. A gear button opens the shared prefs.
 */
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "zlefsdc.h"

#ifndef ZLEFSDC_LOCALEDIR
#define ZLEFSDC_LOCALEDIR "/usr/share/locale"
#endif

typedef struct { ZlefsdcSettings *settings; GtkWidget *win; } App;

static void open_prefs (GtkButton *b, gpointer data) {
  App *app = data;
  GtkWidget *dlg = gtk_dialog_new_with_buttons (
      _("ZlefSDC Preferences"), GTK_WINDOW (app->win),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      _("_Close"), GTK_RESPONSE_CLOSE, NULL);
  GtkWidget *content = gtk_dialog_get_content_area (GTK_DIALOG (dlg));
  gtk_box_pack_start (GTK_BOX (content), zlefsdc_prefs_new (app->settings), TRUE, TRUE, 0);
  gtk_widget_show_all (dlg);
  gtk_dialog_run (GTK_DIALOG (dlg));
  gtk_widget_destroy (dlg);
  (void) b;
}

int main (int argc, char **argv) {
  bindtextdomain (GETTEXT_PACKAGE, ZLEFSDC_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);

  const char *cfg = (argc > 1) ? argv[1]
      : g_build_filename (g_get_user_config_dir (), "zlefsdc", "standalone.ini", NULL);
  ZlefsdcSettings *settings = zlefsdc_settings_new (cfg);

  GtkWidget *win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "ZlefSDC");
  if (!g_getenv ("ZLEFSDC_BARE"))
    gtk_window_set_default_size (GTK_WINDOW (win), 520, 80);
  g_signal_connect (win, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  /* ZLEFSDC_BARE=1 -> undecorated, content-sized window holding just the widget
   * (used to capture clean per-config example tiles for the website). */
  gboolean bare = g_getenv ("ZLEFSDC_BARE") != NULL;
  App app = { settings, win };

  if (!bare) {
    GtkWidget *bar = gtk_header_bar_new ();
    gtk_header_bar_set_title (GTK_HEADER_BAR (bar), "ZlefSDC");
    gtk_header_bar_set_subtitle (GTK_HEADER_BAR (bar), _("Spotify Display Controls"));
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (bar), TRUE);
    GtkWidget *gear = gtk_button_new_from_icon_name ("preferences-system-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_header_bar_pack_end (GTK_HEADER_BAR (bar), gear);
    gtk_window_set_titlebar (GTK_WINDOW (win), bar);
    g_signal_connect (gear, "clicked", G_CALLBACK (open_prefs), &app);
  } else {
    gtk_window_set_decorated (GTK_WINDOW (win), FALSE);
    gtk_window_set_resizable (GTK_WINDOW (win), FALSE);
  }

  /* ZLEFSDC_PANEL=<px> simulates a panel of that thickness (bare mode also
   * constrains the window to it, so any overflow is visible in a screenshot). */
  const char *panel_env = g_getenv ("ZLEFSDC_PANEL");
  int panel = panel_env ? atoi (panel_env) : 56;

  ZlefsdcWidget *w = zlefsdc_widget_new (settings);
  zlefsdc_widget_set_orientation (w, GTK_ORIENTATION_HORIZONTAL);
  zlefsdc_widget_set_panel_size (w, panel);

  GtkWidget *frame = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  g_object_set (frame, "margin", bare ? 4 : 10, NULL);
  gtk_box_pack_start (GTK_BOX (frame), GTK_WIDGET (w), TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (win), frame);
  if (bare) {                       /* pin the widget to the panel height */
    gtk_widget_set_valign (GTK_WIDGET (w), GTK_ALIGN_CENTER);
    gtk_widget_set_size_request (GTK_WIDGET (w), -1, panel);
  }

  gtk_widget_show_all (win);
  gtk_main ();
  return 0;
}
