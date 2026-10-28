#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-stage.sh - monta o tar que leva mudanças da edição (iso-profiles)
# e do big-wm para uma imagem de benchmark, sem gerar ISO nem pacote.
#
#   bigwm-perf-stage.sh ISO_PROFILES BIG_WM SAÍDA.tar
#
# Copia, com dono root:
#   - do desktop-overlay da edição bigcommunity/wmaker, os caminhos em
#     OVERLAY (o que o buildiso põe na imagem e o Calamares leva para o
#     sistema instalado);
#   - do "make install-session" do big-wm, os caminhos em BIGWM (o que o
#     pacote big-wm-session instala).
# Aplicar na VM (bigwm-perf-vm.py shell --writable, numa imagem nova sobre a
# anterior):
#   @push SAÍDA.tar /var/tmp/stage.tar
#   tar --no-same-owner --no-overwrite-dir -xf /var/tmp/stage.tar -C / && rm /var/tmp/stage.tar
#   rm -f <arquivos que o pacote novo deixou de instalar>
#   systemctl daemon-reload; udevadm control --reload
# O pacman não sabe desses arquivos (pacman -Qkk acusa): a imagem é só para
# medir, a instalação de verdade vem da ISO e dos pacotes.
set -euo pipefail
iso=$(realpath "${1:?iso-profiles}") bigwm=$(realpath "${2:?big-wm}") out=$(realpath -m "${3:?saída.tar}")

OVERLAY=(
	etc/systemd/system/ModemManager.service.d
	etc/udev/rules.d/90-bigwm-modemmanager.rules
	etc/systemd/user/render-menu-options-user.service.d
	etc/systemd/user/biglinux-snapshot-detect.service.d
)
BIGWM=(
	usr/lib/big-wm/big-wm-autostart-condition
	usr/lib/big-wm/big-wm-pamac-tray
	'usr/lib/systemd/user/app-org.manjaro.pamac.tray@autostart.service.d'
	'usr/lib/systemd/user/app-pamac\x2dtray@autostart.service.d'
	usr/lib/systemd/user/pamac-tray.service.d
)

work=$(mktemp -d "${TMPDIR:-/var/tmp}/bigwm-stage.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/stage" "$work/bigwm"
make -s -C "$bigwm" install-session DESTDIR="$work/bigwm" >/dev/null
(cd "$iso/bigcommunity/wmaker/desktop-overlay" && cp -a --parents "${OVERLAY[@]}" "$work/stage/")
(cd "$work/bigwm" && cp -a --parents "${BIGWM[@]}" "$work/stage/")
# Nada do shared/ do iso-profiles: só arquivos da edição.
for p in "${OVERLAY[@]}"; do
	real=$(realpath "$iso/bigcommunity/wmaker/desktop-overlay/$p")
	case $real in "$iso/shared/"*) echo "erro: $p vem do shared/" >&2; exit 1 ;; esac
done
tar --owner=0 --group=0 -C "$work/stage" -cf "$out" etc usr
tar -tf "$out" | grep -v '/$'
