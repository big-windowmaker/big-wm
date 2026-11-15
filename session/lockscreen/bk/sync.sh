#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copia o núcleo do teclado do big-keyboard (src/core/bk-keys.* e
# bk-geom.*) para esta pasta e anota o commit no ORIGEM.
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
files=(bk-keys.c bk-keys.h bk-geom.c bk-geom.h)
[ -d "$src/src/core" ] || { echo "sync.sh: $src não parece o big-keyboard (sem src/core)" >&2; exit 2; }
if [ "$check" = 1 ]; then
	bad=0
	for f in "${files[@]}"; do
		cmp -s "$src/src/core/$f" "$here/$f" || { echo "sync.sh: $f difere de $src/src/core/$f" >&2; bad=1; }
	done
	exit "$bad"
fi
for f in "${files[@]}"; do
	install -m644 "$src/src/core/$f" "$here/$f"
done
commit=$(git -C "$src" rev-parse --short HEAD 2>/dev/null || echo desconhecido)
sed -i "s/^commit=.*/commit=$commit/" "$here/ORIGEM"
echo "sync.sh: núcleo do big-keyboard $commit copiado"
