#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -e
cd /src/tabbed-dock
[ -d /build/asan ] || meson setup /build/asan --prefix=/usr -Db_sanitize=address,undefined -Dbuildtype=debug >/dev/null
meson compile -C /build/asan 2>&1 | grep -E "warning:|error:" || true
ASAN_OPTIONS=detect_leaks=1 meson test -C /build/asan 2>&1 | grep -E "^(Ok|Fail)"
