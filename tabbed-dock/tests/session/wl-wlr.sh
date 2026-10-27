#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Dock com o wlmaker + patch proposto (wlr-foreign-toplevel-management).
. /t/lib.sh
export RESULTS=/work/wlr-results.tsv; : >"$RESULTS"
S=/shots
G_MESSAGES_DEBUG=big-wm-tabbed-dock big-wm-tabbed-dock >/work/dock-wlr.log 2>&1 &
wait_for 10 state
check "wlr: backend wlr-foreign-toplevel" "$(state | head -1)" bash -c 'big-wm-tabbed-dock --print-state | grep -q "backend=wayland-wlr-foreign-toplevel caps=31"'
calc() { state | grep -c 'app_id=org.gnome.Calculator'; }
wl_click item 3
wait_for 15 bash -c '[ "$(big-wm-tabbed-dock --print-state | grep -c app_id=org.gnome.Calculator)" = 1 ]'
sleep 2
check "wlr: estado ativo da janela" "active=1" bash -c 'big-wm-tabbed-dock --print-state | grep -q "app_id=org.gnome.Calculator.*active=1"'
wl_click item 3; sleep 1.5
check "wlr: clique no app ativo minimiza" "minimized=1" bash -c 'big-wm-tabbed-dock --print-state | grep -q "app_id=org.gnome.Calculator.*minimized=1"'
grim $S/wlr-01-minimized.png
wl_click item 3; sleep 1.5
check "wlr: clique restaura e ativa" "active=1 minimized=0" bash -c 'big-wm-tabbed-dock --print-state | grep -q "app_id=org.gnome.Calculator.*active=1 minimized=0"'
wl_click item 3 2
wait_for 10 bash -c '[ "$(big-wm-tabbed-dock --print-state | grep -c app_id=org.gnome.Calculator)" = 2 ]'
sleep 2
a1=$(state | grep 'app_id=org.gnome.Calculator' | grep -n 'active=1' | cut -d: -f1)
wl_click item 3; sleep 1.5
a2=$(state | grep 'app_id=org.gnome.Calculator' | grep -n 'active=1' | cut -d: -f1)
check "wlr: ciclo entre janelas do app" "ativa $a1 -> $a2" test -n "$a2" -a "$a1" != "$a2"
grim $S/wlr-02-two-windows.png
# fechar todas pelo menu (Close All Windows)
xy=$(wl_slot_xy item 3); wlpointer 1600 900 move $xy sleep 150 click 3; sleep 1
grim $S/wlr-03-menu.png
state >/work/wlr-state.txt
