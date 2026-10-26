#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Instala o drop-in do serviço de autostart do dock. O nome da unidade tem
# "\x2d" (escape do systemd para "-"), que o meson trataria como separador de
# diretório em install_dir.
# Uso (meson.add_install_script): install-dropin.sh <arquivo> <libdir-do-prefixo>
set -eu
dir="${DESTDIR:-}$2/systemd/user/app-big\\x2dwm\\x2dtabbed\\x2ddock@autostart.service.d"
install -d "$dir"
install -m644 "$1" "$dir/big-wm.conf"
