#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Teste funcional X11 (Window Maker): abas, lançamento, indicador, ativar,
# minimizar, ciclo, DnD entre abas, menu, persistência, recuperação, xrandr.
. /t/lib.sh
export RESULTS=/work/x11-results.tsv; : >"$RESULTS"
S=/shots
cfg=$XDG_CONFIG_HOME/big-tabbed-dock/dock.conf
t0=$(date +%s%N)
big-wm-tabbed-dock >/work/dock-x11.log 2>&1 &
wait_for 10 state; t1=$(date +%s%N)
record "x11: inicia" PASS "$(( (t1 - t0) / 1000000 )) ms até responder --print-state"
check "x11: tipo DOCK" "_NET_WM_WINDOW_TYPE_DOCK" bash -c 'xprop -name "Big Tabbed Dock" _NET_WM_WINDOW_TYPE | grep -q DOCK'
check "x11: reserva de espaço" "_NET_WM_STRUT_PARTIAL" bash -c 'xprop -name "Big Tabbed Dock" _NET_WM_STRUT_PARTIAL | grep -q "= 0, 0, 0, [1-9]"'
st=$(state); echo "$st" >/work/x11-state-0.txt
oy=$(awk -F'[=,]' '/^origin=/{print $3}' <<<"$st"); h=$(awk -F'[=x ]' '/^geometry=/{print $3}' <<<"$st")
check "x11: encostado na borda inferior" "y=$oy h=$h" test $((oy + h)) -eq 900
check "x11: abas iniciais" "general developer documents temporary" bash -c "grep -c '^tab ' /work/x11-state-0.txt | grep -qx 4"
import -window root $S/x11-01-start.png

# troca de aba pelo mouse
click_slot tab 1
check "x11: troca de aba (clique)" "developer" test "$(active_tab)" = developer
import -window root $S/x11-02-developer-tab.png
# rolagem na faixa de abas
xy=$(slot_xy tab 1); xdotool mousemove $xy click 5; sleep 0.6
check "x11: troca de aba (rolagem)" "documents" test "$(active_tab)" = documents
# atalhos globais Super+Alt+N
xdotool key super+alt+1; sleep 0.6
check "x11: atalho Super+Alt+1" "general" test "$(active_tab)" = general
xdotool key super+alt+Next; sleep 0.6
check "x11: atalho Super+Alt+PgDn" "developer" test "$(active_tab)" = developer
big-wm-tabbed-dock --tab 1; sleep 0.5
check "x11: --tab remoto" "general" test "$(active_tab)" = general

# lançar app real (xterm, item 1 da aba General)
click_slot item 1
wait_for 10 bash -c '[ "$(state | awk '\''$1=="item" && $4=="xterm.desktop"{print $5;exit}'\'')" = windows=1 ]'
check "x11: lançar xterm pelo dock" "item xterm windows=1" test "$(item_windows xterm.desktop)" = 1
sleep 1; import -window root $S/x11-03-running.png
check "x11: janela ativa" "active=1" bash -c 'big-wm-tabbed-dock --print-state | grep -q "class=XTerm.*active=1"'
click_slot item 1   # clique no app ativo: minimiza
sleep 1
check "x11: clique no app ativo minimiza" "minimized=1" bash -c 'big-wm-tabbed-dock --print-state | grep -q "class=XTerm.*minimized=1"'
import -window root $S/x11-04-minimized.png
click_slot item 1   # restaura e ativa
sleep 1
check "x11: clique restaura e ativa" "active=1 minimized=0" bash -c 'big-wm-tabbed-dock --print-state | grep -q "class=XTerm.*active=1 minimized=0"'
click_slot item 1 2  # botão do meio: nova instância
wait_for 10 bash -c '[ "$(big-wm-tabbed-dock --print-state | grep -c "class=XTerm")" = 2 ]'
check "x11: botão do meio abre 2ª janela" "2 xterm" test "$(item_windows xterm.desktop)" = 2
a1=$(state | grep 'class=XTerm' | grep -n 'active=1' | cut -d: -f1)
click_slot item 1; sleep 1
a2=$(state | grep 'class=XTerm' | grep -n 'active=1' | cut -d: -f1)
check "x11: ciclo entre janelas do app" "ativa $a1 -> $a2" test -n "$a2" -a "$a1" != "$a2"
# app fora da aba aparece na seção "em execução"
xeyes &
wait_for 6 bash -c 'big-wm-tabbed-dock --print-state | grep -q "^running .*windows=1"'
check "x11: app não fixado na seção em execução" "xeyes" bash -c 'big-wm-tabbed-dock --print-state | grep -q "^running"'
import -window root $S/x11-05-running-section.png

# menu de contexto do item
xy=$(slot_xy item 1); xdotool mousemove $xy sleep 0.3 click 3; sleep 1
import -window root $S/x11-06-context-menu.png
xdotool key Escape; sleep 0.5

# arrastar item da aba General para a aba Temporary (mover)
before=$(state | awk '$1=="tab" && $2 ~ /temporary/{print $4}')
src=$(slot_xy item 0); dst=$(slot_xy tab 3)
# shellcheck disable=SC2086
xdotool mousemove $src sleep 0.3 mousedown 1 sleep 0.3 mousemove_relative 10 0 sleep 0.3 mousemove_relative 10 0 sleep 0.3 mousemove $dst sleep 0.5 mousemove_relative 2 0 sleep 0.5 mouseup 1
sleep 1.5
after=$(state | awk '$1=="tab" && $2 ~ /temporary/{print $4}')
check "x11: arrastar item para outra aba" "temporary $before -> $after" test "$after" = "items=1"
import -window root $S/x11-07-after-dnd.png

# nova aba pelo botão "+" (diálogo)
click_slot add-tab -1 ; sleep 1.5
xdotool type --delay 40 "Projetos"; xdotool key Return; sleep 1
check "x11: nova aba pelo +" "custom-1 Projetos" bash -c 'big-wm-tabbed-dock --print-state | grep -q "custom-1 \"Projetos\""'
import -window root $S/x11-08-new-tab.png

# persistência após reinício
sleep 1; big-wm-tabbed-dock --quit; sleep 1
check "x11: configuração gravada" "$cfg" grep -q 'Label=Projetos' "$cfg"
big-wm-tabbed-dock >>/work/dock-x11.log 2>&1 & sleep 2
check "x11: persistência após reinício" "aba Projetos e item movido" bash -c 'big-wm-tabbed-dock --print-state | grep -q "Projetos" && big-wm-tabbed-dock --print-state | grep -Eq "tab \\*?temporary \"[^\"]*\" items=1"'

# arquivo corrompido: guardado à parte, .bak usado
big-wm-tabbed-dock --tab 2; sleep 1; big-wm-tabbed-dock --quit; sleep 1
printf '\x00\x01 lixo [[[' >"$cfg"
big-wm-tabbed-dock >>/work/dock-x11.log 2>&1 & sleep 2.5
check "x11: recuperação de config corrompida" ".broken + .bak" bash -c "ls $(dirname "$cfg") | grep -q 'dock.conf.broken-' && big-wm-tabbed-dock --print-state | grep -q '^tab '"
import -window root $S/x11-09-recovered-notice.png
xdotool key Return; sleep 0.5

# mudança de resolução (Xvfb + RandR)
xrandr --fb 1280x720 2>/dev/null || xrandr -s 1280x720 2>/dev/null
sleep 2
st=$(state); oy=$(awk -F'[=,]' '/^origin=/{print $3}' <<<"$st"); h=$(awk -F'[=x ]' '/^geometry=/{print $3}' <<<"$st")
mh=$(awk '/^monitor=/{split($3,a,"x"); print a[2]}' <<<"$st")
check "x11: reposiciona após mudança de resolução" "monitor h=$mh, y+h=$((oy + h))" test "$((oy + h))" -eq "$mh" -a "$mh" -ne 900
import -window root $S/x11-10-resolution-1280x720.png
xrandr --fb 1600x900 2>/dev/null || xrandr -s 1600x900 2>/dev/null; sleep 2

# CPU ocioso e memória
dpid=$(pgrep -x big-wm-tabbed-d | head -1)
c0=$(awk '{print $14+$15}' /proc/$dpid/stat); sleep 10; c1=$(awk '{print $14+$15}' /proc/$dpid/stat)
record "x11: CPU ocioso (10 s)" INFO "$((c1 - c0)) ticks (1 tick = 10 ms)"
record "x11: PSS do dock" INFO "$(pss_kb $dpid) kB"
cat /work/dock-x11.log | grep -v -E '^$' | tail -5
pkill xterm; pkill xeyes
