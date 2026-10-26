# SPDX-License-Identifier: GPL-2.0-or-later
i18n_data = import('i18n')
# meson usa /etc como sysconfdir quando o prefixo é /usr
sysconfdir = get_option('sysconfdir')

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

meson.add_install_script('install-dropin.sh', meson.current_source_dir() / 'big-wm.conf',
  get_option('prefix') / 'lib')
