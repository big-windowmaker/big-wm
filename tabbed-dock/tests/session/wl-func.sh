#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Teste funcional Wayland (wlmaker headless): layer-shell, abas, lançamento,
# rastreamento (ext-foreign-toplevel-list), ativação sem wlr-foreign-toplevel
# (Activate por D-Bus + token), reserva de espaço, arrasto no dock,
# persistência, mudança de modo da saída.
. /t/lib.sh
export RESULTS=/work/wl-results.tsv; : >"$RESULTS"
S=/shots
t0=$(date +%s%N)
big-wm-tabbed-dock >/work/dock-wl.log 2>&1 &
wait_for 10 state; t1=$(date +%s%N)
record "wayland: inicia" PASS "$(( (t1 - t0) / 1000000 )) ms até responder --print-state"
st=$(state); echo "$st" >/work/wl-state-0.txt
check "wayland: cliente nativo (sem X11)" "$(head -1 <<<"$st")" grep -q 'session=wayland layer_shell=1' /work/wl-state-0.txt
check "wayland: rastreamento ext-foreign-toplevel-list" "backend" grep -q 'backend=wayland-ext-foreign-toplevel-list' /work/wl-state-0.txt
gh=$(awk -F'[=x ]' '/^geometry=/{print $3}' <<<"$st")
grim /work/wl0.png
px=$(pixel /work/wl0.png 800 897); bg=$(pixel /work/wl0.png 5 500)
check "wayland: ancorado na borda inferior" "pixel (800,897)=$px fundo=$bg" test "$px" != "$bg"
cp /work/wl0.png $S/wl-01-start.png

wl_click tab 1
check "wayland: troca de aba (clique)" "developer" test "$(active_tab)" = developer
xy=$(wl_slot_xy tab 1); wlpointer 1600 900 move $xy scroll 1; sleep 0.6
check "wayland: troca de aba (rolagem)" "documents" test "$(active_tab)" = documents
big-wm-tabbed-dock --tab next; sleep 0.5
check "wayland: --tab remoto (atalho do compositor)" "temporary" test "$(active_tab)" = temporary
big-wm-tabbed-dock --tab 1; sleep 0.5
grim $S/wl-02-tabs.png

# calculadora (GTK 4, Wayland nativo)
wl_click item 3
wait_for 15 bash -c '[ "$(big-wm-tabbed-dock --print-state | awk '\''$1=="item" && $4=="org.gnome.Calculator.desktop"{print $5}'\'')" = windows=1 ]'
check "wayland: lançar app e indicador (app_id)" "org.gnome.Calculator windows=1" test "$(item_windows org.gnome.Calculator.desktop)" = 1
sleep 2; grim $S/wl-03-running.png
# sem wlr-foreign-toplevel-management e sem nome D-Bus: o clique abre o menu de janelas (não duplica)
wl_click item 3; sleep 2
check "wayland: clique em app sem ativação não duplica janela" "continua 1 janela" test "$(item_windows org.gnome.Calculator.desktop)" = 1
grim $S/wl-03b-window-menu-fallback.png
wlpointer 1600 900 move 300 300; sleep 2.5

# app DBusActivatable em execução: Activate por D-Bus com token xdg-activation
install -Dm644 /src/tabbed-dock/tests/fixtures/org.btd.Probe.desktop /usr/local/share/applications/org.btd.Probe.desktop
big-wm-tabbed-dock --quit; sleep 1; G_MESSAGES_DEBUG=big-wm-tabbed-dock big-wm-tabbed-dock >>/work/dock-wl.log 2>&1 & sleep 2
rm -f /work/probe.log; python3 /src/tabbed-dock/tests/fixtures/btd-probe.py >/dev/null 2>&1 &
wait_for 10 bash -c 'big-wm-tabbed-dock --print-state | grep -q "^running org.btd.Probe.desktop"'
ri=$(state | awk '/^running /{n++} /^running org.btd.Probe.desktop/{print n-1; exit}')
wl_click running "${ri:-0}"; sleep 2
check "wayland: ativar app DBusActivatable (Activate + token xdg-activation)" "$(tail -1 /work/probe.log 2>/dev/null)" grep -q "activate 2 token=." /work/probe.log
check "wayland: sem segunda instância" "1 janela btd-probe" test "$(state | grep -c 'app_id=org.btd.Probe')" = 1

# Qt nativo (featherpad) fora da aba: seção em execução
QT_QPA_PLATFORM=wayland featherpad >/dev/null 2>&1 &
wait_for 10 bash -c 'big-wm-tabbed-dock --print-state | grep -q "app_id=featherpad"'
check "wayland: app Qt nativo rastreado" "featherpad" bash -c 'big-wm-tabbed-dock --print-state | grep -q "app_id=featherpad"'
sleep 1; grim $S/wl-04-running-section.png
xy=$(wl_slot_xy item 3); wlpointer 1600 900 move $xy sleep 150 click 3; sleep 1
grim $S/wl-05-context-menu.png
wlpointer 1600 900 move 300 300; sleep 2.5

# arrastar item 0 da aba General para a aba Temporary (arrasto dentro do dock)
pkill featherpad; sleep 1.5
check "wayland: superfície encolhe quando a seção em execução some" "surface == geometry" surface_matches
grim /work/before-drag.png
check "wayland: menu fecha sozinho quando o ponteiro sai" "sem popup aberto" bash -c "! big-wm-tabbed-dock --print-state | grep -q menu_open=1"
src=$(wl_slot_xy item 0); dst=$(wl_slot_xy tab 3)
read -r sx sy <<<"$src"; read -r dx dy <<<"$dst"
wlpointer 1600 900 move $sx $sy sleep 150 down 1 sleep 100 move $((sx+8)) $sy sleep 100 move $((sx+20)) $((sy-5)) sleep 150 move $dx $dy sleep 300 move $((dx+2)) $dy sleep 300 up 1
sleep 1
check "wayland: arrastar item para outra aba" "temporary items=1" bash -c 'big-wm-tabbed-dock --print-state | grep -Eq "^tab \\*?temporary \"Temporary\" items=1"'
# reordenar dentro da aba: item 0 para depois do item 1
big-wm-tabbed-dock --tab 1; sleep 0.5
src=$(wl_slot_xy item 0); dst=$(wl_slot_xy item 1)
read -r sx sy <<<"$src"; read -r dx dy <<<"$dst"
first=$(state | awk '$1=="item" && $2==0 {print $4; exit}')
wlpointer 1600 900 move $sx $sy sleep 150 down 1 sleep 100 move $((sx+10)) $sy sleep 100 move $((dx+24)) $dy sleep 200 move $((dx+25)) $dy sleep 300 up 1
sleep 1
now=$(state | awk '$1=="item" && $2==1 {print $4; exit}')
check "wayland: reordenar item por arrasto" "$first agora na posição 1" test "$first" = "$now"

# reserva de espaço: uma janela maximizada para acima do dock
pkill -f btd-probe.py; sleep 1; rm -f /work/probe.log
BTD_PROBE_MAXIMIZE=1 python3 /src/tabbed-dock/tests/fixtures/btd-probe.py >/dev/null 2>&1 &
wait_for 10 grep -q '^size' /work/probe.log
mh=$(awk '/^size/{print $3}' /work/probe.log)
if [ "${mh:-200}" -le 200 ]; then
	record "wayland: zona exclusiva respeitada ao maximizar" XFAIL "o wlmaker não maximizou a janela de teste (altura ${mh:-?})"
else
	check "wayland: zona exclusiva respeitada ao maximizar" "altura maximizada=$mh, dock=$gh" test "$mh" -le $((900 - gh + 4))
fi
grim $S/wl-07-maximized.png
pkill -f btd-probe.py

# persistência
big-wm-tabbed-dock --quit; sleep 1
G_MESSAGES_DEBUG=big-wm-tabbed-dock big-wm-tabbed-dock >>/work/dock-wl.log 2>&1 & sleep 2
check "wayland: persistência após reinício" "item na Temporary" bash -c 'big-wm-tabbed-dock --print-state | grep -Eq "^tab \\*?temporary \"Temporary\" items=1"'

# mudança de resolução da saída
o=$(wlr-randr | awk '/^[A-Z]/{print $1; exit}')
wlr-randr --output "$o" --custom-mode 1280x720; sleep 2
grim /work/wl-720.png
px=$(pixel /work/wl-720.png 640 717); bg=$(pixel /work/wl-720.png 5 400)
check "wayland: reposiciona após mudança de resolução" "pixel (640,717)=$px fundo=$bg" test "$px" != "$bg"
cp /work/wl-720.png $S/wl-06-resolution-1280x720.png
wlr-randr --output "$o" --custom-mode 1600x900; sleep 1

# CPU ocioso e memória
dpid=$(pgrep -x big-wm-tabbed-d | head -1)
c0=$(awk '{print $14+$15}' /proc/$dpid/stat); sleep 10; c1=$(awk '{print $14+$15}' /proc/$dpid/stat)
record "wayland: CPU ocioso (10 s)" INFO "$((c1 - c0)) ticks"
record "wayland: PSS do dock" INFO "$(pss_kb $dpid) kB"
grep -v '^$' /work/dock-wl.log | grep -v 'window tracking' | tail -5
