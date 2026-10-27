# shellcheck shell=bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Bandeja do Wayland (big-wm-tray) como o wmsystemtray do X11: ladrilho de
# 64×64 com quatro ícones e a paginação "<- 1/2 ->". Carregado com "." pelo
# wayland.sh, com o wlmaker e o Dock (wlmdock) já no ar. Os itens vêm do
# fake-sni.py e os cliques, do ponteiro virtual do compositor (wl-pointer.py).

here=$(dirname "${BASH_SOURCE[0]}")
tray=${BIGWM_TRAY:-big-wm-tray}
# shellcheck disable=SC2086 # BIGWM_TRAY pode ser "python3 -I caminho"
traystate() { $tray --state 2>/dev/null; }
trayline() { traystate | grep -m1 "^$1 " | cut -d' ' -f2-; }
# shellcheck disable=SC2317,SC2329 # chamadas indiretamente por wait_for
trayhas() { traystate | grep -qF -- "$1"; }
# shellcheck disable=SC2317,SC2329
trayhasre() { traystate | grep -q -- "$1"; }

# shellcheck disable=SC2086
$tray >"$OUT/wayland-tray.log" 2>&1 &
traypid=$!
check "wayland: bandeja SNI (big-wm-tray)" "org.kde.StatusNotifierWatcher" \
	wait_for 10 bash -c "gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
		--method org.freedesktop.DBus.NameHasOwner org.kde.StatusNotifierWatcher | grep -q true"
wait_for 10 traystate
check "wayland: bandeja sem ícones mostra 1/1" "$(trayline page)" test "$(trayline page)" = 1/1
# shellcheck disable=SC2086
timeout 10 $tray >"$OUT/wayland-tray-2.log" 2>&1
check "wayland: bandeja é única na sessão" "segunda instância sai" grep -q "já está rodando" "$OUT/wayland-tray-2.log"

python3 -I "$here/fake-sni.py" 6 "$OUT/wayland-sni" >"$OUT/wayland-sni.log" 2>&1 &
snipid=$!
# shellcheck disable=SC2317,SC2329 # chamada indiretamente por wait_for
tray_loaded() { [ "$(traystate | grep -c '^item .* \(shown\|other-page\|passive\) ')" -eq 6 ]; }
wait_for 15 tray_loaded
traystate >"$OUT/wayland-tray-state-1.txt"
check "wayland: bandeja com 5 itens ativos e 4 por página: 1/2" "$(trayline page)" test "$(trayline page)" = 1/2
check "wayland: bandeja ícone pelo nome do tema" "Item0" grep -q 'Item0 fake-0 shown name:network-wired' "$OUT/wayland-tray-state-1.txt"
check "wayland: bandeja ícone só com IconPixmap" "Item1" grep -q 'Item1 fake-1 shown pixmap:22x22' "$OUT/wayland-tray-state-1.txt"
check "wayland: bandeja ícone do IconThemePath" "Item2" grep -q 'Item2 fake-2 shown file:.*fake-sni-custom.png' "$OUT/wayland-tray-state-1.txt"
check "wayland: bandeja ícone de atenção (NeedsAttention)" "Item3" grep -q 'Item3 fake-3 shown name:dialog-warning' "$OUT/wayland-tray-state-1.txt"
check "wayland: bandeja esconde item Passive" "Item4" grep -q 'Item4 fake-4 passive' "$OUT/wayland-tray-state-1.txt"
check "wayland: bandeja quinto item na página 2" "Item5" grep -q 'Item5 fake-5 other-page' "$OUT/wayland-tray-state-1.txt"

# Ponteiro virtual: coordenadas na área de todas as saídas.
read -r _ ox oy _ _ <<<"$(traystate | grep -m1 '^output ')"
read -r _ tx ty _ _ <<<"$(traystate | grep -m1 '^tile ')"
tx=$((ox + tx)) ty=$((oy + ty))
extent=$(wlr-randr --json 2>/dev/null | python3 -I -c 'import json, sys
outs = [o for o in json.load(sys.stdin) if o["enabled"]]
size = lambda o: next(m for m in o["modes"] if m["current"])
print(max(o["position"]["x"] + size(o)["width"] for o in outs), max(o["position"]["y"] + size(o)["height"] for o in outs))')
# shellcheck disable=SC2086 # largura e altura
ptr() { python3 -I "$here/wl-pointer.py" $extent "$@" >>"$OUT/wayland-tray-pointer.log" 2>&1; }
pageis() { [ "$(trayline page)" = "$1" ]; }

grim "$OUT/wayland-tray-pagina1.png" 2>/dev/null
ptr click $((tx + 52)) $((ty + 56)) left # seta da direita
check "wayland: bandeja seta da direita vai para 2/2" "$(trayline page)" wait_for 3 pageis 2/2
grim "$OUT/wayland-tray-pagina2.png" 2>/dev/null
ptr click $((tx + 52)) $((ty + 56)) left # apagada na última página
check "wayland: bandeja seta da direita parada na última página" "$(trayline page)" pageis 2/2
ptr click $((tx + 10)) $((ty + 56)) left # seta da esquerda
check "wayland: bandeja seta da esquerda volta a 1/2" "$(trayline page)" wait_for 3 pageis 1/2
ptr wheel $((tx + 30)) $((ty + 56)) down
check "wayland: bandeja roda sobre a barra avança a página" "$(trayline page)" wait_for 3 pageis 2/2
kill -USR2 "$traypid"
check "wayland: bandeja SIGUSR2 dá a volta para 1/2" "$(trayline page)" wait_for 3 pageis 1/2

# Ícones: esquerdo, do meio, direito e roda vão para o item (fake-0, no
# primeiro lugar da página 1).
ptr click $((tx + 20)) $((ty + 16)) left
ptr click $((tx + 20)) $((ty + 16)) middle
ptr click $((tx + 20)) $((ty + 16)) right
ptr wheel $((tx + 20)) $((ty + 16)) up
sleep 1
for call in "Activate Item0" "SecondaryActivate Item0" "ContextMenu Item0" "Scroll Item0 120 vertical"; do
	check "wayland: bandeja repassa ${call%% *}" "calls.log" grep -q "^$call" "$OUT/wayland-sni/calls.log"
done

# Balão do Window Maker sobre o ícone: o ToolTip do item (o clique esconde
# o balão; ele volta com o movimento, depois de BALLOON_DELAY).
ptr move $((tx + 22)) $((ty + 18))
check "wayland: bandeja balão com o nome do item" "$(trayline balloon)" \
	wait_for 4 trayhas 'balloon Rede falsa\nConectado'
grim "$OUT/wayland-tray-balao.png" 2>/dev/null

# Modo escuro (big-wm-theme): o ladrilho segue na hora, com o Tile do
# Theme-Dark.plist e o número da página claro.
mkdir -p "$HOME/.config/big-wm"
printf 'dark\n' >"$HOME/.config/big-wm/theme.new" && mv -f "$HOME/.config/big-wm/theme.new" "$HOME/.config/big-wm/theme"
check "wayland: bandeja segue o modo escuro" "$(trayline theme)" \
	wait_for 8 trayhas "tile_fill DGRADIENT #5c5c64"
# E na tela: o fundo do ladrilho (perto da borda esquerda) fica escuro.
# shellcheck disable=SC2317,SC2329 # chamada indiretamente por wait_for
tile_dark() {
	grim "$OUT/wayland-tray-escuro.png" 2>/dev/null &&
		[ "$(magick "$OUT/wayland-tray-escuro.png" -crop "1x1+$((tx + 5))+$((ty + 30))" \
			-format '%[fx:int(255*luma)]' info: 2>/dev/null)" -lt 120 ]
}
check "wayland: bandeja redesenha o ladrilho escuro" "wayland-tray-escuro.png" wait_for 5 tile_dark
printf 'light\n' >"$HOME/.config/big-wm/theme"
wait_for 8 trayhas "tile_fill DGRADIENT #a6a6b6"

# Item Passive que vira Active (NewStatus): entra na página 2.
kill -USR1 "$snipid"
check "wayland: bandeja mostra item que deixou de ser Passive" "Item4" \
	wait_for 5 trayhasre 'Item4 fake-4 \(shown\|other-page\)'

# Botão direito fora dos ícones: menu do ladrilho (Configurações..., Terminar).
ptr click $((tx + 2)) $((ty + 30)) right
check "wayland: bandeja menu do ladrilho no botão direito" "$(trayline menu)" wait_for 3 trayhas "menu frame"
grim "$OUT/wayland-tray-menu.png" 2>/dev/null
ptr click $((tx - 300)) $((ty + 300)) left # fecha o menu

# Memória com 6 itens (o ladrilho é um só DrawingArea).
record "wayland: bandeja memória (RSS, 6 itens)" PASS "$(awk '/^VmRSS/ {print $2, $3}' "/proc/$traypid/status")"

kill "$snipid" 2>/dev/null
# shellcheck disable=SC2317,SC2329 # chamada indiretamente por wait_for
tray_empty() { [ "$(trayline page)" = 1/1 ] && ! traystate | grep -q '^item '; }
check "wayland: bandeja tira os itens de quem saiu do barramento" "1/1" wait_for 5 tray_empty
