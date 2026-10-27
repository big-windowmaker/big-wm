#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Variantes Wayland: auto-ocultar, borda superior, estilo moderno, preferências, coexistência.
. /t/lib.sh
export RESULTS=/work/wlv-results.tsv; : >"$RESULTS"
S=/shots
cfg=$XDG_CONFIG_HOME/big-tabbed-dock/dock.conf
big-wm-tabbed-dock >/work/dock-wlv.log 2>&1 & wait_for 10 state
big-wm-tabbed-dock --quit; sleep 0.5
sed -i 's/^AutoHide=.*/AutoHide=true/; s/^Alignment=.*/Alignment=fill/' "$cfg"
big-wm-tabbed-dock >>/work/dock-wlv.log 2>&1 & wait_for 10 state
wlpointer 1600 900 move 800 400; sleep 2
check "wayland: auto-ocultar recolhe a superfície" "$(state | grep -o 'surface=[^ ]*')" bash -c 'big-wm-tabbed-dock --print-state | grep -Eq "surface=[0-9]+x4 "'
grim $S/wl-08-autohide-hidden.png
wlpointer 1600 900 move 800 899; sleep 1
check "wayland: auto-ocultar mostra ao tocar a borda" "hidden=0" bash -c 'big-wm-tabbed-dock --print-state | grep -q "hidden=0"'
grim $S/wl-09-autohide-shown-fill.png
big-wm-tabbed-dock --quit; sleep 0.5
sed -i 's/^AutoHide=.*/AutoHide=false/; s/^Alignment=.*/Alignment=center/; s/^Position=.*/Position=top/; s/^Style=.*/Style=modern/' "$cfg"
big-wm-tabbed-dock >>/work/dock-wlv.log 2>&1 & wait_for 10 state; sleep 1
grim /work/top.png
px=$(pixel /work/top.png 800 2); bg=$(pixel /work/top.png 5 500)
check "wayland: borda superior" "pixel (800,2)=$px fundo=$bg" test "$px" != "$bg"
cp /work/top.png $S/wl-10-top-modern.png
big-wm-tabbed-dock --quit; sleep 0.5
sed -i 's/^Position=.*/Position=bottom/; s/^Style=.*/Style=classic/' "$cfg"
big-wm-tabbed-dock >>/work/dock-wlv.log 2>&1 & wait_for 10 state; sleep 1
big-wm-tabbed-dock --preferences; sleep 2.5
grim $S/wl-11-preferences.png
check "wayland: janela de preferências (xdg_toplevel)" "app_id próprio, fora da seção em execução" bash -c "big-wm-tabbed-dock --print-state | grep -q \"app_id=big-wm-tabbed-dock\" && ! big-wm-tabbed-dock --print-state | grep -q \"^running\""
# coexistência no Wayland: override do autostart do wlmdock
big-wm-tabbed-dock --dock-mode=tabbed >/dev/null 2>&1
check "wayland: modo 'tabbed' esconde o autostart do wlmdock" "Hidden=true" grep -q 'Hidden=true' "$XDG_CONFIG_HOME/autostart/big-wm-wayland-dock.desktop"
big-wm-tabbed-dock --dock-mode=both >/dev/null 2>&1
check "wayland: modo 'both' remove o override" "sem arquivo" test ! -e "$XDG_CONFIG_HOME/autostart/big-wm-wayland-dock.desktop"
# os dois docks juntos: wlmdock (Dock clássico do wlmaker) + dock com abas
wlmdock >/work/wlmdock.log 2>&1 & sleep 3
grim $S/wl-12-both-docks.png
# atalhos do compositor (Config.plist padrão): o comando que eles chamam
# (--tab) é testado acima; a tecla em si não é verificável aqui — com o wtype
# (teclado virtual) só atalhos de um modificador disparam no wlmaker headless.
if grep -q '"Logo+Alt+3" = (ShellExecute, "big-wm-tabbed-dock --tab 3")' "$HOME/.config/wlmaker/Config.plist"; then
	record "wayland: atalhos Logo+Alt+1…9/PgUp/PgDn no Config.plist padrão" PASS "big-wm-firstrun"
fi
record "wayland: tecla Logo+Alt+N dispara no wlmaker" SKIP "pendente sessão real (wtype não aciona atalhos de dois modificadores no headless)"
