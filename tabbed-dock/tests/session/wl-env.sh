#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Ambiente Wayland de teste: wlmaker (fork big-wm/protocols) headless, pixman.
# Uso (no contêiner): wl-env.sh <script-de-teste> [args]; OUTPUTS=2 para duas saídas.
set -u
export HOME=/work/home-wl LANG=${LANG:-pt_BR.UTF-8} LANGUAGE='' NO_AT_BRIDGE=1 GSK_RENDERER=cairo
export XDG_CURRENT_DESKTOP=wlmaker XDG_SESSION_DESKTOP=wlmaker XDG_SESSION_TYPE=wayland
export XDG_RUNTIME_DIR=/tmp/xdg-wl XDG_CONFIG_HOME=/work/home-wl/.config
rm -rf "$HOME"; mkdir -p "$HOME" "$XDG_RUNTIME_DIR"; chmod 700 "$XDG_RUNTIME_DIR"
sed -i -E 's/^#(pt_BR|en_US)\.UTF-8/\1.UTF-8/' /etc/locale.gen; locale-gen >/dev/null 2>&1
cp -a /wlinst/. / 2>/dev/null
make -s -C /src install-session install-wayland install-themes DESTDIR=/ >/dev/null 2>&1 || echo "aviso: make install falhou"
meson install -C /build/${DOCK_BUILD:-dock} --quiet >/dev/null 2>&1 || echo "aviso: meson install falhou"
mkdir -p /work/tools
if [ ! -x /work/tools/wlpointer ]; then
	x=/usr/share/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.xml
	wayland-scanner client-header $x /work/tools/vp.h && wayland-scanner private-code $x /work/tools/vp.c &&
		sed -i 's/"vp.h"/"wlr-virtual-pointer-unstable-v1-client-protocol.h"/' /work/tools/vp.c
	cp /work/tools/vp.h /work/tools/wlr-virtual-pointer-unstable-v1-client-protocol.h
	gcc -O2 -I/work/tools -o /work/tools/wlpointer /src/tabbed-dock/tests/tools/wlpointer.c /work/tools/vp.c -lwayland-client
fi
export PATH=/work/tools:$PATH
big-wm-firstrun wayland >/dev/null 2>&1
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=${OUTPUTS:-1} WLR_LIBINPUT_NO_DEVICES=1
config=$HOME/.config/wlmaker
wlmaker --root_menu_file="$config/RootMenu.plist" ${WLMAKER_ARGS:-} >/work/wlmaker.log 2>&1 &
for _ in $(seq 100); do s=$(ls "$XDG_RUNTIME_DIR" | grep -E '^wayland-[0-9]+$' | head -1); [ -n "$s" ] && break; sleep 0.2; done
export WAYLAND_DISPLAY=$s
sleep 1
W=${W:-1600} H=${H:-900}
for o in $(wlr-randr | awk '/^[A-Z]/{print $1}'); do wlr-randr --output "$o" --custom-mode ${W}x${H} >/dev/null 2>&1; done
sleep 1
# papel de parede só nas capturas (os testes comparam pixels com o fundo liso)
# (o Autostart do wlmaker já roda big-wm-wallpaper --restore -> swaybg)
if [ "${WALLPAPER:-0}" = 1 ]; then
	pgrep -x swaybg >/dev/null || { swaybg -i /usr/share/backgrounds/big-wm/default.jpg -m fill >/dev/null 2>&1 & sleep 1; }
else
	sleep 1; pkill -x swaybg; sleep 0.5
fi
if [ "${DOCK_BUILD:-}" = asan ]; then
	printf "leak:libfontconfig\nleak:libdbus\nleak:libgobject\nleak:libgtk-3\nleak:libgdk-3\nleak:libglib\nleak:libpango\nleak:libcairo\nleak:libX11\nleak:libpixman\nleak:libharfbuzz\nleak:librsvg\nleak:libgio\nleak:libwayland\nleak:libgtk-layer-shell\n" >/work/lsan.supp
	export ASAN_OPTIONS=detect_leaks=1:log_path=/work/asan LSAN_OPTIONS=suppressions=/work/lsan.supp UBSAN_OPTIONS=print_stacktrace=1:log_path=/work/ubsan
	rm -f /work/asan.* /work/ubsan.*
fi
exec timeout ${TEST_TIMEOUT:-420} dbus-run-session -- "$@"
