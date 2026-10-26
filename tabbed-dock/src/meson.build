# SPDX-License-Identifier: GPL-2.0-or-later
gtk = dependency('gtk+-3.0', version: '>= 3.24')
x11 = dependency('x11', required: get_option('x11'))
gtk_x11 = dependency('gtk+-x11-3.0', required: get_option('x11'))
wl_client = dependency('wayland-client', required: get_option('wayland'))
gtk_wl = dependency('gtk+-wayland-3.0', required: get_option('wayland'))
layer_shell = dependency('gtk-layer-shell-0', version: '>= 0.6', required: get_option('wayland'))
wl_protocols = dependency('wayland-protocols', version: '>= 1.36', required: get_option('wayland'))
wl_scanner = find_program('wayland-scanner', native: true, required: get_option('wayland'))

dock_src = files(
  'ui/btd-app.c',
  'ui/btd-dock.c',
  'ui/btd-dock-dnd.c',
  'ui/btd-dock-menu.c',
  'ui/btd-draw.c',
  'ui/btd-icons.c',
  'ui/btd-prefs.c',
  'backend/btd-windows.c',
)
dock_deps = [core_dep, gtk, cc.find_library('m', required: false)]
dock_args = []

have_x11 = x11.found() and gtk_x11.found()
if have_x11
  dock_src += files('backend/btd-x11.c')
  dock_deps += [x11, gtk_x11]
  dock_args += '-DBTD_HAVE_X11'
endif

have_wayland = wl_client.found() and gtk_wl.found() and layer_shell.found() and wl_protocols.found() and wl_scanner.found()
if have_wayland
  protos = [
    wl_protocols.get_variable('pkgdatadir') / 'staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml',
    meson.project_source_root() / 'protocols/wlr-foreign-toplevel-management-unstable-v1.xml',
    wl_protocols.get_variable('pkgdatadir') / 'unstable/xdg-output/xdg-output-unstable-v1.xml',
  ]
  foreach xml : protos
    base = xml.split('/')[-1].replace('.xml', '')
    dock_src += custom_target(base + '-header', input: xml, output: base + '-client-protocol.h',
                              command: [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'])
    dock_src += custom_target(base + '-code', input: xml, output: base + '-protocol.c',
                              command: [wl_scanner, 'private-code', '@INPUT@', '@OUTPUT@'])
  endforeach
  dock_src += files('backend/btd-wayland.c')
  dock_deps += [wl_client, gtk_wl, layer_shell]
  dock_args += '-DBTD_HAVE_WAYLAND'
endif

executable('big-wm-tabbed-dock', dock_src,
  dependencies: dock_deps,
  c_args: dock_args,
  include_directories: include_directories('ui', 'backend'),
  install: true)

summary({'X11 (EWMH)': have_x11, 'Wayland (layer-shell, foreign toplevels)': have_wayland}, section: 'Backends')
