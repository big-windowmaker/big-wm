#!/bin/bash
# Teclado na tela do bloqueio X11 (auth_big_wm do xsecurelock), num Xvfb:
#  1. o botão ⌨ do campo abre o teclado; uma senha errada digitada nele
#     não desbloqueia; a certa ("certa"), com o Enter da tela, desbloqueia;
#  2. Cancelar fecha o diálogo e a tela continua bloqueada;
#  3. com VirtualKeyboard=false no lock.conf o campo fica sem o botão ⌨.
# Os cliques vão pelo XTest (xdotool) só para simular o mouse no Xvfb de
# teste; o authproto é falso (aceita "certa") e nunca fala com o PAM.
#
# uso: lock-keyboard.sh <auth_big_wm>
set -u
here=$(cd "${0%/*}" && pwd)
auth=$1
for t in Xvfb xsecurelock xdotool; do
	command -v "$t" >/dev/null || { echo "lock-keyboard: sem $t, pulado" >&2; exit 77; }
done
[ -x /usr/lib/xsecurelock/saver_blank ] || { echo "lock-keyboard: sem o saver_blank do xsecurelock, pulado" >&2; exit 77; }

dir=$(mktemp -d)
pids=()
cleanup() {
	[ "${#pids[@]}" -gt 0 ] && kill "${pids[@]}" 2>/dev/null
	wait 2>/dev/null
	rm -rf "$dir"
}
trap cleanup EXIT
mkdir -p "$dir/helpers" "$dir/cfg/big-wm"
cp "$auth" "$dir/helpers/auth_big_wm"
cat >"$dir/helpers/authproto_pam" <<'EOP'
#!/bin/bash
export LC_ALL=C
printf 'P 10\nPassword: \n'
read -r hdr || exit 1
type=${hdr%% *} len=${hdr#* }
IFS= read -r -N "$len" pw
read -r _
[ "$type" = p ] && [ "$pw" = certa ] && exit 0
printf 'e 22\nAuthentication failure\n'
exit 1
EOP
chmod +x "$dir/helpers/authproto_pam"

# -noreset: sem clientes entre um passo e outro, o Xvfb não recomeça.
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp -noreset 3>"$dir/dpy" >/dev/null 2>&1 &
pids+=($!)
for _ in $(seq 50); do [ -s "$dir/dpy" ] && break; sleep 0.1; done
DISPLAY=:$(cat "$dir/dpy")
export DISPLAY XDG_CONFIG_HOME=$dir/cfg LANG=pt_BR.UTF-8
frames=$here/lock-frames.py
bad=0
fail() {
	echo "FALHOU: $*" >&2
	bad=$((bad + 1))
}
click() { xdotool mousemove "${1%,*}" "${1#*,}" click 1; sleep "${2:-0.2}"; }

lock() {
	XSECURELOCK_AUTH=$dir/helpers/auth_big_wm XSECURELOCK_SAVER=saver_blank \
		XSECURELOCK_DISCARD_FIRST_KEYPRESS=0 XSECURELOCK_AUTH_TIMEOUT=60 \
		xsecurelock >>"$dir/xsl.log" 2>&1 &
	xs=$!
	sleep 1.2
	xdotool mousemove 500 300 # o movimento abre o diálogo
	sleep 0.3
	xdotool mousemove 510 310
	sleep 0.8
}

# 1. ⌨, senha errada, senha certa.
printf '[Lock]\n' >"$dir/cfg/big-wm/lock.conf"
lock
icon=$(python3 "$frames" find icon)
if [ "$icon" = none ]; then
	fail "sem o botão ⌨ no campo de senha"
else
	click "$icon" 0.6
	read -ra k <<<"$(python3 "$frames" find keys errado)"
	if [ "${k[0]}" = none ] || [ "${#k[@]}" -lt 8 ]; then
		fail "o teclado não abriu com o botão ⌨"
	else
		for i in 0 1 2 3 4 5; do click "${k[$i]}" 0.15; done
		click "${k[6]}" 1.2
		kill -0 "$xs" 2>/dev/null || fail "desbloqueou com a senha errada da tela"
		read -ra k <<<"$(python3 "$frames" find keys certa)"
		for i in 0 1 2 3 4; do click "${k[$i]}" 0.15; done
		click "${k[5]}" 0.2
	fi
fi
for _ in $(seq 40); do kill -0 "$xs" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$xs" 2>/dev/null; then
	fail "a senha certa digitada na tela não desbloqueou"
	kill "$xs"
	wait "$xs" 2>/dev/null
else
	wait "$xs" || fail "xsecurelock saiu com erro depois da senha da tela"
fi

# 2. Cancelar: o diálogo some (o auth sai), a tela continua bloqueada.
lock
icon=$(python3 "$frames" find icon)
click "$icon" 0.6
read -ra k <<<"$(python3 "$frames" find keys c)"
if [ "${k[0]}" = none ]; then
	fail "teclado não abriu para o teste do Cancelar"
else
	click "${k[0]}" 0.2
	click "${k[2]}" 1
	kill -0 "$xs" 2>/dev/null || fail "Cancelar desbloqueou a tela"
	[ "$(python3 "$frames" find icon)" = none ] || fail "Cancelar não fechou o diálogo"
fi
xdotool type --delay 30 certa
xdotool key Return
for _ in $(seq 40); do kill -0 "$xs" 2>/dev/null || break; sleep 0.1; done
kill -0 "$xs" 2>/dev/null && { fail "teclado físico não desbloqueou depois do Cancelar"; kill "$xs"; }
wait "$xs" 2>/dev/null

# 3. VirtualKeyboard=false: sem o botão.
printf '[Lock]\nVirtualKeyboard=false\n' >"$dir/cfg/big-wm/lock.conf"
lock
[ "$(python3 "$frames" find icon)" = none ] || fail "VirtualKeyboard=false e o botão ⌨ continua no campo"
xdotool type --delay 30 certa
xdotool key Return
for _ in $(seq 40); do kill -0 "$xs" 2>/dev/null || break; sleep 0.1; done
kill -0 "$xs" 2>/dev/null && { fail "teclado físico não desbloqueou (VirtualKeyboard=false)"; kill "$xs"; }
wait "$xs" 2>/dev/null

[ "$bad" = 0 ] || { echo "lock-keyboard: $bad falha(s)" >&2; exit 1; }
echo "lock-keyboard: ok"
