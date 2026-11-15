#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-cenarios.sh - cenários de uso na sessão X11 aberta de USUÁRIO
# (como root, com DISPLAY e XAUTHORITY do Xorg; docs/performance/11-cenarios.md):
#   B  abrir cada programa a frio (drop_caches) e 2 vezes com cache: segundos
#      até a janela entrar na _NET_CLIENT_LIST;
#   C  todos abertos: memória, swap, PSI e a latência de um comando curto;
#   E  200 movimentos da janela do Thunar: ticks de Xorg, wmaker e picom.
#
#   bigwm-perf-cenarios.sh USUÁRIO
#
# Fecha os programas da lista ao começar e entre as medições (perde o que
# estiver aberto neles). Firefox com um perfil próprio de teste em
# /var/tmp/bigwm-perf-ffprof; Kate com -n -b (sem -b ele se separa do
# processo e a unidade do systemd leva tudo junto; sem -n a instância aberta
# recebe o pedido e o novo processo sai sem janela).
set -u
export LC_NUMERIC=C
u=${1:?uso: bigwm-perf-cenarios.sh USUÁRIO}
uid=$(id -u "$u")
ffp=/var/tmp/bigwm-perf-ffprof
if [ ! -d "$ffp" ]; then
	mkdir -p "$ffp"
	printf '%s\n' 'user_pref("toolkit.startup.max_resumed_crashes", -1);' \
		'user_pref("browser.shell.checkDefaultBrowser", false);' \
		'user_pref("browser.aboutwelcome.enabled", false);' \
		'user_pref("datareporting.policy.dataSubmissionEnabled", false);' \
		'user_pref("browser.startup.homepage_override.mstone", "ignore");' >"$ffp/user.js"
	chown -R "$u:" "$ffp"
fi
declare -A cmd=([thunar]=thunar [ashyterm]=ashyterm [gnome-calculator]=gnome-calculator
	[kate]="/usr/bin/kate -n -b" [firefox]="/usr/lib/firefox/firefox --profile $ffp --new-instance about:blank")
apps=(thunar ashyterm gnome-calculator kate firefox)

run() {
	# shellcheck disable=SC2086 # comando com argumentos
	runuser -u "$u" -- env XDG_RUNTIME_DIR="/run/user/$uid" systemd-run --user -q --collect \
		--unit="bigwm-cen-$RANDOM" ${cmd[$1]}
}
nwin() { xprop -root _NET_CLIENT_LIST | tr ',' '\n' | grep -c 0x; }
openwait() {
	local n0 t0
	n0=$(nwin) t0=$(date +%s.%N)
	run "$1"
	for _ in $(seq 1200); do [ "$(nwin)" -gt "$n0" ] && break; sleep 0.05; done
	echo "$(date +%s.%N) - $t0" | bc
}
closeall() {
	pkill -u "$u" -x 'thunar|ashyterm|kate|firefox|gnome-calculato'
	pkill -u "$u" -f /usr/lib/firefox/
	for _ in $(seq 60); do
		pgrep -u "$u" -f '/usr/lib/firefox/|thunar|ashyterm|gnome-calculator|bin/kate' >/dev/null || break
		sleep 0.25
	done
	pkill -9 -u "$u" -f '/usr/lib/firefox/|bin/kate'
	sleep 1
}

echo "== B: abertura (s até a janela): frio | cache | cache"
for a in "${apps[@]}"; do
	closeall
	sync
	echo 3 >/proc/sys/vm/drop_caches
	sleep 2
	c=$(openwait "$a"); closeall
	w1=$(openwait "$a"); closeall
	w2=$(openwait "$a"); closeall
	printf "%-17s %6.2f | %6.2f | %6.2f\n" "$a" "$c" "$w1" "$w2"
done

echo "== C: todos abertos"
free -m | sed -n 2p
for a in "${apps[@]}"; do openwait "$a" >/dev/null; done
sleep 15
echo "janelas: $(nwin)"
free -m | sed -n 2,3p
head -1 /proc/pressure/memory
head -1 /proc/pressure/io
l=""
for _ in $(seq 10); do
	s=$(date +%s%N)
	python3 -c pass
	l="$l $((($(date +%s%N) - s) / 1000000))"
done
echo "python3 -c pass (ms):$l"

echo "== E: mover a janela do Thunar 200 vezes"
w=$(xdotool search --onlyvisible --class Thunar | head -1)
tk() { for n in Xorg wmaker picom; do awk '{printf "%d ", $14 + $15}' "/proc/$(pgrep -x $n | head -1)/stat"; done; }
read -r a b c < <(tk)
t0=$(date +%s.%N)
for i in $(seq 200); do xdotool windowmove "$w" $((100 + (i % 40) * 20)) $((100 + (i % 20) * 15)); done
t1=$(date +%s.%N)
read -r d e f < <(tk)
echo "200 movimentos em $(echo "$t1 - $t0" | bc) s; ticks Xorg=$((d - a)) wmaker=$((e - b)) picom=$((f - c))"
closeall
