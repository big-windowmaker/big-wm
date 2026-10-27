#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Duas saídas (WLR_HEADLESS_OUTPUTS=2): escolha de monitor e saída desligada.
. /t/lib.sh
export RESULTS=/work/wlmulti-results.tsv; : >"$RESULTS"
S=/shots
wlr-randr --output HEADLESS-1 --pos 0,0 >/dev/null 2>&1; wlr-randr --output HEADLESS-2 --pos 1600,0 >/dev/null 2>&1; sleep 1
wlr-randr | grep -E '^[A-Z]|Position'
big-wm-tabbed-dock >/work/dock-multi.log 2>&1 &
wait_for 10 state; sleep 1
state | grep -E '^monitor'
grim -o HEADLESS-1 /work/o1.png; grim -o HEADLESS-2 /work/o2.png
has_dock() { local p bg; p=$(pixel "$1" 800 897); bg=$(pixel "$1" 5 500); [ "$p" != "$bg" ]; }
check "multi: dock na saída principal (1)" "HEADLESS-1" has_dock /work/o1.png
if has_dock /work/o2.png; then record "multi: nada na saída 2" FAIL "HEADLESS-2"; else record "multi: nada na saída 2" PASS "HEADLESS-2"; fi
cfg=$XDG_CONFIG_HOME/big-tabbed-dock/dock.conf
big-wm-tabbed-dock --quit; sleep 1
sed -i 's/^Monitor=.*/Monitor=HEADLESS-2/' "$cfg"
big-wm-tabbed-dock >>/work/dock-multi.log 2>&1 & wait_for 10 state; sleep 1.5
state | grep -E '^monitor'
grim -o HEADLESS-2 /work/o2b.png; grim -o HEADLESS-1 /work/o1b.png
check "multi: Monitor=HEADLESS-2 (nome xdg-output) põe o dock na saída 2" "HEADLESS-2" has_dock /work/o2b.png
cp /work/o2b.png $S/wl-multi-output2.png
# saída 2 desligada (hotplug): o dock não pode sumir
wlr-randr --output HEADLESS-2 --off; sleep 2.5
state | grep -E '^monitor|^surface'
grim -o HEADLESS-1 /work/o1c.png
check "multi: saída desligada -> dock volta para a saída restante" "HEADLESS-1" has_dock /work/o1c.png
cp /work/o1c.png $S/wl-multi-after-unplug.png
if wlr-randr --output HEADLESS-2 --on 2>/dev/null; then
	sleep 2.5; grim -o HEADLESS-2 /work/o2d.png
	check "multi: saída religada -> dock volta para a saída configurada" "HEADLESS-2" has_dock /work/o2d.png
else
	record "multi: saída religada -> dock volta para a saída configurada" SKIP "o backend headless do wlroots remove a saída desligada (não dá para religar)"
fi
pgrep -x big-wm-tabbed-d >/dev/null && record "multi: dock sobreviveu ao hotplug" PASS "processo vivo" || record "multi: dock sobreviveu ao hotplug" FAIL "processo morreu"
tail -5 /work/dock-multi.log
