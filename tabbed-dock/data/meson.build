# SPDX-License-Identifier: GPL-2.0-or-later
i18n_data = import('i18n')
sysconfdir = get_option('prefix') == '/usr' ? '/etc' : get_option('sysconfdir')

i18n_data.merge_file(
  input: 'big-wm-tabbed-dock.desktop.in',
  output: 'big-wm-tabbed-dock.desktop',
  type: 'desktop',
  po_dir: meson.project_source_root() / 'po',
  install: true,
  install_dir: sysconfdir / 'xdg' / 'autostart')

i18n_data.merge_file(
  input: 'big-wm-tabbed-dock-preferences.desktop.in',
  output: 'big-wm-tabbed-dock-preferences.desktop',
  type: 'desktop',
  po_dir: meson.project_source_root() / 'po',
  install: true,
  install_dir: get_option('datadir') / 'applications')

install_data('big-wm.conf',
  install_dir: get_option('prefix') / 'lib' / 'systemd' / 'user' / 'app-big\\x2dwm\\x2dtabbed\\x2ddock@autostart.service.d')
