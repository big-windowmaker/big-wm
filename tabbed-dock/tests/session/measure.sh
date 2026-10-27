#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Medições reprodutíveis (mesmo método nas duas sessões):
#   PSS/USS de /proc/<pid>/smaps_rollup, CPU ociosa por /proc/<pid>/stat em 60 s,
#   tempo até o primeiro quadro (BTD_TIMING=1), soma de PSS da sessão antes e depois.
. /t/lib.sh
session=${1:-x11}
out=/work/measure-$session.txt; : >"$out"
uss_kb() { awk '/^Private_(Clean|Dirty):/{s+=$2} END{print s}' "/proc/$1/smaps_rollup"; }
session_pss() { local s=0 p; for p in $(pgrep -u "$(id -u)"); do [ "$p" = $$ ] && continue; [ -r /proc/$p/smaps_rollup ] && s=$((s + $(pss_kb $p 2>/dev/null || echo 0))); done; echo $s; }
procs() { ps -u "$(id -u)" -o comm= | sort | tr '\n' ' '; }
sleep 3; sync; echo 3 >/proc/sys/vm/drop_caches 2>/dev/null
before=$(session_pss); echo "antes: $(procs)" >>"$out"
pids_before=" $(pgrep -u "$(id -u)" | tr '\n' ' ') "
for run in 1 2 3; do
	BTD_TIMING=1 big-wm-tabbed-dock >/work/dock-m$run.log 2>&1 &
	wait_for 10 state; sleep 1
	grep -o 'first frame [0-9.,]* ms' /work/dock-m$run.log >>"$out"
	[ $run -lt 3 ] && { big-wm-tabbed-dock --quit; sleep 1; }
done
sleep 5
pid=$(pgrep -x big-wm-tabbed-d | head -1)
after=$(session_pss); echo "depois: $(procs)" >>"$out"
c0=$(awk '{print $14+$15}' /proc/$pid/stat); sleep 60; c1=$(awk '{print $14+$15}' /proc/$pid/stat)
{
	echo "dock PSS=$(pss_kb $pid) kB USS=$(uss_kb $pid) kB RSS=$(awk '/^Rss:/{print $2}' /proc/$pid/smaps_rollup) kB threads=$(ls /proc/$pid/task | wc -l)"
	echo "dock CPU ociosa 60 s: $((c1 - c0)) ticks"
	echo "sessão PSS antes=$before kB depois=$after kB delta=$((after - before)) kB"
	echo "processos novos (PSS kB):"
	for p in $(pgrep -u "$(id -u)"); do
		case "$pids_before" in *" $p "*) continue ;; esac
		[ -r /proc/$p/smaps_rollup ] && printf '  %s %s\n' "$(cat /proc/$p/comm)" "$(pss_kb $p)"
	done
} >>"$out"
big-wm-tabbed-dock --quit; sleep 1
for run in 1 2 3; do
	python3 /src/tabbed-dock/tests/fixtures/python-baseline.py >/work/py-m$run.log 2>&1 &
	ppid=$!; sleep 4
	grep -o 'python first frame [0-9.]* ms' /work/py-m$run.log >>"$out"
	[ $run -lt 3 ] && { kill $ppid; sleep 1; }
done
echo "python PSS=$(pss_kb $ppid) kB USS=$(uss_kb $ppid) kB" >>"$out"
kill $ppid
cat "$out"
