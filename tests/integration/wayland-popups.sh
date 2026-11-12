# shellcheck shell=bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Menus de contexto no lugar (GTK, Wayland nativo e X11 pelo XWayland): o
# menu abre no ponteiro com a janela recém-aberta, depois de arrastada pela
# barra de título do Window Maker, e perto da borda de baixo da saída ele
# fica inteiro nela (inverte para cima). Antes, no XWayland, um menu ia
# para a posição da janela mais a dele (o do GParted, longe, embaixo à
# direita); no Wayland nativo, desviava pela sombra do menu e passava da
# borda da saída, cortado.
# Carregado com "." pelo wayland.sh, com o wlmaker no ar. O ponteiro é o
# virtual (wl-pointer.py); a janela e o menu, achados na captura (grim).

here=$(dirname "${BASH_SOURCE[0]}")
read -r _pox poy _pow poh pextent <<<"$(wlr-randr --json 2>/dev/null | python3 -I -c 'import json, sys
outs = [o for o in json.load(sys.stdin) if o["enabled"]]
size = lambda o: next(m for m in o["modes"] if m["current"])
o = min(outs, key=lambda o: (o["position"]["y"], o["position"]["x"]))
print(o["position"]["x"], o["position"]["y"], size(o)["width"], size(o)["height"],
      "%d %d" % (max(p["position"]["x"] + size(p)["width"] for p in outs),
                 max(p["position"]["y"] + size(p)["height"] for p in outs)))')"
if [ -z "${pextent:-}" ]; then
	record "wayland: menus de contexto no lugar" SKIP "sem wlr-randr"
	return 0
fi
# shellcheck disable=SC2086 # pextent: largura e altura, duas palavras
pptr() { python3 -I "$here/wl-pointer.py" $pextent "$@" >>"$OUT/wayland-popups-pointer.log" 2>&1; }
# Caixa (LxA+X+Y) da maior área de uma cor na captura, ou nada.
color_box() {
	magick "$1" -fill black +opaque "$2" -fill white -opaque "$2" \
		-define connected-components:verbose=true -connected-components 8 null: 2>/dev/null |
		awk '/srgb\(255,255,255\)/ {split($2, g, "+"); split(g[1], s, "x");
		     if (s[1] * s[2] > best) {best = s[1] * s[2]; box = $2}} END {print box}'
}
box_xy() { local b=$1; b=${b#*+}; echo "${b%%+*} ${b#*+}"; }
box_wh() { local b=${1%%+*}; echo "${b%x*} ${b#*x}"; }
# A janela de teste (420×300, fundo #303030) visível: uma área dessa cor do
# tamanho dela, não um pixel solto do papel de parede ou de um ícone.
# shellcheck disable=SC2317,SC2329 # chamada indiretamente por wait_for
window_up() {
	local b w h
	grim "$OUT/wayland-popups-w.png" 2>/dev/null || return 1
	b=$(color_box "$OUT/wayland-popups-w.png" '#303030')
	[ -n "$b" ] || return 1
	read -r w h <<<"$(box_wh "$b")"
	[ "$w" -ge 300 ] && [ "$h" -ge 200 ]
}

# Clique direito em X,Y; o menu tem de abrir ali (2 px do GTK, folga de 6).
menu_at() { # rótulo, x, y
	pptr click "$2" "$3" right
	sleep 1.2
	grim "$OUT/wayland-popups-$1.png" 2>/dev/null
	local m mx my
	m=$(color_box "$OUT/wayland-popups-$1.png" '#ff00ff')
	pptr click "$(($2 - 40))" "$(($3 - 30))" left # fecha o menu
	sleep 0.5
	read -r mx my <<<"$(box_xy "$m")"
	if [ -n "$m" ] && [ $((mx - $2)) -ge 0 ] && [ $((mx - $2)) -le 6 ] &&
		[ $((my - $3)) -ge 0 ] && [ $((my - $3)) -le 6 ]; then
		record "wayland: menu no ponteiro, $1" PASS "clique $2,$3, menu $m"
	else
		record "wayland: menu no ponteiro, $1" FAIL "clique $2,$3, menu ${m:-não achado}"
	fi
}

# Janelas dos passos anteriores (editor, calculadora) cobririam a de teste.
pkill -u "$(id -u)" -x featherpad 2>/dev/null
pkill -u "$(id -u)" -x gnome-calculator 2>/dev/null
sleep 0.5

backends=wayland
[ -n "${xdisplay:-}" ] && backends="wayland x11"
for b in $backends; do
	DISPLAY=${xdisplay:-} GDK_BACKEND=$b python3 -I "$here/popup-menu.py" "Menu $b" \
		>"$OUT/wayland-popups-$b.log" 2>&1 &
	mpid=$!
	if ! wait_for 15 window_up; then
		record "wayland: menus de contexto ($b)" FAIL "janela de teste não apareceu"
		kill "$mpid" 2>/dev/null
		continue
	fi
	read -r wx wy <<<"$(box_xy "$(color_box "$OUT/wayland-popups-w.png" '#303030')")"
	menu_at "$b, janela nova" $((wx + 120)) $((wy + 90))
	# Arrastada pela barra de título do Window Maker (logo acima do conteúdo).
	pptr drag $((wx + 150)) $((wy - 10)) $((wx + 450)) $((wy + 140))
	sleep 0.8
	grim "$OUT/wayland-popups-w.png" 2>/dev/null
	read -r wx wy <<<"$(box_xy "$(color_box "$OUT/wayland-popups-w.png" '#303030')")"
	menu_at "$b, janela arrastada" $((wx + 120)) $((wy + 90))
	# Perto da borda de baixo da saída: o menu fica inteiro nela.
	read -r _ww wh <<<"$(box_wh "$(color_box "$OUT/wayland-popups-w.png" '#303030')")"
	bottom=$((poy + poh))
	pptr drag $((wx + 150)) $((wy - 10)) $((wx + 150)) $((bottom - wh - 10))
	sleep 0.8
	grim "$OUT/wayland-popups-w.png" 2>/dev/null
	wbox=$(color_box "$OUT/wayland-popups-w.png" '#303030')
	read -r wx wy <<<"$(box_xy "$wbox")"
	read -r _ww wh <<<"$(box_wh "$wbox")"
	cy=$((wy + wh - 15))
	[ "$cy" -ge "$bottom" ] && cy=$((bottom - 15))
	pptr click $((wx + 120)) "$cy" right
	sleep 1.2
	grim "$OUT/wayland-popups-$b-borda.png" 2>/dev/null
	m=$(color_box "$OUT/wayland-popups-$b-borda.png" '#ff00ff')
	pptr click $((wx + 60)) $((wy + 20)) left
	sleep 0.5
	read -r mx my <<<"$(box_xy "$m")"
	read -r _mw mh <<<"$(box_wh "$m")"
	if [ -n "$m" ] && [ $((my + mh)) -le "$bottom" ] && [ "$mh" -ge 100 ]; then
		record "wayland: menu inteiro na saída perto da borda ($b)" PASS "clique $((wx + 120)),$cy, menu $m, borda $bottom"
	else
		record "wayland: menu inteiro na saída perto da borda ($b)" FAIL "clique $((wx + 120)),$cy, menu ${m:-não achado}, borda $bottom"
	fi
	kill "$mpid" 2>/dev/null
	wait "$mpid" 2>/dev/null
done
