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

# Ícone do dock e das preferências: o tabbed-dock.svg dos ícones do Big
# WindowMaker (themes/icons/svg, big-wm-themes) no hicolor, achado com
# qualquer tema de ícones. Compilado fora da árvore do big-wm, fica sem ele
# (as preferências usam um ícone genérico).
tabbed_icon = meson.project_source_root() / '..' / 'themes' / 'icons' / 'svg' / 'tabbed-dock.svg'
fs = import('fs')
if fs.is_file(tabbed_icon)
  install_data(tabbed_icon, rename: 'big-wm-tabbed-dock.svg',
    install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps')
endif
