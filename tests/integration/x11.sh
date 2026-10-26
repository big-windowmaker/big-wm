#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Sessão X11: Xvfb + Window Maker com a configuração padrão do Big WindowMaker.
# Executado por session-test.sh como usuário comum, dentro de dbus-run-session
# (OUT = diretório de saída).

# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

export LANG=pt_BR.UTF-8 XDG_CURRENT_DESKTOP=WindowMaker NO_AT_BRIDGE=1 GSK_RENDERER=cairo
width=1600 height=900

Xvfb :1 -screen 0 ${width}x${height}x24 >"$OUT/x11-xvfb.log" 2>&1 &
export DISPLAY=:1
wait_for 15 xwininfo -root || { record "x11: Xvfb" FAIL "não iniciou"; exit 0; }

big-wm-firstrun x11
check "x11: configuração padrão aplicada" "GNUstep/Defaults" test -s "$HOME/GNUstep/Defaults/WindowMaker"
check "x11: menu raiz no idioma (pt_BR)" "WMRootMenu" grep -q Aplicativos "$HOME/GNUstep/Defaults/WMRootMenu"

wmaker >"$OUT/x11-wmaker.log" 2>&1 &
big-wm-notifyd >"$OUT/x11-dunst.log" 2>&1 &
wait_for 20 alive wmaker && sleep 10
check "x11: wmaker em execução" "processo wmaker" alive wmaker

# Avisos de configuração do Window Maker (sintaxe de plist, domínios).
if grep -E 'wmaker\(.*\): (warning|error)' "$OUT/x11-wmaker.log" >"$OUT/x11-wmaker-warnings.txt"; then
	record "x11: configuração sem avisos" FAIL "$(head -1 "$OUT/x11-wmaker-warnings.txt")"
else
	record "x11: configuração sem avisos" PASS "x11-wmaker.log"
fi

windows=$(xwininfo -root -children 2>/dev/null)
for pair in "wmclock WMClock" "wmsystemtray0 wmsystemtray" "wmcpuload DockApp" "wmmemload DockApp" "wmnet WMNET"; do
	read -r instance class <<<"$pair"
	if grep -q "(\"$instance\" \"$class\")" <<<"$windows"; then
		record "x11: dockapp $instance" PASS "janela $instance.$class"
	else
		record "x11: dockapp $instance" FAIL "janela $instance.$class ausente"
	fi
done
if [ -e /dev/snd/controlC0 ]; then
	check "x11: dockapp wmix" "janela wmix" grep -q '("wmix" "wmix")' <<<"$windows"
else
	record "x11: dockapp wmix" SKIP "sem placa de som no ambiente de teste (wmix aborta sem dispositivo ALSA)"
fi

# Dock na borda direita: o ladrilho do logo (Logo.WMDock) encosta em x = largura - 64.
dock=$(xdotool search --classname '^Logo$' 2>/dev/null | while read -r id; do
	xprop -id "$id" WM_CLASS 2>/dev/null | grep -q '"WMDock"' && echo "$id"
done | head -1)
if [ -n "$dock" ]; then
	x=$(xwininfo -id "$dock" | awk '/Absolute upper-left X/{print $NF}')
	if [ "$x" -ge $((width - 64)) ]; then
		record "x11: Dock na borda direita" PASS "x=$x"
	else
		record "x11: Dock na borda direita" FAIL "x=$x (esperado >= $((width - 64)))"
	fi
else
	record "x11: Dock na borda direita" FAIL "ladrilho Logo.WMDock não encontrado"
fi

# Janela de aplicativo gerenciada (moldura do Window Maker).
xterm -geometry 80x20 -title big-wm-teste &
if wait_for 10 xdotool search --name '^big-wm-teste$'; then
	id=$(xdotool search --name '^big-wm-teste$' | head -1)
	check "x11: janela gerenciada" "_NET_FRAME_EXTENTS" bash -c "xprop -id $id _NET_FRAME_EXTENTS | grep -q ="
	xdotool windowactivate --sync "$id" 2>/dev/null
	check "x11: foco na janela" "_NET_ACTIVE_WINDOW" bash -c "xprop -root _NET_ACTIVE_WINDOW | grep -qi \"$(printf '0x%x' "$id")\""
else
	record "x11: janela gerenciada" FAIL "xterm não mapeou"
fi

# Menu raiz: clique direito numa área vazia abre uma janela de menu.
before=$(xwininfo -root -children | grep -c '^ *0x')
xdotool mousemove 150 300 click 3
sleep 1
after=$(xwininfo -root -children | grep -c '^ *0x')
import -window root "$OUT/x11-menu.png" 2>/dev/null
if [ "$after" -gt "$before" ]; then
	record "x11: menu raiz" PASS "x11-menu.png"
else
	record "x11: menu raiz" FAIL "nenhuma janela nova após o clique direito"
fi
xdotool key Escape
xdotool mousemove 800 500 click 1

# Notificações: o dunst do Big WindowMaker recebe e exibe.
if wait_for 10 alive dunst; then
	notify-send "Big WindowMaker" "Teste de notificação"
	sleep 1
	# shellcheck disable=SC2016 # expandido pelo bash -c
	check "x11: notificações (dunst)" "dunstctl count displayed >= 1" \
		bash -c '[ "$(dunstctl count displayed)" -ge 1 ]'
else
	record "x11: notificações (dunst)" FAIL "dunst não iniciou"
fi

# Captura de tela nativa do Window Maker (tecla Print).
xdotool key Print
sleep 2
shots=$(find "$HOME/GNUstep/Library/WindowMaker/Screenshots/" -name '*.png' 2>/dev/null | wc -l)
check "x11: captura de tela (Print)" "$shots arquivo(s)" test "$shots" -ge 1

import -window root "$OUT/x11-desktop.png" 2>/dev/null
check "x11: captura do desktop" "x11-desktop.png" test -s "$OUT/x11-desktop.png"

pkill -u "$(id -u)" -x xterm
pkill -u "$(id -u)" -x wmaker
kill %1 2>/dev/null
exit 0
