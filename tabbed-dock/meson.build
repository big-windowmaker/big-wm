# SPDX-License-Identifier: GPL-2.0-or-later
project('big-wm-tabbed-dock', 'c',
  version: '0.1.0',
  license: 'GPL-2.0-or-later',
  meson_version: '>= 0.62',
  default_options: ['c_std=gnu11', 'warning_level=2', 'buildtype=debugoptimized', 'b_ndebug=if-release'])

cc = meson.get_compiler('c')
add_project_arguments(
  '-DG_LOG_DOMAIN="big-wm-tabbed-dock"',
  '-DGETTEXT_PACKAGE="big-wm-tabbed-dock"',
  '-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DBTD_VERSION="@0@"'.format(meson.project_version()),
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_74',
  cc.get_supported_arguments('-Wno-unused-parameter', '-Wno-missing-field-initializers',
                             '-Wshadow', '-Wformat=2', '-Werror=implicit-function-declaration'),
  language: 'c')

glib = dependency('glib-2.0', version: '>= 2.74')
gio = dependency('gio-unix-2.0', version: '>= 2.74')

core_src = files(
  'src/core/btd-model.c',
  'src/core/btd-desktop.c',
  'src/core/btd-config.c',
  'src/core/btd-defaults.c',
  'src/core/btd-coexist.c',
  'src/core/btd-launch.c',
)
core_inc = include_directories('src/core')
core_lib = static_library('btdcore', core_src, dependencies: [glib, gio], include_directories: core_inc)
core_dep = declare_dependency(link_with: core_lib, include_directories: core_inc, dependencies: [glib, gio])

if get_option('dock')
  subdir('src')
  subdir('po')
  subdir('data')
endif

if get_option('tests')
  subdir('tests')
endif
