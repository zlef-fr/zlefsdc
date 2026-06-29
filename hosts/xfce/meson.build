# The loadable panel module. Name must match X-XFCE-Module in the .desktop.
shared_module(
  'zlefsdc',
  files('zlefsdc-xfce.c'),
  dependencies: [zlefsdc_core_dep, xfce_dep, xfceui_dep],
  install: true,
  install_dir: join_paths(get_option('libdir'), 'xfce4', 'panel', 'plugins'),
)

# Panel-plugin .desktop files use a non-standard [Xfce Panel] group, so the
# translations live inline (Name[fr]=…) rather than going through msgfmt.
install_data(
  'zlefsdc.desktop',
  install_dir: join_paths(get_option('datadir'), 'xfce4', 'panel', 'plugins'),
)
