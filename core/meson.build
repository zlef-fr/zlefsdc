core_sources = files(
  'zlefsdc-settings.c',
  'zlefsdc-player.c',
  'zlefsdc-cover.c',
  'zlefsdc-widget.c',
  'zlefsdc-prefs.c',
)

core_deps = [gtk_dep, gio_dep]

libzlefsdc = static_library(
  'zlefsdc',
  core_sources,
  dependencies: core_deps,
  include_directories: include_directories('.'),
  install: false,
)

zlefsdc_core_dep = declare_dependency(
  link_with: libzlefsdc,
  dependencies: core_deps,
  include_directories: include_directories('.'),
)
