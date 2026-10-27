#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -u
big-wm-tabbed-dock >/work/dock-x11.log 2>&1 &
sleep 3
big-wm-tabbed-dock --print-state
import -window root /shots/x11-en-US-first.png
xprop -name "Big Tabbed Dock" _NET_WM_WINDOW_TYPE _NET_WM_STRUT_PARTIAL WM_CLASS 2>&1
cat /work/dock-x11.log
