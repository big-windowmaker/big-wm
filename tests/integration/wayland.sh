#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Sessão Wayland (experimental): wlmaker no backend headless, com XWayland e
# duas saídas, usando a configuração padrão do Big WindowMaker.
# Executado por session-test.sh como usuário comum, dentro de dbus-run-session.

# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

export LANG=pt_BR.UTF-8 NO_AT_BRIDGE=1 GSK_RENDERER=cairo
export XDG_CURRENT_DESKTOP=wlmaker XDG_SESSION_DESKTOP=wlmaker
export XDG_RUNTIME_DIR=/tmp/xdg-runtime-$UID
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1
mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"

big-wm-firstrun wayland
check "wayland: configuração padrão aplicada" ".config/wlmaker" test -s "$HOME/.config/wlmaker/Config.plist"
config=$HOME/.config/wlmaker

wlmaker --start_xwayland --root_menu_file="$config/RootMenu.plist" >"$OUT/wayland-wlmaker.log" 2>&1 &
wlpid=$!
socket() { find "$XDG_RUNTIME_DIR" -maxdepth 1 -name 'wayland-[0-9]*' ! -name '*.lock' | head -1; }
# shellcheck disable=SC2317 # chamada indiretamente por wait_for
has_socket() { [ -n "$(socket)" ]; }
if ! wait_for 20 has_socket; then
	record "wayland: compositor" FAIL "socket Wayland não foi criado"
	exit 0
fi
WAYLAND_DISPLAY=$(basename "$(socket)")
export WAYLAND_DISPLAY
sleep 3
check "wayland: compositor" "socket $WAYLAND_DISPLAY" kill -0 "$wlpid"

wayland-info >"$OUT/wayland-info.txt" 2>&1
for global in xdg_wm_base zxdg_decoration_manager_v1 zwlr_layer_shell_v1 ext_session_lock_manager_v1 \
	zwp_idle_inhibit_manager_v1 zwlr_screencopy_manager_v1 zwlr_output_manager_v1 zxdg_output_manager_v1 \
	wl_data_device_manager zwp_primary_selection_device_manager_v1 zwlr_data_control_manager_v1 \
	wp_fractional_scale_manager_v1 wp_viewporter; do
	check "wayland: protocolo $global" "wayland-info" grep -q "'$global'" "$OUT/wayland-info.txt"
done
# Limitações conhecidas do wlmaker 0.8: registradas, nunca dadas como prontas.
for global in xdg_activation_v1 zwp_input_method_manager_v2 zwp_text_input_manager_v3; do
	if grep -q "'$global'" "$OUT/wayland-info.txt"; then
		record "wayland: protocolo $global" PASS "wayland-info"
	else
		record "wayland: protocolo $global" XFAIL "não implementado no wlmaker 0.8 (limitação conhecida)"
	fi
done

outputs=$(wlr-randr 2>/dev/null | grep -c '^[A-Z]')
check "wayland: duas saídas (wlr-output-management)" "$outputs saída(s)" test "$outputs" -eq 2
check "wayland: papel de parede (swaybg, layer-shell)" "processo swaybg" alive swaybg

# Aplicativos: GTK nativo, Qt nativo e X11 pelo XWayland.
GDK_BACKEND=wayland gnome-calculator >"$OUT/wayland-gtk.log" 2>&1 &
QT_QPA_PLATFORM=wayland featherpad >"$OUT/wayland-qt.log" 2>&1 &
sleep 6
check "wayland: aplicativo GTK nativo" "gnome-calculator" alive gnome-calculator
check "wayland: aplicativo Qt nativo" "featherpad" alive featherpad

xsock=$(find /tmp/.X11-unix -name 'X[0-9]*' -user "$(id -u)" 2>/dev/null | head -1)
if [ -n "$xsock" ]; then
	xdisplay=:${xsock##*/X}
	DISPLAY=$xdisplay xeyes >"$OUT/wayland-xeyes.log" 2>&1 &
	sleep 3
	if DISPLAY=$xdisplay xwininfo -root -children 2>/dev/null | grep -q '"xeyes"'; then
		record "wayland: aplicativo X11 via XWayland" PASS "DISPLAY=$xdisplay"
	else
		record "wayland: aplicativo X11 via XWayland" FAIL "xeyes não mapeou em $xdisplay"
	fi
else
	record "wayland: aplicativo X11 via XWayland" FAIL "XWayland não criou socket X11"
fi

# Clipboard e seleção primária.
printf 'big-wm-clipboard' | wl-copy
# shellcheck disable=SC2016 # expandido pelo bash -c
check "wayland: clipboard" "wl-copy/wl-paste" bash -c '[ "$(wl-paste -n)" = big-wm-clipboard ]'
printf 'big-wm-primary' | wl-copy -p
# shellcheck disable=SC2016 # expandido pelo bash -c
check "wayland: seleção primária" "wl-copy -p/wl-paste -p" bash -c '[ "$(wl-paste -p -n)" = big-wm-primary ]'

# Captura de tela por saída (screencopy).
first=$(wlr-randr 2>/dev/null | awk '/^[A-Z]/{print $1; exit}')
grim -o "$first" "$OUT/wayland-desktop.png" 2>"$OUT/wayland-grim.log"
check "wayland: captura de tela (grim)" "wayland-desktop.png" test -s "$OUT/wayland-desktop.png"

# Portal de captura/compartilhamento de tela: o xdg-desktop-portal tem que
# expor ScreenCast usando o backend wlr (wlmaker-portals.conf). Na sessão real
# o PipeWire vem do systemd --user; aqui é iniciado à mão.
pipewire >"$OUT/wayland-pipewire.log" 2>&1 &
sleep 1
wireplumber >"$OUT/wayland-wireplumber.log" 2>&1 &
sleep 2
dbus-update-activation-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP
/usr/lib/xdg-desktop-portal-wlr -l DEBUG >"$OUT/wayland-portal-wlr.log" 2>&1 &
sleep 2
/usr/lib/xdg-desktop-portal -r >"$OUT/wayland-portal.log" 2>&1 &
sleep 4
# O xdg-desktop-portal-wlr só usa wlr-screencopy junto com linux-dmabuf, que o
# wlroots só anuncia com renderizador de GPU; com pixman (software, como neste
# contêiner e em VMs sem GPU) o ScreenCast não pode existir.
if ! grep -q "'zwp_linux_dmabuf_v1'" "$OUT/wayland-info.txt"; then
	record "wayland: portal ScreenCast" SKIP "renderizador por software sem linux-dmabuf; o xdpw exige dmabuf (validar com GPU)"
elif gdbus introspect --session --dest org.freedesktop.portal.Desktop \
	--object-path /org/freedesktop/portal/desktop 2>/dev/null | grep -q 'org.freedesktop.portal.ScreenCast'; then
	record "wayland: portal ScreenCast" PASS "backend wlr"
else
	record "wayland: portal ScreenCast" FAIL "interface ScreenCast ausente (ver wayland-portal.log)"
fi

# Bloqueio de sessão (ext-session-lock): -f só retorna depois de bloquear.
if timeout 10 swaylock -f -c 202a2e >"$OUT/wayland-swaylock.log" 2>&1; then
	sleep 1
	grim -o "$first" "$OUT/wayland-locked.png" 2>/dev/null
	record "wayland: bloqueio de sessão" PASS "swaylock -f"
else
	record "wayland: bloqueio de sessão" FAIL "swaylock não obteve o bloqueio"
fi

# Encerramento limpo do compositor.
kill -TERM "$wlpid"
if wait_for 10 bash -c "! kill -0 $wlpid"; then
	record "wayland: encerramento limpo" PASS "SIGTERM"
else
	record "wayland: encerramento limpo" FAIL "wlmaker não saiu em 10 s"
	kill -KILL "$wlpid" 2>/dev/null
fi
pkill -u "$(id -u)" -f xdg-desktop-portal 2>/dev/null
pkill -u "$(id -u)" -x wireplumber 2>/dev/null
pkill -u "$(id -u)" -x pipewire 2>/dev/null
exit 0
