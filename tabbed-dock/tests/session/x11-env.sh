#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Ambiente X11 de teste: Xvfb + Window Maker com a configuração do Big WindowMaker.
# Uso (no contêiner): x11-env.sh <script-de-teste> [args]
set -u
export HOME=/work/home-x11 LANG=${LANG:-pt_BR.UTF-8} LANGUAGE='' XDG_CURRENT_DESKTOP=WindowMaker NO_AT_BRIDGE=1
export XDG_RUNTIME_DIR=/tmp/xdg-x11 XDG_CONFIG_HOME=/work/home-x11/.config
rm -rf "$HOME"; mkdir -p "$HOME" "$XDG_RUNTIME_DIR"; chmod 700 "$XDG_RUNTIME_DIR"
sed -i -E 's/^#(pt_BR|en_US)\.UTF-8/\1.UTF-8/' /etc/locale.gen; locale-gen >/dev/null 2>&1
# big-wm e o dock instalados como num sistema real
make -s -C /src install-session install-themes DESTDIR=/ >/dev/null 2>&1 || echo "aviso: make install falhou"
meson install -C /build/${DOCK_BUILD:-dock} --quiet >/dev/null 2>&1 || echo "aviso: meson install falhou"
xdg-user-dirs-update >/dev/null 2>&1; mkdir -p "$HOME/Documents" "$HOME/Downloads" "$HOME/Pictures"
# dockapps do Big WindowMaker não existem neste contêiner: stubs evitam os
# diálogos de erro do Window Maker nas capturas
for p in wmclock wmsystemtray wmcpuload wmmemload wmnet wmix; do
	command -v $p >/dev/null || printf '#!/bin/sh\nexec sleep 100000\n' >/usr/local/bin/$p
	chmod +x /usr/local/bin/$p 2>/dev/null
done
W=${W:-1600} H=${H:-900}
if [ "${XINERAMA:-0}" = 1 ]; then
	# dois monitores lado a lado (o GTK 3 os vê pelo Xinerama)
	Xvfb :1 +xinerama -extension RANDR -screen 0 $((W / 2))x${H}x24 -screen 1 $((W / 2))x${H}x24 >/work/xvfb.log 2>&1 &
else
	Xvfb :1 -screen 0 ${W}x${H}x24 +extension RANDR >/work/xvfb.log 2>&1 &
fi
export DISPLAY=:1
for _ in $(seq 50); do xwininfo -root >/dev/null 2>&1 && break; sleep 0.2; done
big-wm-firstrun x11 >/dev/null 2>&1
wmaker >/work/wmaker.log 2>&1 &
sleep 4
if [ "${DOCK_BUILD:-}" = asan ]; then
	printf "leak:libfontconfig\nleak:libdbus\nleak:libgobject\nleak:libgtk-3\nleak:libgdk-3\nleak:libglib\nleak:libpango\nleak:libcairo\nleak:libX11\nleak:libpixman\nleak:libharfbuzz\nleak:librsvg\nleak:libgio\nleak:libwayland\nleak:libgtk-layer-shell\n" >/work/lsan.supp
	export ASAN_OPTIONS=detect_leaks=1:log_path=/work/asan LSAN_OPTIONS=suppressions=/work/lsan.supp UBSAN_OPTIONS=print_stacktrace=1:log_path=/work/ubsan
	rm -f /work/asan.* /work/ubsan.*
fi
exec timeout ${TEST_TIMEOUT:-420} dbus-run-session -- "$@"
