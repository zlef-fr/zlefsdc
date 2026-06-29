project('zlefsdc', 'c',
  version: '0.1.0',
  license: 'GPL-2.0-or-later',
  default_options: ['c_std=gnu11', 'warning_level=2'],
)

gtk_dep = dependency('gtk+-3.0')
gio_dep = dependency('gio-2.0')

i18n = import('i18n')

gettext_package = 'zlefsdc'
localedir = join_paths(get_option('prefix'), get_option('localedir'))

add_project_arguments(
  '-DGETTEXT_PACKAGE="' + gettext_package + '"',
  '-DZLEFSDC_LOCALEDIR="' + localedir + '"',
  language: 'c',
)

subdir('core')
subdir('hosts/standalone')

# The xfce4-panel plugin is only built where its dev files exist, so the core
# library + standalone host still build on any GTK desktop.
xfce_dep = dependency('libxfce4panel-2.0', required: get_option('xfce'))
xfceui_dep = dependency('libxfce4ui-2', required: get_option('xfce'))
build_xfce = xfce_dep.found() and xfceui_dep.found()
if build_xfce
  subdir('hosts/xfce')
endif

subdir('po')

summary({
  'standalone host': true,
  'xfce4-panel plugin': build_xfce,
}, section: 'Hosts')
