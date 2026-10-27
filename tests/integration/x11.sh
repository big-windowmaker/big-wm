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
check "x11: Qt com o QTStep (Kvantum)" "kvantum.kvconfig" grep -qx theme=QTStepOpenStep "$HOME/.config/Kvantum/kvantum.kvconfig"
# Menu Aplicativos: gerado sem o diretório do flatpak (o wmmenugen abortava o
# menu inteiro nesse caso), traduzido e com o aplicativo instalado.
big-wm-appmenu --no-cache >"$OUT/x11-appmenu.plist" 2>"$OUT/x11-appmenu.err"
check "x11: menu Aplicativos (big-wm-appmenu)" "x11-appmenu.plist" \
	grep -q '"gtk-launch org.gnome.Calculator"' "$OUT/x11-appmenu.plist"
check "x11: categorias traduzidas no menu" "Acessórios" grep -q '"Acessórios"' "$OUT/x11-appmenu.plist"
# Ícones do tema Window Maker em SVG (big-wm-themes): o Clip, o Netscape no
# Firefox e a pasta no Thunar, todos existentes nos ícones do Window Maker
# (nome simples) ou no disco (caminho).
attributes=$HOME/GNUstep/Defaults/WMWindowAttributes
missing=$(grep -o 'Icon = "[^"]*"' "$attributes" | cut -d'"' -f2 | while read -r icon; do
	case $icon in
	/*) [ -e "$icon" ] || echo "$icon" ;;
	*) [ -e "/usr/share/WindowMaker/Icons/$icon" ] || echo "$icon" ;;
	esac
done)
if grep -q '"big-wm-netscape.png"' "$attributes" && grep -q '"big-wm-folder.png"' "$attributes" &&
	[ -z "$missing" ]; then
	record "x11: ícones do Clip (tema Window Maker em SVG)" PASS "WMWindowAttributes"
else
	record "x11: ícones do Clip (tema Window Maker em SVG)" FAIL "faltam: $missing; $(tr -d '\n' <"$attributes" | head -c 300)"
fi

# Mesmo passo da sessão real: exporta o ambiente gráfico para o D-Bus (sem
# systemd --user aqui, o target não sobe; o importante é a ativação D-Bus).
big-wm-session-ctl start >"$OUT/x11-session-ctl.log" 2>&1

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
	record "x11: dockapp wmix" SKIP "sem placa de som no ambiente de teste"
	check "x11: sem placa de som o wmix não é iniciado" "big-wm-mixer" bash -c "! grep -q 'mixer_alsa_set_channel' '$OUT/x11-wmaker.log'"
fi

# Dock na borda direita: os ladrilhos do Window Maker não têm WM_CLASS, então
# mede-se um dockapp acoplado (wmnet), que fica dentro do ladrilho do Dock.
wmnet_x=$(xwininfo -id "$(xdotool search --classname '^wmnet$' | tail -1)" 2>/dev/null |
	awk '/Absolute upper-left X/{print $NF}')
if [ -n "$wmnet_x" ] && [ "$wmnet_x" -ge $((width - 64)) ]; then
	record "x11: Dock na borda direita" PASS "dockapp acoplado em x=$wmnet_x"
else
	record "x11: Dock na borda direita" FAIL "dockapp wmnet em x=${wmnet_x:-?} (esperado >= $((width - 64)))"
fi

# Janela de aplicativo gerenciada (moldura do Window Maker). O xterm roda
# "sleep" para o prompt do shell não trocar o título.
xterm -geometry 80x20 -T big-wm-teste -e sleep 120 &
if wait_for 10 xdotool search --name '^big-wm-teste$'; then
	id=$(xdotool search --name '^big-wm-teste$' | head -1)
	check "x11: janela gerenciada" "_NET_FRAME_EXTENTS" bash -c "xprop -id $id _NET_FRAME_EXTENTS | grep -q ="
	xdotool windowactivate --sync "$id" 2>/dev/null
	check "x11: foco na janela" "_NET_ACTIVE_WINDOW" bash -c "xprop -root _NET_ACTIVE_WINDOW | grep -qi \"$(printf '0x%x' "$id")\""
else
	record "x11: janela gerenciada" FAIL "xterm não mapeou"
fi

# Atalho global Ctrl+Alt+T (menu raiz) abre o terminal padrão
# (big-wm-open terminal -> xdg-terminal-exec). Com o AshyTerm instalado
# (teste do terminal translúcido) é ele o padrão, não o xterm.
terminals='xterm|ashyterm'
terminals_before=$(pgrep -u "$(id -u)" -c -x "$terminals")
xdotool key ctrl+alt+t
if wait_for 15 bash -c "[ \$(pgrep -u $(id -u) -c -x '$terminals') -gt $terminals_before ]"; then
	record "x11: atalho Ctrl+Alt+T (terminal)" PASS "xdg-terminal-exec: $(pgrep -u "$(id -u)" -n -x "$terminals" -l | cut -d' ' -f2)"
else
	record "x11: atalho Ctrl+Alt+T (terminal)" FAIL "nenhum terminal novo"
fi
# A janela do AshyTerm atrapalharia a comparação do teste do terminal translúcido.
pkill -u "$(id -u)" -x ashyterm

# Menu raiz: os menus do Window Maker não têm nome nem classe, então compara-se
# a região da tela antes e depois do clique direito numa área vazia.
import -window root -crop 300x400+150+300 "$OUT/x11-menu-before.png" 2>/dev/null
xdotool mousemove 150 300 click 3
sleep 1
import -window root -crop 300x400+150+300 "$OUT/x11-menu-after.png" 2>/dev/null
import -window root "$OUT/x11-menu.png" 2>/dev/null
changed=$(compare -metric AE -fuzz 5% "$OUT/x11-menu-before.png" "$OUT/x11-menu-after.png" null: 2>&1 | cut -d' ' -f1)
if [ "${changed%.*}" -gt 5000 ] 2>/dev/null; then
	record "x11: menu raiz" PASS "$changed pixels mudaram (x11-menu.png)"
else
	record "x11: menu raiz" FAIL "a região não mudou após o clique direito ($changed pixels)"
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

# Portal de configurações (backend GTK): é dele que os apps Qt
# (QT_QPA_PLATFORMTHEME=xdg-desktop-portal) e GTK4 leem o esquema de cores.
if gdbus call --session --dest org.freedesktop.portal.Desktop \
	--object-path /org/freedesktop/portal/desktop \
	--method org.freedesktop.portal.Settings.ReadOne org.freedesktop.appearance color-scheme \
	>"$OUT/x11-portal-settings.txt" 2>&1; then
	record "x11: portal Settings (color-scheme)" PASS "$(tr -d '\n' <"$OUT/x11-portal-settings.txt")"
else
	record "x11: portal Settings (color-scheme)" FAIL "$(head -1 "$OUT/x11-portal-settings.txt")"
fi

# Clipboard e seleção primária.
printf 'big-wm-clipboard' | xclip -selection clipboard
# shellcheck disable=SC2016 # expandido pelo bash -c
check "x11: clipboard" "xclip" bash -c '[ "$(xclip -o -selection clipboard)" = big-wm-clipboard ]'
printf 'big-wm-primary' | xclip -selection primary
# shellcheck disable=SC2016 # expandido pelo bash -c
check "x11: seleção primária" "xclip" bash -c '[ "$(xclip -o -selection primary)" = big-wm-primary ]'

# Aplicativos GTK e Qt. O GTK com o gtk-nocsd, como o startbigwm faz.
LD_PRELOAD=/usr/lib/libgtk-nocsd.so.0 gnome-calculator >"$OUT/x11-gtk.log" 2>&1 &
QT_QPA_PLATFORM=xcb featherpad >"$OUT/x11-qt.log" 2>&1 &
sleep 6
check "x11: aplicativo GTK" "gnome-calculator" alive gnome-calculator
check "x11: aplicativo Qt" "featherpad" alive featherpad
# Ícones de reserva (big-wm-appmenu --icons): o arquivo continua válido e
# ganha o ícone de um aplicativo instalado.
big-wm-appmenu --icons >"$OUT/x11-icons.log" 2>&1
check "x11: ícones de reserva em WMWindowAttributes" "gnome-calculator" \
	grep -q '"gnome-calculator" = {Icon = ' "$HOME/GNUstep/Defaults/WMWindowAttributes"
# Com o gtk-nocsd a janela GTK 4/libadwaita pede a decoração do Window Maker
# (_MOTIF_WM_HINTS com decorations != 0); com CSD ela pede 0.
decorated=0
for id in $(xdotool search --class gnome-calculator 2>/dev/null); do
	hints=$(xprop -id "$id" _MOTIF_WM_HINTS 2>/dev/null)
	[[ $hints =~ =\ 0x[0-9a-f]+,\ 0x[0-9a-f]+,\ 0x([0-9a-f]+) ]] && [ "${BASH_REMATCH[1]}" != 0 ] && decorated=1
done
check "x11: janela GTK com a decoração do Window Maker (gtk-nocsd)" "_MOTIF_WM_HINTS" test "$decorated" = 1
import -window root "$OUT/x11-apps.png" 2>/dev/null

# Captura de tela nativa do Window Maker (tecla Print).
xdotool key Print
sleep 2
shots=$(find "$HOME/GNUstep/Library/WindowMaker/Screenshots/" -name '*.png' 2>/dev/null | wc -l)
check "x11: captura de tela (Print)" "$shots arquivo(s)" test "$shots" -ge 1

# Menu Captura de tela > Tela inteira (big-wm-screenshot com o scrot), na
# mesma pasta das teclas do Window Maker.
before=$(find "$HOME/GNUstep/Library/WindowMaker/Screenshots/" -name '*.png' 2>/dev/null | wc -l)
big-wm-screenshot full >"$OUT/x11-big-wm-screenshot.log" 2>&1
after=$(find "$HOME/GNUstep/Library/WindowMaker/Screenshots/" -name '*.png' 2>/dev/null | wc -l)
check "x11: menu Captura de tela (big-wm-screenshot full)" "$before -> $after arquivo(s)" test "$after" -gt "$before"

# Ctrl+Alt+Esc (menu raiz): a caveira do xkill espera o clique; Esc cancela.
xdotool key ctrl+alt+Escape
if wait_for 5 alive xkill; then
	record "x11: Ctrl+Alt+Esc (xkill)" PASS "processo xkill"
else
	record "x11: Ctrl+Alt+Esc (xkill)" FAIL "xkill não iniciou"
fi
pkill -u "$(id -u)" -x xkill

import -window root "$OUT/x11-desktop.png" 2>/dev/null
check "x11: captura do desktop" "x11-desktop.png" test -s "$OUT/x11-desktop.png"

# Compositor (picom com a configuração do Big WindowMaker): uma janela nova
# precisa aparecer desenhada. O caso que motivou o backend xrender (glx com
# llvmpipe deixando janelas invisíveis) só se reproduz em VM: é coberto pelo
# smoke test do build-iso; aqui fica a verificação geral.
pkill -u "$(id -u)" -x xterm
picom --config "$HOME/.config/picom/picom.conf" >"$OUT/x11-picom.log" 2>&1 &
sleep 2
if alive picom; then
	xterm -geometry 40x10+400+300 -bg '#ff0000' -T picom-teste -e sleep 60 &
	sleep 3
	# Cor em 8 bits, independente da profundidade da captura.
	pixel=$(import -window root -crop 1x1+450+350 -depth 8 txt:- 2>/dev/null | tail -1)
	import -window root "$OUT/x11-picom.png" 2>/dev/null
	if grep -qi '#FF0000' <<<"$pixel"; then
		record "x11: compositor (picom) desenha janelas novas" PASS "pixel da janela vermelho"
	else
		record "x11: compositor (picom) desenha janelas novas" FAIL "pixel: ${pixel##* } (x11-picom.png)"
	fi
	pkill -u "$(id -u)" -x xterm
	# shellcheck disable=SC2317,SC2329 # chamada indiretamente por check_translucent_terminal
	x11_shot() { import -window root "$1" 2>/dev/null; }
	check_translucent_terminal "x11: terminal translúcido (AshyTerm + picom)" "$OUT/x11-ashyterm" x11_shot

	# Ladrilhos do Dock e do Clip com o picom no ar: o Window Maker desenha
	# cada ladrilho uma vez só, e o picom só pinta o que recebeu desenho
	# depois de começar a acompanhar a janela (ficavam invisíveis ao entrar
	# na sessão e ao reiniciar o Window Maker). O big-wm-x11-repaint é o
	# ExecStartPost do picom na sessão; aqui, reiniciar o Window Maker com
	# o picom já no ar recria todos os ladrilhos.
	"${BIGWM_X11_REPAINT:-/usr/lib/big-wm/big-wm-x11-repaint}" >"$OUT/x11-repaint.log" 2>&1
	sleep 1
	kill -USR1 "$(pgrep -u "$(id -u)" -f '^wmaker --for-real')"
	sleep 8
	import -window root "$OUT/x11-picom-ladrilhos.png" 2>/dev/null
	# Pixel perto da borda do Clip e do quadrado do Dock: cinza (o do Clip
	# é escuro, ~#383a48), nunca o papel de parede (quase preto).
	# shellcheck disable=SC2317,SC2329 # chamada indiretamente por check
	tile_grey() { # x y
		local rgb
		rgb=$(magick "$OUT/x11-picom-ladrilhos.png" -crop "1x1+$1+$2" -depth 8 \
			-format '%[fx:int(255*r)] %[fx:int(255*g)] %[fx:int(255*b)]' info: 2>/dev/null)
		read -r r g b <<<"$rgb"
		[ -n "$b" ] && ((r > 40 && g > 40 && b > 40 && b - r < 30 && r - b < 30))
	}
	check "x11: Clip visível com o picom (após reiniciar o Window Maker)" "x11-picom-ladrilhos.png" tile_grey 2 40
	check "x11: Dock visível com o picom (após reiniciar o Window Maker)" "x11-picom-ladrilhos.png" tile_grey $((width - 62)) 40
	pkill -u "$(id -u)" -x picom
	pkill -u "$(id -u)" -f big-wm-x11-repaint
else
	record "x11: compositor (picom) desenha janelas novas" FAIL "picom não iniciou (x11-picom.log)"
fi

# Dock com abas (pacote big-wm-tabbed-dock): sobe pelo mesmo Exec do
# autostart, é uma janela do tipo dock com reserva de espaço, troca de aba
# pela linha de comando e desenha na borda inferior.
if command -v big-wm-tabbed-dock >/dev/null; then
	big-wm-tabbed-dock >"$OUT/x11-tabbed-dock.log" 2>&1 &
	if wait_for 15 big-wm-tabbed-dock --print-state; then
		big-wm-tabbed-dock --print-state >"$OUT/x11-tabbed-dock-state.txt" 2>&1
		check "x11: dock com abas (rastreamento EWMH)" "backend=x11" grep -q 'backend=x11' "$OUT/x11-tabbed-dock-state.txt"
		check "x11: dock com abas é _NET_WM_WINDOW_TYPE_DOCK" "xprop" \
			bash -c 'xprop -name "Big Tabbed Dock" _NET_WM_WINDOW_TYPE | grep -q DOCK'
		check "x11: dock com abas reserva a borda inferior" "_NET_WM_STRUT_PARTIAL" \
			bash -c 'xprop -name "Big Tabbed Dock" _NET_WM_STRUT_PARTIAL | grep -q "= 0, 0, 0, [1-9]"'
		check "x11: dock com abas tem as 4 abas (pt_BR)" "Geral/Desenvolvimento/Documentos/Temporário" \
			grep -q '"Desenvolvimento"' "$OUT/x11-tabbed-dock-state.txt"
		big-wm-tabbed-dock --tab 2
		sleep 1
		check "x11: dock com abas troca de aba (--tab)" "developer" \
			bash -c 'big-wm-tabbed-dock --print-state | grep -q "^tab \*developer"'
		import -window root "$OUT/x11-tabbed-dock.png" 2>/dev/null
		big-wm-tabbed-dock --quit
	else
		record "x11: dock com abas" FAIL "não respondeu (x11-tabbed-dock.log)"
	fi
else
	record "x11: dock com abas" SKIP "pacote big-wm-tabbed-dock não instalado"
fi

# Modo escuro com a sessão aberta (big-wm-theme dark): o Window Maker relê o
# estilo na hora (ícone do Dock mais escuro), o Kvantum e o color-scheme
# trocam, e um aplicativo GTK aberto depois sai com o OneStepBack-Dark.
# Volta ao claro no fim.
tile() { magick "$1" -crop 8x8+$((width - 55))+140 -resize '1x1!' -format '%[hex:u]' info: 2>/dev/null; }
import -window root -depth 8 "$OUT/x11-light.png" 2>/dev/null
light_tile=$(tile "$OUT/x11-light.png")
big-wm-theme dark
sleep 3
import -window root -depth 8 "$OUT/x11-dark.png" 2>/dev/null
dark_tile=$(tile "$OUT/x11-dark.png")
check "x11: modo escuro guardado (big-wm-theme get)" "dark" bash -c "big-wm-theme get | grep -qx dark"
check "x11: modo escuro no Window Maker" "BigWindowMaker-Dark" grep -q 'rgb:2e/2e/2e' "$HOME/GNUstep/Defaults/WindowMaker"
if [ -n "$light_tile" ] && [ -n "$dark_tile" ] && [ "$light_tile" != "$dark_tile" ]; then
	record "x11: Window Maker relê o estilo escuro na hora" PASS "ícone do Dock $light_tile -> $dark_tile (x11-dark.png)"
else
	record "x11: Window Maker relê o estilo escuro na hora" FAIL "ícone do Dock '$light_tile' -> '$dark_tile'"
fi
if [ -d /usr/share/Kvantum/QTStepOpenStepDark ]; then
	check "x11: modo escuro no Kvantum" "QTStepOpenStepDark" grep -qx theme=QTStepOpenStepDark "$HOME/.config/Kvantum/kvantum.kvconfig"
fi
check "x11: modo escuro no color-scheme" "prefer-dark" \
	bash -c 'gsettings get org.gnome.desktop.interface color-scheme | grep -q prefer-dark'
if [ -d /usr/share/themes/OneStepBack-Dark/gtk-3.0 ]; then
	check "x11: GTK_THEME escuro para aplicativos novos" "OneStepBack-Dark" \
		bash -c ". /usr/lib/big-wm/big-wm-theme.sh; GTK_THEME=OneStepBack; big_wm_theme_refresh_env; echo \$GTK_THEME | grep -qx OneStepBack-Dark"
fi
big-wm-theme light
sleep 2
check "x11: volta ao modo claro" "BigWindowMaker" grep -q 'rgb:50/5a/5e' "$HOME/GNUstep/Defaults/WindowMaker"

# Bloqueio de tela: big-wm-lock (menu, Super+L e xss-lock) desenha a imagem
# e o i3lock pega teclado e ponteiro.
big-wm-lock >"$OUT/x11-i3lock.log" 2>&1 &
sleep 4
check "x11: bloqueio de tela (big-wm-lock)" "processo i3lock" alive i3lock
check "x11: imagem da tela de bloqueio" "big-wm-lock.png" \
	bash -c "pgrep -u $(id -u) -a i3lock | grep -q -- '-i .*big-wm-lock.png'"
import -window root "$OUT/x11-locked.png" 2>/dev/null
pkill -u "$(id -u)" -x i3lock

pkill -u "$(id -u)" -x gnome-calculator
pkill -u "$(id -u)" -x featherpad
pkill -u "$(id -u)" -x xterm
pkill -u "$(id -u)" -x wmaker
kill %1 2>/dev/null
exit 0
