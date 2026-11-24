#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Bloqueio do Wayland com três monitores: o big-wm-lock com o gtklock e o
# módulo big-wm-keyboard.so, num wlmaker headless próprio (três saídas, um
# $XDG_RUNTIME_DIR temporário; nunca o compositor da sessão):
#  1. o campo de senha aparece nos três monitores (o gtklock sozinho só o
#     mostra no primeiro);
#  2. o ⌨ do 3.º monitor abre o teclado do big-keyboard nos três;
#  3. teclas tocadas no 3.º e no 1.º monitor escrevem uma senha só, que
#     aparece só no campo do monitor tocado por último (PasswordMask=dot);
#  4. um clique no campo do 2.º monitor leva a senha para lá (os outros
#     ficam vazios).
# Os cliques vêm do ponteiro virtual (wl-pointer.py); a tela, do grim. A
# senha nunca é enviada (nada de Enter): o PAM não é chamado.
#
# uso: lock-wayland.sh <big-wm-keyboard.so>
set -u
here=$(cd "${0%/*}" && pwd)
repo=$(cd "$here/../.." && pwd)
module=$(cd "${1%/*}" && pwd)/${1##*/}
for t in wlmaker gtklock grim; do
	command -v "$t" >/dev/null || { echo "lock-wayland: sem $t, pulado" >&2; exit 77; }
done
# O gtklock encerra com um módulo de outra versão maior: com um gtklock
# instalado de outra versão, o teste não diz nada sobre o módulo.
want=$(sed -n 's/.*module_major_version = \([0-9]*\);.*/\1/p' "$repo/session/lockscreen/gtklock-keyboard.c")
have=$(gtklock --version 2>/dev/null | sed -n 's/^gtklock v\([0-9]*\).*/\1/p')
if [ -n "$want" ] && [ -n "$have" ] && [ "$want" != "$have" ]; then
	echo "lock-wayland: gtklock $have instalado, módulo da versão $want; pulado" >&2
	exit 77
fi

dir=$(mktemp -d)
pids=()
cleanup() {
	# o gtklock deste teste (o big-wm-lock o solta), nunca o da sessão
	pkill -u "$(id -u)" -f "^gtklock .*$dir/" 2>/dev/null
	[ "${#pids[@]}" -gt 0 ] && kill "${pids[@]}" 2>/dev/null
	wait 2>/dev/null
	rm -rf "$dir"
}
trap cleanup EXIT
mkdir -p "$dir/rt" "$dir/home" "$dir/cfg/big-wm"
chmod 700 "$dir/rt"
# Nada da sessão de quem roda o teste: outro runtime, outra configuração.
unset DISPLAY WAYLAND_DISPLAY WAYLAND_SOCKET
export XDG_RUNTIME_DIR=$dir/rt HOME=$dir/home XDG_CONFIG_HOME=$dir/cfg LANG=pt_BR.UTF-8
export XDG_SESSION_TYPE=wayland NO_AT_BRIDGE=1 GSK_RENDERER=cairo BIG_WM_LOCK_GAMEPAD=0
WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=3 WLR_LIBINPUT_NO_DEVICES=1 \
	wlmaker >"$dir/wlmaker.log" 2>&1 &
pids+=($!)
for _ in $(seq 100); do
	sock=$(find "$dir/rt" -maxdepth 1 -name 'wayland-[0-9]*' ! -name '*.lock' | head -1)
	[ -n "$sock" ] && break
	sleep 0.1
done
[ -n "$sock" ] || { echo "lock-wayland: o wlmaker headless não subiu, pulado" >&2; exit 77; }
# Caminho absoluto: os clientes nunca acham o compositor da sessão.
export WAYLAND_DISPLAY=$sock
sleep 1

bad=0
fail() {
	echo "FALHOU: $*" >&2
	bad=$((bad + 1))
}
frames=$here/lock-frames.py
shot() { grim -t ppm "$dir/shot.ppm" 2>/dev/null; }
read_extent() {
	shot || return 1
	read -r W H < <(head -c 32 "$dir/shot.ppm" | tr -s ' \n' '  ' | cut -d' ' -f2,3)
}
lf() { LOCK_FRAMES_PPM=$dir/shot.ppm python3 -I "$frames" "$@"; }
click() { python3 -I "$here/wl-pointer.py" "$W" "$H" click "${1%,*}" "${1#*,}" >>"$dir/pointer.log" 2>&1; sleep "${2:-0.3}"; }

printf '[Lock]\nKeyboardLayout=us\nKeyboardTouch=false\nPasswordMask=dot\n' >"$dir/cfg/big-wm/lock.conf"
BIG_WM_DATADIR=$repo/data BIG_WM_GTKLOCK_KEYBOARD=$module \
	timeout 20 "$repo/session/bin/big-wm-lock" >"$dir/lock.log" 2>&1
pgrep -u "$(id -u)" -f "^gtklock .*$dir/" >/dev/null || { echo "lock-wayland: o gtklock não bloqueou: $(tail -n3 "$dir/lock.log")" >&2; exit 1; }
sleep 1.5
read_extent || { echo "lock-wayland: sem captura do grim" >&2; exit 1; }
mw=$((W / 3))
mon=("0-$mw" "$mw-$((2 * mw))" "$((2 * mw))-$W")
[ "$((mw * 3))" = "$W" ] || fail "três saídas lado a lado: captura de ${W}x$H"

# 1. O campo em cada monitor.
for m in 0 1 2; do
	read -r box sig <<<"$(lf field "${mon[$m]}")"
	fields[m]=$box
	[ "$box" = none ] && fail "monitor $((m + 1)) sem o campo de senha"
	[ "${sig:-0}" -lt 60 ] || fail "monitor $((m + 1)): o campo vazio já mostra $sig pixels de sinais"
done
if [ "$bad" != 0 ]; then
	cp "$dir/shot.ppm" "${BIG_WM_LOCK_WAYLAND_SHOT:-/dev/null}" 2>/dev/null
	echo "lock-wayland: $bad falha(s)" >&2
	exit 1
fi

# 2. O ⌨ do 3.º monitor: a ponta direita do campo.
IFS=, read -r fx0 fy0 fx1 fy1 <<<"${fields[2]}"
click "$((fx1 - 14)),$(((fy0 + fy1) / 2))" 0.8
shot
for m in 0 1 2; do
	read -ra k <<<"$(lf find keys ab "${mon[$m]}")"
	[ "${k[0]}" = none ] && fail "monitor $((m + 1)) sem o teclado na tela"
	keys[m]="${k[*]}"
done

# 3. "ab" no teclado do 3.º monitor e "c" no do 1.º: a senha vai junto.
if [ "$bad" = 0 ]; then
	read -ra k3 <<<"${keys[2]}"
	click "${k3[0]}" 0.2
	click "${k3[1]}" 0.4
	shot
	read -r _ s1 <<<"$(lf field "${mon[0]}")"
	read -r _ s3 <<<"$(lf field "${mon[2]}")"
	[ "$s3" -ge 60 ] && [ "$s1" -lt 60 ] || fail "\"ab\" no 3.º monitor: sinais $s1 (1.º) e $s3 (3.º)"
	read -ra k1 <<<"$(lf find keys c "${mon[0]}")"
	click "${k1[0]}" 0.4
	shot
	read -r _ s1 <<<"$(lf field "${mon[0]}")"
	read -r _ s3 <<<"$(lf field "${mon[2]}")"
	[ "$s1" -ge 90 ] && [ "$s3" -lt 60 ] ||
		fail "\"c\" no 1.º monitor: a senha devia ir inteira para o 1.º (sinais $s1) e sair do 3.º ($s3)"
	three=$s1
	# 4. Clique no campo do 2.º monitor.
	IFS=, read -r fx0 fy0 fx1 fy1 <<<"${fields[1]}"
	click "$((fx0 + 40)),$(((fy0 + fy1) / 2))" 0.4
	shot
	read -r _ s1 <<<"$(lf field "${mon[0]}")"
	read -r _ s2 <<<"$(lf field "${mon[1]}")"
	read -r _ s3 <<<"$(lf field "${mon[2]}")"
	[ "$s2" -ge $((three - 30)) ] && [ "$s1" -lt 60 ] && [ "$s3" -lt 60 ] ||
		fail "clique no 2.º monitor: sinais $s1, $s2, $s3 (a senha devia estar só no 2.º)"
fi
cp "$dir/shot.ppm" "${BIG_WM_LOCK_WAYLAND_SHOT:-/dev/null}" 2>/dev/null

[ "$bad" = 0 ] || { echo "lock-wayland: $bad falha(s)" >&2; exit 1; }
echo "lock-wayland: ok"
