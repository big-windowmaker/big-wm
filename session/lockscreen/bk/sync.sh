#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copia do big-keyboard o núcleo do teclado (src/core/bk-keys.* e
# bk-geom.*) e o layout e o desenho do controle na tela
# (src/gamepad/bk-controls.* e bk-ctlpaint.*) para esta pasta e anota o
# commit no ORIGEM.
#
#   sync.sh [PASTA]           copia de PASTA (padrão: ../big-keyboard ao
#                             lado do big-wm)
#   sync.sh --check [PASTA]   só compara; sai com 1 se a cópia mudou
set -eu
here=$(cd "${0%/*}" && pwd)
check=0
if [ "${1:-}" = --check ]; then
	check=1
	shift
fi
src=${1:-$here/../../../../big-keyboard}
files=(core/bk-keys.c core/bk-keys.h core/bk-geom.c core/bk-geom.h
	gamepad/bk-controls.c gamepad/bk-controls.h gamepad/bk-ctlpaint.c gamepad/bk-ctlpaint.h)
[ -d "$src/src/core" ] && [ -d "$src/src/gamepad" ] ||
	{ echo "sync.sh: $src não parece o big-keyboard (sem src/core e src/gamepad)" >&2; exit 2; }
if [ "$check" = 1 ]; then
	bad=0
	for f in "${files[@]}"; do
		cmp -s "$src/src/$f" "$here/${f##*/}" || { echo "sync.sh: ${f##*/} difere de $src/src/$f" >&2; bad=1; }
	done
	exit "$bad"
fi
for f in "${files[@]}"; do
	install -m644 "$src/src/$f" "$here/${f##*/}"
done
commit=$(git -C "$src" rev-parse --short HEAD 2>/dev/null || echo desconhecido)
sed -i "s/^commit=.*/commit=$commit/" "$here/ORIGEM"
echo "sync.sh: núcleo do big-keyboard $commit copiado"
