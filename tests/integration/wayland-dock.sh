# shellcheck shell=bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Dock e Clip do Wayland como os do Window Maker: arrastar um dockapp do Clip
# para o Dock (com o fantasma do lugar) e de volta, o balão com o nome, o
# menu do botão direito e o "kaboom" de um dockapp solto no meio da tela.
# Carregado com "." pelo wayland.sh, com o wlmaker, o Dock, o Clip e os
# dockapps já no ar. Os arrastos vêm do ponteiro virtual (wl-pointer.py).

here=$(dirname "${BASH_SOURCE[0]}")
if ! grep -qa DockappTiles "$(command -v wlmaker)"; then
	record "wayland: arrastar dockapps entre Clip e Dock" SKIP "wlmaker sem os patches do Dock do Window Maker"
	return 0
fi

# A bandeja (wayland-tray.sh) fica logo depois dos lançadores do Dock, onde
# os dockapps arrastados para o Dock entram: enquanto ela não seguir o
# DockappTiles do State.plist, cobriria o dockapp. Já foi testada; sai.
pkill -u "$(id -u)" -f big-wm-tray 2>/dev/null
sleep 1

state=$HOME/.config/wlmaker/State.plist
dockapps=$HOME/.config/wlmaker/Dockapps.plist
# Saída principal (a de cima e mais à esquerda, onde estão o Clip e o Dock;
# a ordem do wlr-randr não é a das saídas do wlmaker): o Clip no canto
# superior esquerdo, o Dock no direito; ladrilhos de 64 px com 1 px de
# margem (tema do Big WindowMaker).
read -r ox oy ow _oh extent <<<"$(wlr-randr --json 2>/dev/null | python3 -I -c 'import json, sys
outs = [o for o in json.load(sys.stdin) if o["enabled"]]
size = lambda o: next(m for m in o["modes"] if m["current"])
o = min(outs, key=lambda o: (o["position"]["y"], o["position"]["x"]))
print(o["position"]["x"], o["position"]["y"], size(o)["width"], size(o)["height"],
      "%d,%d" % (max(p["position"]["x"] + size(p)["width"] for p in outs),
                 max(p["position"]["y"] + size(p)["height"] for p in outs)))')"
pitch=65
# shellcheck disable=SC2317,SC2329 # chamadas indiretamente por wait_for
dptr() { python3 -I "$here/wl-pointer.py" "${extent%,*}" "${extent#*,}" "$@" >>"$OUT/wayland-dock-pointer.log" 2>&1; }
# shellcheck disable=SC2317,SC2329
in_dock_order() { grep -q "^  Dock = ($1);" "$dockapps" 2>/dev/null; }
# shellcheck disable=SC2317,SC2329
dock_order_empty() { grep -q '^  Dock = ();' "$dockapps" 2>/dev/null; }

clock=wlmclock
command -v wlmcalclock >/dev/null && clock=wlmcalclock
# Clip: o Clip, os 2 lançadores e então os dockapps, o relógio primeiro.
cx=$((ox + 2)) cy=$((oy + 3 * pitch + 32))
dx=$((ox + ow - 40)) dy=$((oy + 3 * pitch + 32))

# 1. Do Clip para o Dock, pela borda do ladrilho: o fantasma aparece no
#    primeiro lugar depois dos lançadores do Dock.
dptr drag "$cx" "$cy" "$dx" "$dy" 1.2 &
sleep 1.3
grim "$OUT/wayland-dock-fantasma.png" 2>/dev/null
wait $!
check "wayland: dockapp arrastado do Clip para o Dock" "Dockapps.plist: Dock = ($clock)" \
	wait_for 5 in_dock_order "$clock"
check "wayland: número de dockapps do Dock no State.plist (DockappTiles)" "State.plist" \
	grep -q 'DockappTiles = 1;' "$state"
grim "$OUT/wayland-dock-dockapp-no-dock.png" 2>/dev/null

# 2. Balão com o nome, sobre o dockapp no Dock (depois do atraso de 1 s).
dptr move $((ox + ow - 32)) "$dy"
sleep 1.5
grim "$OUT/wayland-dock-balao.png" 2>/dev/null
record "wayland: balão do dockapp" PASS "captura wayland-dock-balao.png (conferir a imagem)"

# 3. Menu do botão direito de um lançador do Dock.
dptr click $((ox + ow - 32)) $((oy + pitch + 32)) right
sleep 0.5
grim "$OUT/wayland-dock-menu.png" 2>/dev/null
dptr click $((ox + ow / 2)) $((oy + 400)) left
record "wayland: menu do lançador do Dock" PASS "captura wayland-dock-menu.png (conferir a imagem)"

# 4. De volta para o Clip, depois dos lançadores.
dptr drag $((ox + ow - 62)) "$dy" "$cx" "$cy" 0.5
check "wayland: dockapp arrastado de volta para o Clip" "Dockapps.plist: Dock = ()" \
	wait_for 5 dock_order_empty
check "wayland: DockappTiles volta a 0" "State.plist" wait_for 3 grep -q 'DockappTiles = 0;' "$state"

# 5. Um dockapp solto no meio da tela explode e é encerrado; não inicia
#    mais com a sessão (Removed). O relógio, de volta ao primeiro lugar.
victim=$clock
if alive "$victim"; then
	dptr drag "$cx" "$cy" $((ox + ow / 2)) $((oy + 300)) 0.3 &
	sleep 0.9
	grim "$OUT/wayland-dock-kaboom.png" 2>/dev/null
	wait $!
	# shellcheck disable=SC2016 # expandido pelo bash -c
	check "wayland: dockapp solto fora explode e é encerrado" "$victim" \
		wait_for 5 bash -c '! pgrep -u "$(id -u)" -x "$0" >/dev/null' "$victim"
	check "wayland: dockapp tirado não inicia com a sessão (Removed)" "Dockapps.plist" \
		grep -q "Removed = ($victim)" "$dockapps"
else
	record "wayland: dockapp solto fora explode" SKIP "$victim não está rodando"
fi
