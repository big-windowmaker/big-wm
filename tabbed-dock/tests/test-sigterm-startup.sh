#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# O dock tem de sair logo com SIGTERM mesmo enquanto espera o display na
# inicialização. Um "servidor X" mudo (aceita a conexão e nunca responde)
# segura o dock antes do laço principal. Antes da correção o SIGTERM ficava
# esperando o laço, e o systemd matava o dock com SIGKILL depois de 5 s,
# atrasando o login seguinte.
#
#   test-sigterm-startup.sh CAMINHO/big-wm-tabbed-dock
set -u
dock=${1:?binário do dock}
if ! command -v dbus-run-session >/dev/null || ! command -v python3 >/dev/null; then
	echo "1..0 # SKIP sem dbus-run-session/python3"
	exit 0
fi
tmp=$(mktemp -d)
trap 'kill "${srv:-}" 2>/dev/null; rm -rf -- "$tmp"' EXIT
num=$((90 + RANDOM % 9))
python3 -I -c '
import socket, sys
s = socket.socket(socket.AF_UNIX); s.bind("\0/tmp/.X11-unix/X" + sys.argv[1]); s.listen(8)
cs = []
while True:
    cs.append(s.accept()[0])
' "$num" &
srv=$!
sleep 0.3
echo "1..1"
# shellcheck disable=SC2016 # o script interno recebe o dock em $1
if dbus-run-session -- env -i PATH="$PATH" HOME="$tmp" XDG_RUNTIME_DIR="$tmp" DISPLAY=":$num" GDK_BACKEND=x11 \
	bash -c '"$1" & d=$!; sleep 0.5; kill -TERM $d
	for _ in $(seq 20); do kill -0 $d 2>/dev/null || exit 0; sleep 0.1; done
	kill -KILL $d; exit 1' _ "$dock"; then
	echo "ok 1 - SIGTERM na inicialização encerra em até 2 s"
else
	echo "not ok 1 - dock ainda vivo 2 s depois do SIGTERM"
	exit 1
fi
