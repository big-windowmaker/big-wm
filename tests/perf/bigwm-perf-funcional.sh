#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-funcional.sh USUÁRIO [DIR]: 20 verificações funcionais na sessão
# gráfica aberta do USUÁRIO (X11 ou Wayland), como root, dentro da VM do
# bigwm-perf-vm.py (--setup com systemd-run, como a carga dos alocadores;
# docs/performance/2026-11-23-memoria/04-validacao-virt-manager.md).
# Uma linha por verificação em DIR/funcional.log: "ok|FALHA NN nome: detalhe".
# Não substitui os testes do tests/run.sh nem o teste manual: confere que a
# sessão subiu inteira (Dock, Clip, dockapps, bandeja, notificações, portal,
# áudio, área de transferência, janelas nativas e X11, captura de tela) e
# que nada caiu (coredump, unidade com falha).
set -u
u=${1:?uso: bigwm-perf-funcional.sh USUÁRIO [DIR]}
dir=${2:-/var/tmp}
log=$dir/funcional.log
mkdir -p "$dir"
: >"$log"
uid=$(id -u "$u")
rt=/run/user/$uid
wl=
for f in "$rt"/wayland-[0-9]; do [ -S "$f" ] && wl=$(basename "$f") && break; done
xauth=/home/$u/.Xauthority
as_user() {
	runuser -u "$u" -- env XDG_RUNTIME_DIR="$rt" DBUS_SESSION_BUS_ADDRESS="unix:path=$rt/bus" \
		WAYLAND_DISPLAY="${wl:-wayland-0}" DISPLAY=:0 XAUTHORITY="$xauth" LANG=C "$@"
}
n=0
check() { # check NOME DETALHE COMANDO...
	local name=$1 detail=$2
	shift 2
	n=$((n + 1))
	if "$@" >/dev/null 2>&1; then
		printf 'ok %02d %s: %s\n' "$n" "$name" "$detail" >>"$log"
	else
		printf 'FALHA %02d %s: %s\n' "$n" "$name" "$detail" >>"$log"
	fi
}
running() { pgrep -u "$u" -x "$1"; }
# Alguma janela do X com a classe $1 (WM_CLASS); a imagem não tem o
# xwininfo: XQueryTree e XGetClassHint pela libX11 (ctypes).
x_class() {
	as_user python3 -I -c '
import ctypes, sys
x = ctypes.CDLL("libX11.so.6")
x.XOpenDisplay.restype = ctypes.c_void_p
x.XDefaultRootWindow.restype = ctypes.c_ulong
x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
x.XQueryTree.argtypes = [ctypes.c_void_p, ctypes.c_ulong] + [ctypes.c_void_p] * 4
x.XGetClassHint.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_void_p]
class Hint(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char_p), ("cls", ctypes.c_char_p)]
d = x.XOpenDisplay(None)
if not d:
    sys.exit(2)
want = sys.argv[1].encode()
stack = [x.XDefaultRootWindow(d)]
while stack:
    w = stack.pop()
    h = Hint()
    if x.XGetClassHint(d, w, ctypes.byref(h)) and want in (h.name, h.cls):
        sys.exit(0)
    root, parent = ctypes.c_ulong(), ctypes.c_ulong()
    kids, n = ctypes.POINTER(ctypes.c_ulong)(), ctypes.c_uint()
    if x.XQueryTree(d, w, ctypes.byref(root), ctypes.byref(parent), ctypes.byref(kids), ctypes.byref(n)):
        stack.extend(kids[i] for i in range(n.value))
sys.exit(1)' "$1"
}
owned() { as_user busctl --user status "$1"; }

sid=$(loginctl show-user "$u" -p Display --value)
type=$(loginctl show-session "$sid" -p Type --value 2>/dev/null)
echo "sessão $sid tipo=$type wayland=${wl:-nenhum}" >>"$log"
as_user wmake-zones status 2>/dev/null | grep '^monitor=' | cut -d' ' -f1-2 >>"$log"
case $type in
x11 | wayland) ;;
*)
	echo "FALHA 00 sessao: nenhuma sessão gráfica do $u (login não terminou?)" >>"$log"
	echo "fim 0/1" >>"$log"
	exit 1
	;;
esac

check sessao "sessão gráfica ativa ($type)" \
	test "$(loginctl show-session "$sid" -p State --value)" = active
if [ "$type" = wayland ]; then
	check compositor "wlmaker" running wlmaker
	check dock "wlmdock (Dock e lançadores do Clip)" running wlmdock
	check clip "Clip no layout dos docks" \
		sh -c "grep -l Clip $rt/wlmaker-docks-*.plist"
	check bandeja "big-wm-tray (hospedeiro SNI)" \
		sh -c "pgrep -u $u -f 'big-wm-tray' | grep -q ."
else
	check compositor "wmaker --for-real" sh -c "pgrep -u $u -f '^(/usr/bin/)?wmaker --for-real' | grep -q ."
	# Os ladrilhos do Dock e do Clip são janelas internas do wmaker, sem
	# WM_CLASS: o estado que ele carrega e grava (WMState) e um dockapp
	# acoplado (classe DockApp) mostram que estão lá.
	dock_x11() { grep -q '^ *Dock = ' "/home/$u/GNUstep/Defaults/WMState" && x_class DockApp; }
	check dock "Dock no WMState + dockapp acoplado (classe DockApp)" dock_x11
	check clip "Clip no WMState" grep -q '^ *Clip = ' "/home/$u/GNUstep/Defaults/WMState"
	check bandeja "snixembed + wmsystemtray" sh -c "pgrep -u $u -x snixembed && pgrep -u $u -x wmsystemtray"
fi
check dockapps "wmcliphist, wmpulsemixer, controlador do Dock com Abas" \
	sh -c "pgrep -u $u -x wmcliphist && pgrep -u $u -x wmpulsemixer && pgrep -u $u -f tabbed-dock-dockapp"
check microfone "item do microfone (org.bigwindowmaker.MicTray)" owned org.bigwindowmaker.MicTray
check watcher "org.kde.StatusNotifierWatcher" owned org.kde.StatusNotifierWatcher
check notificacoes "notify-send entregue (org.freedesktop.Notifications)" \
	as_user notify-send -t 1500 "bigwm-perf" "funcional"
check portal "xdg-desktop-portal responde" \
	as_user busctl --user get-property org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop \
	org.freedesktop.portal.Settings version
check polkit "agente do polkit" sh -c "pgrep -u $u -f polkit-gnome-authentication-agent-1 | grep -q ."
check audio "pipewire + wireplumber + pactl info" \
	sh -c "pgrep -u $u -x pipewire && pgrep -u $u -x wireplumber && runuser -u $u -- env XDG_RUNTIME_DIR=$rt pactl info"
if [ "$type" = wayland ]; then
	check clipboard "wl-copy/wl-paste ida e volta" \
		sh -c "runuser -u $u -- env XDG_RUNTIME_DIR=$rt WAYLAND_DISPLAY=${wl:-wayland-0} sh -c 'printf bigwm-perf | timeout 5 wl-copy; test \"\$(timeout 5 wl-paste -n)\" = bigwm-perf'"
	check captura "grim: PNG da tela" \
		sh -c "as=\$(mktemp -u /tmp/bwmfXXXX.png); runuser -u $u -- env XDG_RUNTIME_DIR=$rt WAYLAND_DISPLAY=${wl:-wayland-0} timeout 10 grim \$as && test -s \$as && rm -f \$as"
else
	check clipboard "xsel ida e volta" \
		sh -c "runuser -u $u -- env DISPLAY=:0 XAUTHORITY=$xauth sh -c 'printf bigwm-perf | timeout 5 xsel -bi; test \"\$(timeout 5 xsel -bo)\" = bigwm-perf'"
	check captura "scrot: PNG da tela" \
		sh -c "as=\$(mktemp -u /tmp/bwmfXXXX.png); runuser -u $u -- env DISPLAY=:0 XAUTHORITY=$xauth timeout 10 scrot \$as && test -s \$as && rm -f \$as"
fi
# Janela nova: X11 pela _NET_CLIENT_LIST; Wayland pelo wmake-zones windows
# (nativa e, pelo XWayland, GDK_BACKEND=x11).
count_windows() {
	if [ "$type" = wayland ]; then
		as_user wmake-zones windows 2>/dev/null | grep -c Calculator
	else
		as_user xprop -root _NET_CLIENT_LIST | tr ',' '\n' | while read -r w; do
			w=${w##* }
			as_user xprop -id "$w" WM_CLASS 2>/dev/null
		done | grep -ci calculator
	fi
}
window_appears() { # BACKEND
	as_user env GDK_BACKEND="$1" gnome-calculator >/dev/null 2>&1 &
	local pid=$!
	for _ in $(seq 1 40); do
		[ "$(count_windows)" -ge 1 ] && break
		sleep 0.25
	done
	local found
	found=$(count_windows)
	kill "$pid" 2>/dev/null
	wait "$pid" 2>/dev/null
	sleep 0.5
	[ "$found" -ge 1 ]
}
if [ "$type" = wayland ]; then
	check janela-nativa "gnome-calculator Wayland nativo aparece" window_appears wayland
	check janela-xwayland "gnome-calculator pelo XWayland aparece" window_appears x11
else
	check janela-nativa "gnome-calculator aparece na _NET_CLIENT_LIST" window_appears x11
	check xsettings "xsettingsd (tema GTK ao vivo)" running xsettingsd
fi
if [ "$type" = wayland ]; then
	check menu-raiz "RootMenu.plist do wlmaker" test -s "/home/$u/.config/wlmaker/RootMenu.plist"
else
	check menu-raiz "WMRootMenu do Window Maker" test -s "/home/$u/GNUstep/Defaults/WMRootMenu"
fi
check zram "swap em zram ativa" sh -c "swapon --show=NAME --noheadings | grep -q zram"
check coredump "nenhum coredump desde o boot" \
	sh -c "! coredumpctl list --no-pager --since=\"\$(uptime -s)\" 2>/dev/null | grep -q ."
check unidades "nenhuma unidade do usuário com falha" \
	sh -c "test \"\$(runuser -u $u -- env XDG_RUNTIME_DIR=$rt systemctl --user --failed --no-legend | wc -l)\" -eq 0"
check sistema "nenhuma unidade do sistema com falha" \
	sh -c "test \"\$(systemctl --failed --no-legend | wc -l)\" -eq 0"
echo "fim $(grep -c '^ok' "$log")/$n" >>"$log"
