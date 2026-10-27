#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Roda todas as suítes de sessão e junta os resultados em $BTD_LAB/work/ALL-RESULTS.tsv.
#   WLINST       wlmaker do teste padrão (DESTDIR em $BTD_LAB)
#   WLINST_WLR   wlmaker com o patch wlr-foreign-toplevel (docs/tabbed-dock/propostas)
cd "$(dirname "$0")" || exit 1
lab=${BTD_LAB:-$HOME/.cache/tabbed-dock}
rm -f "$lab"/shots/*.png "$lab"/work/*-results.tsv
podman run --rm -v "$(cd ../../.. && pwd):/src:Z" -v "$lab/build:/build:Z" -v "$PWD:/t:Z" localhost/tabbed-dock-img \
	bash -c 'rm -rf /build/dock; bash /t/build-dock.sh' >"$lab/work/build.log" 2>&1
./run-in.sh x11-env.sh x11-func.sh >"$lab/work/run-x11-func.log" 2>&1
./run-in.sh x11-env.sh x11-variants.sh >"$lab/work/run-x11-variants.log" 2>&1
# primeiro com os patches propostos ao wlmaker (resultados com outro prefixo)
WLINST=${WLINST_WLR:-wlmaker-inst-wlr} ./run-in.sh wl-env.sh wl-func.sh >"$lab/work/run-wl-func-patched.log" 2>&1
sed 's/^wayland:/wayland+patches 0042-0044:/' "$lab/work/wl-results.tsv" >"$lab/work/wlpatched-results.tsv"
./run-in.sh wl-env.sh wl-func.sh >"$lab/work/run-wl-func.log" 2>&1
./run-in.sh wl-env.sh wl-variants.sh >"$lab/work/run-wl-variants.log" 2>&1
PODMAN_ARGS="-e OUTPUTS=2" ./run-in.sh wl-env.sh wl-multi.sh >"$lab/work/run-wl-multi.log" 2>&1
WLINST=${WLINST_WLR:-wlmaker-inst-wlr} ./run-in.sh wl-env.sh wl-wlr.sh >"$lab/work/run-wl-wlr.log" 2>&1
LANG=en_US.UTF-8 ./run-in.sh x11-env.sh x11-smoke.sh >"$lab/work/run-x11-en.log" 2>&1
PODMAN_ARGS="-e WALLPAPER=1" ./run-in.sh wl-env.sh wl-shots.sh >"$lab/work/run-wl-shots.log" 2>&1
./run-in.sh x11-env.sh x11-shots.sh >"$lab/work/run-x11-shots.log" 2>&1
cat "$lab"/work/*-results.tsv >"$lab/work/ALL-RESULTS.tsv"
printf 'PASS=%s FAIL=%s SKIP/XFAIL/INFO=%s\n' "$(grep -c $'\tPASS\t' "$lab/work/ALL-RESULTS.tsv")" \
	"$(grep -c $'\tFAIL\t' "$lab/work/ALL-RESULTS.tsv")" "$(grep -cE $'\t(SKIP|XFAIL|INFO)\t' "$lab/work/ALL-RESULTS.tsv")"
