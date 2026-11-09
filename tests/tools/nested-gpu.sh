#!/bin/bash
# nested-gpu.sh WLMAKER DIR: wlmaker aninhado com GPU (backend headless,
# render node), independente da tela do usuário e do bloqueio de tela, com
# HOME de teste em DIR/home (copie para lá a configuração a testar). Grava
# DIR/wl (WAYLAND_DISPLAY), DIR/xw (DISPLAY do XWayland) e DIR/pid.
# systemctl/reboot ficam bloqueados pelo safebin do laboratório.
# Usado no teste da Steam (docs/wayland-window-decorations.md), com
# tests/tools/vptr para arrastar pela barra de título.
set -eu
bin=$1
dir=$(readlink -f "$2")
mkdir -p "$dir/home/.config/wlmaker"
node=${WLR_RENDER_DRM_DEVICE:-$(find /dev/dri -name 'renderD*' | sort | head -n1)}
safebin=$HOME/.cache/big-wm-lab/safebin
HOME=$dir/home XDG_CONFIG_HOME=$dir/home/.config PATH=$safebin:$PATH \
	WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=gles2 \
	WLR_RENDER_DRM_DEVICE=$node setsid "$bin" --start_xwayland --log_to_stderr \
	>"$dir/wlmaker.log" 2>&1 &
echo $! >"$dir/pid"
for _ in $(seq 100); do
	grep -q "Starting Xwayland on" "$dir/wlmaker.log" && break
	sleep 0.1
done
grep -o "compositor for server [^ ]* at wayland-[0-9]*" "$dir/wlmaker.log" | grep -o "wayland-[0-9]*" >"$dir/wl"
grep -o "Starting Xwayland on :[0-9]*" "$dir/wlmaker.log" | tail -n1 | grep -o ":[0-9]*" >"$dir/xw"
echo "WAYLAND_DISPLAY=$(cat "$dir/wl") DISPLAY=$(cat "$dir/xw") pid=$(cat "$dir/pid")"
