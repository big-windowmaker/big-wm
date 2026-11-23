#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-alloc-load.sh USUÁRIO [CICLOS]: carga do benchmark de alocadores
# (docs/performance/02-metodologia-benchmark.md). Na sessão gráfica aberta do
# USUÁRIO, abre e fecha CICLOS janelas GTK 4 (gnome-calculator), alternando
# Wayland nativo e XWayland (GDK_BACKEND=x11) na sessão Wayland; no X11, só
# X11. Cada janela fica 2 s aberta. Roda como root (systemd-run, a partir do
# --setup do bigwm-perf-vm.py); grava o tempo de cada ciclo e o alocador
# carregado em cada processo do compositor (wlmaker, wmaker) em
# DIR/alloc-load.log (DIR: o 3º argumento, onde o coletor grava). Cada
# execução acrescenta ao log (sessões longas repetem a carga pelo timer).
set -u
u=${1:?uso: bigwm-perf-alloc-load.sh USUÁRIO [CICLOS] [DIR]}
n=${2:-40}
uid=$(id -u "$u")
log=${3:-/var/tmp}/alloc-load.log
mkdir -p "$(dirname "$log")"
echo "inicio $(cut -d' ' -f1 /proc/uptime)" >>"$log"
for p in $(pgrep -x wlmaker) $(pgrep -x wmaker); do
	libs=$(grep -oE 'lib(mimalloc|jemalloc|tcmalloc_minimal|snmallocshim|clang_rt.scudo)[^ ]*|libbigwm-malloc-once' "/proc/$p/maps" | sort -u | tr '\n' ' ')
	echo "maps $p $(tr '\0' ' ' <"/proc/$p/cmdline"): ${libs:-glibc}" >>"$log"
done
wl=
for f in /run/user/"$uid"/wayland-[0-9]; do [ -S "$f" ] && wl=$f && break; done
# Cada linha: ciclo, backend, segundos, código de saída. 124 = o timeout
# fechou a janela (ciclo válido); outro código (inclusive 0: o
# gnome-calculator achou a instância anterior no D-Bus e saiu sem janela
# nova) conta como falha.
falhas=0
for i in $(seq 1 "$n"); do
	be=x11
	if [ -n "$wl" ] && [ $((i % 2)) -eq 0 ]; then be=wayland; fi
	t0=$(date +%s.%N)
	rc=0
	runuser -u "$u" -- env XDG_RUNTIME_DIR="/run/user/$uid" \
		WAYLAND_DISPLAY="$(basename "${wl:-wayland-0}")" DISPLAY=:0 \
		XAUTHORITY="/home/$u/.Xauthority" GDK_BACKEND="$be" \
		timeout 2 gnome-calculator >/dev/null 2>&1 || rc=$?
	[ "$rc" -eq 124 ] || falhas=$((falhas + 1))
	awk -v a="$t0" -v b="$(date +%s.%N)" -v i="$i" -v be="$be" -v rc="$rc" \
		'BEGIN { printf "%d %s %.2f %d\n", i, be, b - a, rc }' >>"$log"
	sleep 0.5
done
echo "fim falhas=$falhas" >>"$log"
[ "$falhas" -eq 0 ]
