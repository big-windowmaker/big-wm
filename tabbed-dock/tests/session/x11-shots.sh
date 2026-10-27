#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Capturas reais da sessão X11 (Window Maker + Xvfb) com o papel de parede.
. /t/lib.sh
S=/shots
big-wm-tabbed-dock >/work/dock-shots.log 2>&1 & wait_for 10 state; sleep 1
import -window root $S/x11-real-01-general.png
big-wm-tabbed-dock --activate-item 1; sleep 3
big-wm-tabbed-dock --activate-item 3; sleep 3
xeyes -geometry 150x100+1100+200 & sleep 2
import -window root $S/x11-real-02-running.png
big-wm-tabbed-dock --tab 3; sleep 1
import -window root $S/x11-real-03-documents.png
big-wm-tabbed-dock --tab 1; sleep 0.5
xy=$(slot_xy item 1); xdotool mousemove $xy sleep 0.5 click 3; sleep 1.5
import -window root $S/x11-real-04-menu.png
xdotool key Escape
