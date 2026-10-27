#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Sessão Wayland (experimental): wlmaker no backend headless, com XWayland e
# duas saídas, usando a configuração padrão do Big WindowMaker.
# Executado por session-test.sh como usuário comum, dentro de dbus-run-session.

# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

export LANG=pt_BR.UTF-8 NO_AT_BRIDGE=1 GSK_RENDERER=cairo
export XDG_CURRENT_DESKTOP=wlmaker XDG_SESSION_DESKTOP=wlmaker XDG_SESSION_TYPE=wayland
export XDG_RUNTIME_DIR=/tmp/xdg-runtime-$UID
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1
mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"

big-wm-firstrun wayland
check "wayland: configuração padrão aplicada" ".config/wlmaker" test -s "$HOME/.config/wlmaker/Config.plist"
config=$HOME/.config/wlmaker

# Como o startbigwm-wayland: o que foi configurado no Window Maker (a sessão
# X11 acabou de rodar com esta conta) passa para o wlmaker antes de ele ler
# a configuração.
big-wm-wprefs-sync --no-reload >"$OUT/wayland-wprefs-sync.log" 2>&1
check "wayland: big-wm-wprefs-sync no início da sessão" "wayland-wprefs-sync.log" \
	test -s "$config/.wprefs-sync.json"
wlargs=(--start_xwayland "--root_menu_file=$config/RootMenu.plist")
if [ -e "$config/Theme.plist" ] && ! head -c 300 "$config/Theme.plist" | grep -q 'Gerado pelo big-wm-wprefs-sync'; then
	wlargs+=("--theme_file=$config/Theme.plist")
fi
wlmaker "${wlargs[@]}" >"$OUT/wayland-wlmaker.log" 2>&1 &
wlpid=$!
socket() { find "$XDG_RUNTIME_DIR" -maxdepth 1 -name 'wayland-[0-9]*' ! -name '*.lock' | head -1; }
# shellcheck disable=SC2317,SC2329 # chamada indiretamente por wait_for
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
	wp_fractional_scale_manager_v1 wp_viewporter \
	xdg_activation_v1 org_kde_kwin_server_decoration_manager ext_idle_notifier_v1 \
	zxdg_exporter_v2 zxdg_importer_v2 wp_presentation wp_single_pixel_buffer_manager_v1 \
	zwlr_output_power_manager_v1 zwlr_gamma_control_manager_v1 zwp_relative_pointer_manager_v1 \
	zwp_virtual_keyboard_manager_v1 zwlr_virtual_pointer_manager_v1 zwlr_foreign_toplevel_manager_v1; do
	check "wayland: protocolo $global" "wayland-info" grep -q "'$global'" "$OUT/wayland-info.txt"
done
# Limitações conhecidas do wlmaker: registradas, nunca dadas como prontas.
for global in zwp_input_method_manager_v2 zwp_text_input_manager_v3 zwp_pointer_constraints_v1; do
	if grep -q "'$global'" "$OUT/wayland-info.txt"; then
		record "wayland: protocolo $global" PASS "wayland-info"
	else
		record "wayland: protocolo $global" XFAIL "não implementado no wlmaker (limitação conhecida)"
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

# Forçar o fechamento (Ctrl+Alt+Esc): as janelas vêm do wlr-foreign-toplevel
# e a calculadora aberta tem o PID encontrado (nome no D-Bus).
big-wm-kill --list >"$OUT/wayland-kill-list.txt" 2>&1
check "wayland: Forçar o fechamento lista as janelas (big-wm-kill)" "wayland-kill-list.txt" \
	grep -qE $'\torg\\.gnome\\.Calculator\t.*\t[0-9]+$' "$OUT/wayland-kill-list.txt"

# WPrefs -> wlmaker com a sessão aberta: muda o estilo, um atalho e o foco
# no domínio WindowMaker (com o wdwrite, como o WPrefs grava), sincroniza e
# o wlmaker recarrega (SIGHUP) sem reiniciar: a barra de título fica
# vermelha na captura.
# Todas as saídas: as janelas podem estar em qualquer uma.
grim "$OUT/wayland-wprefs-before.png" 2>/dev/null
wdwrite WindowMaker FTitleBack '(solid, "#c03020")'
wdwrite WindowMaker UTitleBack '(solid, "#c03020")'
wdwrite WindowMaker FTitleColor '"#ffe060"'
wdwrite WindowMaker TitleJustify center
wdwrite WindowMaker CloseKey '"Mod4+q"'
wdwrite WindowMaker FocusMode sloppy
big-wm-wprefs-sync >>"$OUT/wayland-wprefs-sync.log" 2>&1
sleep 2
grim "$OUT/wayland-wprefs-after.png" 2>/dev/null
check "wayland: WPrefs -> Theme.plist (barra de título)" "Theme.plist" \
	grep -q 'argb32:ffc03020' "$config/Theme.plist"
check "wayland: WPrefs -> atalho (CloseKey Super+Q)" "Config.plist" \
	grep -q '"Logo+q" = WindowClose;' "$config/Config.plist"
check "wayland: WPrefs -> foco (FocusMode)" "Config.plist" grep -q 'FocusMode = Sloppy;' "$config/Config.plist"
if grep -qa ReloadConfiguration "$(command -v wlmaker)"; then
	# shellcheck disable=SC2016 # expandido pelo bash -c
	check "wayland: wlmaker recarrega a configuração (SIGHUP)" "wayland-wlmaker.log" \
		bash -c 'grep -q "Reloading configuration" "$0" "$1"/wlmaker.log 2>/dev/null' \
		"$OUT/wayland-wlmaker.log" "${XDG_CACHE_HOME:-$HOME/.cache}"
	check "wayland: compositor vivo depois de recarregar" "processo $wlpid" kill -0 "$wlpid"
	red=$(magick "$OUT/wayland-wprefs-after.png" -fill black -fuzz 4% +opaque "#c03020" \
		-fill white -opaque "#c03020" -format '%[fx:round(mean*w*h)]' info: 2>/dev/null)
	if [ "${red:-0}" -gt 2000 ]; then
		record "wayland: estilo do WPrefs aplicado na hora" PASS "$red pixels da cor nova (wayland-wprefs-after.png)"
	else
		record "wayland: estilo do WPrefs aplicado na hora" FAIL "${red:-0} pixels da cor nova (wayland-wprefs-after.png)"
	fi
else
	record "wayland: wlmaker recarrega a configuração (SIGHUP)" SKIP "wlmaker sem o patch de recarga: vale no próximo login"
fi

# Notificações (dunst também roda em Wayland, via layer-shell).
big-wm-notifyd >"$OUT/wayland-dunst.log" 2>&1 &
if wait_for 10 alive dunst; then
	notify-send "Big WindowMaker" "Teste de notificação"
	sleep 1
	# shellcheck disable=SC2016 # expandido pelo bash -c
	check "wayland: notificações (dunst)" "dunstctl count displayed >= 1" \
		bash -c '[ "$(dunstctl count displayed)" -ge 1 ]'
else
	record "wayland: notificações (dunst)" FAIL "dunst não iniciou"
fi

# Menu raiz igual ao da sessão X11: os submenus gerados pelo big-wm-appmenu
# precisam carregar (o wlmaker descarta o menu inteiro se um falhar).
check "wayland: menu raiz carregado (GeneratePlistMenu)" "wayland-wlmaker.log" \
	bash -c "! grep -qiE 'Failed to (generate|load|create).*menu|Unknown menu definition' '$OUT/wayland-wlmaker.log'"
big-wm-appmenu --wlmaker --no-cache >"$OUT/wayland-appmenu.plist" 2>&1
check "wayland: menu Aplicativos (big-wm-appmenu --wlmaker)" "wayland-appmenu.plist" \
	grep -q '"gtk-launch org.gnome.Calculator"' "$OUT/wayland-appmenu.plist"

# Dockapps nativos do wlmaker.
big-wm-dockapps >"$OUT/wayland-dockapps.log" 2>&1 &
sleep 3
# Relógio padrão: wlmcalclock (relógio + calendário); wlmclock sem ele.
clock=wlmclock
command -v wlmcalclock >/dev/null && clock=wlmcalclock
# Sem dispositivo apontador (WLR_LIBINPUT_NO_DEVICES) os dockapps também têm
# que ficar (wlmaker patch 0016; antes saíam no create_pointer_observer).
check "wayland: dockapps (relógio)" "processo $clock" alive "$clock"

# Dock e Clip (wlmdock), como os autostarts big-wm-wayland-dock e -clip.
WLR_RENDERER=pixman wlmdock >"$OUT/wayland-wlmdock.log" 2>&1 &
dockpid=$!
WLR_RENDERER=pixman wlmdock --clip >"$OUT/wayland-wlmdock-clip.log" 2>&1 &
clippid=$!
sleep 3
check "wayland: Dock (wlmdock)" "processo $dockpid" kill -0 "$dockpid"
check "wayland: Clip (wlmdock --clip)" "processo $clippid" kill -0 "$clippid"

# Bandeja (StatusNotifierItem) depois dos lançadores do Dock, como o
# wmsystemtray: paginação, cliques, balão e menu do ladrilho.
# shellcheck source=wayland-tray.sh
. "$(dirname "$0")/wayland-tray.sh"

# Dock e Clip como os do Window Maker: arrastar, fantasma, balão, menu e
# kaboom.
# shellcheck source=wayland-dock.sh
. "$(dirname "$0")/wayland-dock.sh"

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
# shellcheck disable=SC2317,SC2329 # chamada indiretamente por check_translucent_terminal
wayland_shot() { grim "$1" 2>/dev/null; } # todas as saídas: a janela pode abrir em qualquer uma
check_translucent_terminal "wayland: terminal translúcido (AshyTerm)" "$OUT/wayland-ashyterm" wayland_shot

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

# Bloqueio de sessão (ext-session-lock) pelo big-wm-lock, com a imagem do
# Big WindowMaker: o swaylock -f só retorna depois de bloquear.
if timeout 20 big-wm-lock >"$OUT/wayland-swaylock.log" 2>&1 && alive swaylock; then
	sleep 1
	grim -o "$first" "$OUT/wayland-locked.png" 2>/dev/null
	if pgrep -u "$(id -u)" -a swaylock | grep -q -- '-i .*big-wm-lock.png'; then
		record "wayland: bloqueio de sessão" PASS "big-wm-lock (swaylock com a imagem)"
	else
		record "wayland: bloqueio de sessão" FAIL "swaylock sem a imagem do big-wm-lock"
	fi
else
	record "wayland: bloqueio de sessão" FAIL "swaylock não obteve o bloqueio"
fi

# Dock com abas no Wayland: cliente nativo (layer-shell), sem X11,
# rastreando janelas pelo ext-foreign-toplevel-list do wlmaker.
if command -v big-wm-tabbed-dock >/dev/null; then
	GDK_BACKEND=wayland big-wm-tabbed-dock >"$OUT/wayland-tabbed-dock.log" 2>&1 &
	if wait_for 15 big-wm-tabbed-dock --print-state; then
		big-wm-tabbed-dock --print-state >"$OUT/wayland-tabbed-dock-state.txt" 2>&1
		check "wayland: dock com abas em layer-shell" "layer_shell=1" \
			grep -q 'session=wayland layer_shell=1' "$OUT/wayland-tabbed-dock-state.txt"
		check "wayland: dock com abas rastreia janelas (foreign-toplevel)" "backend" \
			grep -q 'backend=wayland-.*foreign-toplevel' "$OUT/wayland-tabbed-dock-state.txt"
		check "wayland: dock com abas vê a calculadora aberta" "org.gnome.Calculator" \
			grep -q 'app_id=org.gnome.Calculator' "$OUT/wayland-tabbed-dock-state.txt"
		# O x11.sh usa a mesma conta e deixou a aba Desenvolvimento ativa: a
		# configuração é a mesma nas duas sessões.
		check "wayland: dock com abas lembra a aba escolhida na sessão X11" "developer" \
			grep -q '^tab \*developer' "$OUT/wayland-tabbed-dock-state.txt"
		big-wm-tabbed-dock --tab 1
		big-wm-tabbed-dock --tab next
		sleep 1
		check "wayland: dock com abas troca de aba (--tab 1, --tab next)" "developer" \
			bash -c 'big-wm-tabbed-dock --print-state | grep -q "^tab \*developer"'
		grim -o "$first" "$OUT/wayland-tabbed-dock.png" 2>/dev/null
		big-wm-tabbed-dock --quit
	else
		record "wayland: dock com abas" FAIL "não respondeu (wayland-tabbed-dock.log)"
	fi
else
	record "wayland: dock com abas" SKIP "pacote big-wm-tabbed-dock não instalado"
fi

# Encerramento limpo do compositor.
kill -TERM "$wlpid"
if wait_for 10 bash -c "! kill -0 $wlpid"; then
	record "wayland: encerramento limpo" PASS "SIGTERM"
else
	record "wayland: encerramento limpo" FAIL "wlmaker não saiu em 10 s"
	kill -KILL "$wlpid" 2>/dev/null
fi
# Ao sair da sessão o Dock e o Clip saem sozinhos, sem abortar (SIGABRT do
# wlmdock no logout, wlmaker patches 0033 e 0045).
for pair in "Dock:$dockpid" "Clip:$clippid"; do
	if wait_for 10 bash -c "! kill -0 ${pair#*:}"; then
		wait "${pair#*:}"
		status=$?
		if [ "$status" -eq 0 ]; then
			record "wayland: ${pair%%:*} sai junto com o compositor" PASS "status 0"
		else
			record "wayland: ${pair%%:*} sai junto com o compositor" FAIL "status $status (wayland-wlmdock*.log)"
		fi
	else
		record "wayland: ${pair%%:*} sai junto com o compositor" FAIL "ainda rodando 10 s depois"
		kill -KILL "${pair#*:}" 2>/dev/null
	fi
done
pkill -u "$(id -u)" -f xdg-desktop-portal 2>/dev/null
pkill -u "$(id -u)" -x wireplumber 2>/dev/null
pkill -u "$(id -u)" -x dunst 2>/dev/null
pkill -u "$(id -u)" -f big-wm-tray 2>/dev/null
pkill -u "$(id -u)" -x swaylock 2>/dev/null
pkill -u "$(id -u)" -x pipewire 2>/dev/null
exit 0
