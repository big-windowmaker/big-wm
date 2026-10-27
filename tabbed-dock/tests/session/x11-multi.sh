#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Dois monitores (Xvfb + Xinerama): monitor configurado e volta ao principal.
. /t/lib.sh
export RESULTS=/work/x11m-results.tsv; : >"$RESULTS"
cfg=$XDG_CONFIG_HOME/big-tabbed-dock/dock.conf
big-wm-tabbed-dock >/work/dock-x11m.log 2>&1 & wait_for 10 state; sleep 1
state | grep -E '^monitor'
ox=$(state | awk -F'[=,]' '/^origin=/{print $2}')
check "x11 multi: dock no monitor principal" "x=$ox" test "$ox" -lt 800
big-wm-tabbed-dock --quit; sleep 0.5
sed -i 's/^Monitor=.*/Monitor=1/' "$cfg"
big-wm-tabbed-dock >>/work/dock-x11m.log 2>&1 & wait_for 10 state; sleep 1
ox=$(state | awk -F'[=,]' '/^origin=/{print $2}'); oy=$(state | awk -F'[=,]' '/^origin=/{print $3}')
check "x11 multi: Monitor=1 põe o dock no 2º monitor" "x=$ox y=$oy" test "$ox" -ge 800
check "x11 multi: strut parcial só sob o dock" "$(xprop -name 'Big Tabbed Dock' _NET_WM_STRUT_PARTIAL)" bash -c "xprop -name 'Big Tabbed Dock' _NET_WM_STRUT_PARTIAL | grep -q ', $ox, '"
import -window root /shots/x11-12-second-monitor-xinerama.png
big-wm-tabbed-dock --quit; sleep 0.5
sed -i 's/^Monitor=.*/Monitor=7/' "$cfg"
big-wm-tabbed-dock >>/work/dock-x11m.log 2>&1 & wait_for 10 state; sleep 1
ox=$(state | awk -F'[=,]' '/^origin=/{print $2}')
check "x11 multi: monitor inexistente -> principal (nunca fora da tela)" "x=$ox" test "$ox" -lt 800
