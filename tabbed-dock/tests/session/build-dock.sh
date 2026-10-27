#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Compila o dock completo no contêiner (uso: build-dock.sh [buildtype])
set -e
cd /src/tabbed-dock
if [ ! -d /build/dock ]; then meson setup /build/dock --prefix=/usr -Dbuildtype=${1:-debugoptimized} >/build/meson-setup.log; fi
meson compile -C /build/dock 2>&1 | grep -E "warning:|error:|FAILED" || true
meson test -C /build/dock 2>&1 | grep -E "^(Ok|Fail|Expected|Unexpected)" 
ls -la /build/dock/src/big-wm-tabbed-dock
