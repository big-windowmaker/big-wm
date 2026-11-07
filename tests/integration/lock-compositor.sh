#!/bin/bash
# Bloqueio X11 com o picom: ciclos de xsecurelock + auth_big_wm num Xvfb com
# dois monitores, uma janela de "sessão" e o picom com o picom.conf dado.
#
# Cada ciclo: bloqueia, envia uma senha errada (às vezes duas), confere que
# continua bloqueado, envia a certa e grava a tela o tempo todo. Falha se
#  - a sessão aparecer em algum quadro enquanto bloqueado;
#  - o aviso branco "INCOMPATIBLE COMPOSITOR, PLEASE FIX!" (o obscurer do
#    xsecurelock) aparecer depois da senha certa;
#  - o xsecurelock sair com a senha errada ou não sair com a certa.
# Um ciclo em cada cinco abre por cima uma janela de tela cheia que pede
# _NET_WM_BYPASS_COMPOSITOR (o picom deixa de compor e esconde a janela de
# sobreposição): aí o obscurer TEM de cobrir a sessão.
#
# A senha vai pelo XTest (xdotool) só para simular o teclado dentro do Xvfb
# de teste; o authproto é falso (aceita "certa") e nunca fala com o PAM.
#
# uso: lock-compositor.sh <auth_big_wm> <picom.conf|none> [ciclos]
set -u
# Um bloqueio do X11, num Xvfb próprio: rodado de dentro de uma sessão
# Wayland, o xsecurelock recusaria ("Wayland detected... Will not lock").
unset WAYLAND_DISPLAY
export XDG_SESSION_TYPE=x11
here=$(cd "${0%/*}" && pwd)
auth=$1 conf=$2 cycles=${3:-30}
for t in Xvfb xsecurelock xdotool xrandr; do
	command -v "$t" >/dev/null || { echo "lock-compositor: sem $t, pulado" >&2; exit 77; }
done
[ "$conf" = none ] || command -v picom >/dev/null || { echo "lock-compositor: sem picom, pulado" >&2; exit 77; }
helpers=/usr/lib/xsecurelock
[ -x "$helpers/saver_blank" ] || { echo "lock-compositor: sem $helpers/saver_blank, pulado" >&2; exit 77; }

dir=$(mktemp -d)
pids=()
cleanup() {
	[ "${#pids[@]}" -gt 0 ] && kill "${pids[@]}" 2>/dev/null
	wait 2>/dev/null
	rm -rf "$dir"
}
trap cleanup EXIT

mkdir -p "$dir/helpers"
cp "$auth" "$dir/helpers/auth_big_wm"
cat >"$dir/helpers/authproto_pam" <<'EOP'
#!/bin/bash
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

W=2048 H=768
Xvfb -displayfd 3 -screen 0 "${W}x${H}x24" -nolisten tcp 3>"$dir/dpy" >/dev/null 2>&1 &
pids+=($!)
for _ in $(seq 50); do [ -s "$dir/dpy" ] && break; sleep 0.1; done
DISPLAY=:$(cat "$dir/dpy")
export DISPLAY XDG_CONFIG_HOME=$dir/cfg
xrandr --setmonitor A 270/1024x200/768+0+0 none >/dev/null
xrandr --setmonitor B 270/1024x200/768+1024+0 none >/dev/null

python3 -c '
import ctypes, ctypes.util, time
X = ctypes.cdll.LoadLibrary(ctypes.util.find_library("X11"))
X.XOpenDisplay.restype = ctypes.c_void_p
X.XCreateSimpleWindow.restype = ctypes.c_ulong
d = ctypes.c_void_p(X.XOpenDisplay(None))
w = X.XCreateSimpleWindow(d, ctypes.c_ulong(X.XDefaultRootWindow(d)), 100, 100, 600, 400, 0, ctypes.c_ulong(0), ctypes.c_ulong(0xff0000))
X.XMapWindow(d, ctypes.c_ulong(w)); X.XFlush(d)
time.sleep(100000)' &
pids+=($!)
if [ "$conf" != none ]; then
	picom --config "$conf" --log-level=warn >"$dir/picom.log" 2>&1 &
	pids+=($!)
fi
sleep 1

bad=0
for i in $(seq "$cycles"); do
	c=$dir/c$i
	mkdir -p "$c"
	mode=normal
	((i % 3 == 0)) && mode=wrong2
	((i % 5 == 0)) && mode=attack
	# Com menos de 5 ciclos (o make check roda 3), o último é o de ataque.
	((cycles < 5 && i == cycles)) && mode=attack
	python3 "$here/lock-frames.py" capture "$c" "$W" "$H" &
	cap=$!
	XSECURELOCK_AUTH=$dir/helpers/auth_big_wm XSECURELOCK_SAVER=saver_blank \
		XSECURELOCK_DISCARD_FIRST_KEYPRESS=0 XSECURELOCK_AUTH_TIMEOUT=60 \
		XSECURELOCK_BLANK_TIMEOUT=600 XSECURELOCK_SHOW_DATETIME=0 \
		XSECURELOCK_BACKGROUND_COLOR=black xsecurelock >"$c.log" 2>&1 &
	xs=$!
	sleep 1.2
	attack_ok=yes
	if [ "$mode" = attack ] && ! python3 "$here/lock-frames.py" attack 2; then
		attack_ok=no
	fi
	xdotool type --delay 20 errada && xdotool key Return
	sleep 1
	if [ "$mode" = wrong2 ]; then
		xdotool type --delay 20 outra && xdotool key Return
		sleep 1
	fi
	alive=no
	kill -0 "$xs" 2>/dev/null && alive=yes
	xdotool type --delay 20 certa
	: >"$c/unlock"
	xdotool key Return
	for _ in $(seq 50); do kill -0 "$xs" 2>/dev/null || break; sleep 0.1; done
	if kill -0 "$xs" 2>/dev/null; then
		kill "$xs"
		status=timeout
	else
		wait "$xs"
		status=$?
	fi
	sleep 0.8
	: >"$c/stop"
	wait "$cap"
	r=$(python3 "$here/lock-frames.py" classify "$c" "$W")
	echo "ciclo $i ($mode): bloqueado_após_errada=$alive saída=$status $r"
	case "$alive $status $r" in
	"yes 0 "*" exposicao=0 aviso=0 "*" final=S") ;;
	*) bad=$((bad + 1)) ;;
	esac
	# Ataque: a janela que pede ao picom para não compor precisa ter
	# rodado, e o obscurer do xsecurelock precisa ter coberto a sessão.
	if [ "$mode" = attack ]; then
		case "$attack_ok $r" in
		"yes "*" obscurer=0 "* | "no "*) bad=$((bad + 1)) ;;
		esac
	fi
	rm -f "$c"/*.ppm
done
[ "$bad" = 0 ] || { echo "lock-compositor: $bad de $cycles ciclo(s) falharam" >&2; exit 1; }
