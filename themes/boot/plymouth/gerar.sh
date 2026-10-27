#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Monta o tema do Plymouth do Big WindowMaker num diretório:
#
#   gerar.sh DESTINO
#
# O script e o .plymouth vêm daqui; o logo sai do rsvg-convert e as demais
# imagens (ladrilho do Dock, barra de título, cores) do ../pecas.py.

set -euo pipefail

dest=${1:?uso: gerar.sh DESTINO}
aqui=$(cd "$(dirname "$0")" && pwd)
raiz=$(cd "$aqui/../../.." && pwd)

: "${RSVG:=rsvg-convert}"

mkdir -p "$dest"
install -m644 "$aqui/big-wm.plymouth" "$aqui/big-wm.script" "$dest/"
# Logo no ladrilho de 176 px (a proporção do ícone no ladrilho de 64 px do
# Dock) e no painel de senha.
"$RSVG" -w 136 -h 136 "$raiz/themes/logo/big-wm.svg" -o "$dest/logo.png"
"$RSVG" -w 48 -h 48 "$raiz/themes/logo/big-wm.svg" -o "$dest/logo-48.png"
python3 -I "$aqui/../pecas.py" plymouth "$dest"
