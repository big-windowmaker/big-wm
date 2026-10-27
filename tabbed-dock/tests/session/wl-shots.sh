#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Capturas reais da sessão Wayland (wlmaker headless) com o papel de parede.
. /t/lib.sh
S=/shots
big-wm-tabbed-dock >/work/dock-shots.log 2>&1 & wait_for 10 state; sleep 1
grim $S/wl-real-01-general.png
big-wm-tabbed-dock --activate-item 3; sleep 4
QT_QPA_PLATFORM=wayland featherpad >/dev/null 2>&1 & sleep 3
grim $S/wl-real-02-running.png
big-wm-tabbed-dock --tab 3; sleep 1
grim $S/wl-real-03-documents.png
wlmdock >/dev/null 2>&1 & sleep 3
big-wm-tabbed-dock --tab 1; sleep 1
grim $S/wl-real-04-both-docks.png
