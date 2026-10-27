#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
. /t/lib.sh
printf 'leak:libfontconfig\nleak:libdbus\nleak:libgobject\nleak:libgtk-3\nleak:libgdk-3\nleak:libglib\nleak:libpango\nleak:libcairo\nleak:libX11\nleak:libpixman\nleak:libharfbuzz\nleak:librsvg\nleak:libgio\n' >/work/lsan.supp
export ASAN_OPTIONS=detect_leaks=1:log_path=/work/asan:abort_on_error=0 LSAN_OPTIONS=suppressions=/work/lsan.supp:print_suppressions=0 UBSAN_OPTIONS=print_stacktrace=1:log_path=/work/ubsan
rm -f /work/asan.* /work/ubsan.*
/build/asan/src/big-wm-tabbed-dock >/work/dock-asan.log 2>&1 &
wait_for 30 state; sleep 2
for t in 2 3 4 1; do big-wm-tabbed-dock --tab $t; sleep 0.7; done
click_slot item 1; sleep 3
xy=$(slot_xy item 1); xdotool mousemove $xy sleep 0.5 click 3; sleep 1.5; xdotool key Escape; sleep 1
src=$(slot_xy item 0); dst=$(slot_xy tab 3)
xdotool mousemove $src sleep 0.3 mousedown 1 sleep 0.3 mousemove_relative 12 0 sleep 0.3 mousemove $dst sleep 1 mousemove_relative 2 0 sleep 0.3 mouseup 1; sleep 1
big-wm-tabbed-dock --tab 1; big-wm-tabbed-dock --reload; sleep 1
big-wm-tabbed-dock --preferences; sleep 2
pkill xterm; sleep 1
big-wm-tabbed-dock --quit; sleep 4
ls /work/asan.* /work/ubsan.* 2>/dev/null && cat /work/asan.* /work/ubsan.* 2>/dev/null | grep -E 'ERROR|SUMMARY|runtime error|#[0-9] .*btd' | head -40 || echo "ASan/LSan/UBSan: nenhum relatório"
