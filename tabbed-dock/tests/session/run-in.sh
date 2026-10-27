#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Roda um teste de sessão do Big Tabbed Dock num contêiner podman.
#
#   run-in.sh <ambiente> <teste> [args]      ambiente: x11-env.sh | wl-env.sh
#
# Laboratório em $BTD_LAB (padrão ~/.cache/tabbed-dock; fora do /tmp, que
# pode ser noexec): build/ (meson, ver build-dock.sh), shots/, work/ e
# wlmaker-inst/ (DESTDIR de um build do wlmaker do fork big-wm/protocols;
# WLINST escolhe outro). Imagem: Containerfile desta pasta, construída como
# localhost/tabbed-dock-img a partir de localhost/wlmaker-dev-img.
# Variáveis repassadas: W, H, LANG; PODMAN_ARGS="-e OUTPUTS=2 -e WALLPAPER=1 …".
set -u
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../.." && pwd)
lab=${BTD_LAB:-$HOME/.cache/tabbed-dock}
mkdir -p "$lab/build" "$lab/shots" "$lab/work"
# shellcheck disable=SC2086 # PODMAN_ARGS é uma lista de opções
podman run --rm ${PODMAN_ARGS:-} -e W -e H -e LANG -v "$repo:/src:Z" -v "$lab/build:/build:Z" -v "$here:/t:Z" \
	-v "$lab/shots:/shots:Z" -v "$lab/work:/work:Z" -v "$lab/${WLINST:-wlmaker-inst}:/wlinst:ro,Z" \
	localhost/tabbed-dock-img bash "/t/$1" bash "/t/$2" "${@:3}"
