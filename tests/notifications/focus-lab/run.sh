#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Focus lab: a notification must never take the keyboard, close a menu,
# break a drag or eat the wheel. Reproduces the original complaints in a
# nested, headless wlmaker (nothing on the user's screen) with a virtual
# pointer and keyboard, a GTK probe that logs every focus/pointer/menu event,
# the notification server under test, and an application that tries to
# present itself without user input.
#
#   tests/notifications/focus-lab/run.sh
#   WLMAKER=/path/to/wlmaker NOTIFYD="dunst" tests/notifications/focus-lab/run.sh
#
# Needs: wlmaker (with wlr-virtual-pointer and virtual-keyboard: Big
# WindowMaker's), gcc, wayland-scanner, wayland-protocols, xkbcommon,
# python-gobject, notify-send, dbus-run-session. Exit status 1 if a check
# fails. The lab gets its own XDG_RUNTIME_DIR, HOME and session bus, and
# refusing stubs for systemctl & co. (a nested compositor must never reach
# the real session).
set -u
here=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$here/../../.." && pwd)
wlmaker=${WLMAKER:-wlmaker}
notifyd=${NOTIFYD:-$top/session/notifications/big-wm-notifications}

R=$(mktemp -d /tmp/bwnfl.XXXX) # short: Wayland sockets have a path limit
trap 'rm -rf "$R"' EXIT
mkdir -p "$R/home" "$R/bin" "$R/run" "$R/build"
chmod 700 "$R/run"

# Tools: the virtual input client and the "stealer".
P=$(pkg-config --variable=pkgdatadir wayland-protocols)
gen() { wayland-scanner client-header "$1" "$R/build/$2.h" && wayland-scanner private-code "$1" "$R/build/$2.c"; }
gen "$here/protocols/wlr-virtual-pointer-unstable-v1.xml" vp &&
	gen "$here/protocols/virtual-keyboard-unstable-v1.xml" vk &&
	gen "$P/stable/xdg-shell/xdg-shell.xml" xdg-shell &&
	gen "$P/staging/xdg-activation/xdg-activation-v1.xml" xdg-activation || exit 2
cflags=$(pkg-config --cflags wayland-client xkbcommon)
libs=$(pkg-config --libs wayland-client xkbcommon)
# shellcheck disable=SC2086
${CC:-cc} -O1 -I"$R/build" -o "$R/vin" "$here/vin.c" "$R/build/vp.c" "$R/build/vk.c" $cflags $libs &&
	${CC:-cc} -O1 -I"$R/build" -o "$R/stealer" "$here/stealer.c" "$R/build/xdg-shell.c" \
		"$R/build/xdg-activation.c" $cflags $libs || exit 2

for c in systemctl loginctl reboot poweroff halt shutdown systemd-run \
	dbus-update-activation-environment big-wm-session-ctl; do
	printf '#!/bin/sh\necho "lab: refused %s" >&2\nexit 1\n' "$c" >"$R/bin/$c"
	chmod +x "$R/bin/$c"
done

cat >"$R/scenario.sh" <<'EOF'
mkfifo "$R/cmd" "$R/vin.fifo"
"$R/vin" 1280 720 <"$R/vin.fifo" >"$R/vin.log" 2>&1 & V=$!
exec 7>"$R/vin.fifo"
sleep 0.3
python3 "$here/probe.py" "$R/cmd" >"$R/probe.log" 2>"$R/probe.err" & P=$!
for _ in $(seq 50); do grep -q READY "$R/probe.log" 2>/dev/null && break; sleep 0.1; done
sleep 0.5
v() { echo "$*" >&7; sleep 0.15; }
mark() { echo "MARK $*" >"$R/cmd"; }
note() { notify-send -t 1500 -a Lab "Notificação $1" "corpo $1" || echo "NOTIFY-FAILED $1" >>"$R/errors"; }
$NOTIFYD >"$R/notifyd.log" 2>&1 & D=$!
sleep 0.8
kill -0 $D 2>/dev/null || echo "NOTIFYD-DIED" >>"$R/errors"
# 1. typing, a notification arrives mid-word
v move 300 40; v down; v up
mark typing; for k in 30 48 46; do v key $k; done; note t; for k in 32 18 33; do v key $k; done; sleep 0.4
# 2. an open menu while notifications arrive (closed by Escape only)
for i in 1 2; do
	mark menu-open-$i; echo menu >"$R/cmd"; sleep 0.1; v move 400 400; v down; v up; sleep 0.5
	mark notify-$i; note m$i; sleep 1.2
	mark escape-$i; v key 1; sleep 0.5
done
# 3. a button held while a balloon appears under the pointer; a drag that
#    leaves the window (the release must come back to the application)
sleep 1.5
mark drag-under; v move 1000 60; v down; note d; sleep 0.6; v move 1001 62; sleep 0.2; v move 900 300; v up; sleep 2.5
mark drag-out; v move 1000 500; v down; v move 1150 400; sleep 0.3; v up; sleep 0.3; v move 1000 500; sleep 0.3
mark drag-end
# 4. the wheel right after
mark scroll; v move 600 300; v scroll 1; note s; sleep 0.5; v scroll 1; sleep 0.3
# 5. an application presents itself with no user input, with its message
"$R/stealer" 2500 2>"$R/stealer.log" & S=$!
sleep 0.8; v move 300 40; v down; v up; sleep 0.2
mark steal-wait; note x; sleep 2.6; v key 30; sleep 0.4
mark steal-end
kill $S 2>/dev/null
# 6. a click on a balloon takes the user to the application: one without a
#    "default" action (its window comes to the front) and a GNotification
#    one (org.gtk.Notifications: its default action runs)
python3 "$here/fdoapp.py" 2500 >"$R/fdo.log" 2>&1 & F=$!
sleep 1.2; v move 300 40; v down; v up
for _ in $(seq 100); do grep -q notified "$R/fdo.log" 2>/dev/null && break; sleep 0.1; done
sleep 0.6 # the balloon on screen
mark click-fdo; v move 1000 50; v down; v up; sleep 1.0
python3 "$here/gapp.py" 2500 >"$R/gapp.log" 2>&1 & G=$!
sleep 1.2; v move 300 40; v down; v up
for _ in $(seq 100); do grep -q notified "$R/gapp.log" 2>/dev/null && break; sleep 0.1; done
sleep 0.6
mark click-gapp; v move 1000 50; v down; v up; sleep 1.2
kill $F $G 2>/dev/null
kill -0 $D 2>/dev/null || echo "NOTIFYD-DIED" >>"$R/errors"
mark end; sleep 0.2
exec 7>&-; sleep 0.3
kill $P $D $V 2>/dev/null
EOF

cat >"$R/inner.sh" <<EOF
export PATH=$R/bin:/usr/bin HOME=$R/home XDG_RUNTIME_DIR=$R/run XDG_CONFIG_HOME=$R/home/.config
export XDG_STATE_HOME=$R/home/.local/state DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent
export GIO_USE_VFS=local NO_AT_BRIDGE=1
unset WAYLAND_DISPLAY DISPLAY XDG_SESSION_ID
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=1 WLR_LIBINPUT_NO_DEVICES=1
"$wlmaker" --log_to_stderr >$R/wlmaker.log 2>&1 &
W=\$!
for _ in \$(seq 50); do [ -S $R/run/wayland-0 ] && break; sleep 0.1; done
export WAYLAND_DISPLAY=wayland-0 GDK_BACKEND=wayland
R=$R here=$here NOTIFYD="$notifyd" bash $R/scenario.sh
kill \$W 2>/dev/null; wait 2>/dev/null
EOF
timeout 120 dbus-run-session --config-file="$here/session.conf" -- bash "$R/inner.sh"

python3 - "$R/probe.log" "$R/errors" "$R/stealer.log" "$R/fdo.log" "$R/gapp.log" <<'EOF'
import re, sys
events = []
for line in open(sys.argv[1]):
    m = re.match(r"\s*([\d.]+) (.*)", line.rstrip())
    if m:
        events.append((float(m.group(1)), m.group(2)))
def between(a, b):
    ia = next(i for i, e in enumerate(events) if e[1] == "---- " + a)
    ib = next(i for i, e in enumerate(events) if e[1] == "---- " + b)
    return [e[1] for e in events[ia + 1:ib]]
fails = 0
def check(ok, what):
    global fails
    print(("PASS: " if ok else "FAIL: ") + what)
    fails += not ok
try:
    seg = between("typing", "menu-open-1")
    check("ENTRY text 'abcdef'" in seg and "WIN focus-out" not in seg,
          "typing goes on in the same field while a notification arrives")
    for i in (1, 2):
        opened = "MENU show" in between(f"menu-open-{i}", f"notify-{i}")
        seg = between(f"notify-{i}", f"escape-{i}")
        check(opened and "MENU hide" not in seg and "WIN focus-out" not in seg,
              f"the menu opened and stays open when notification {i} arrives")
    seg = between("drag-under", "drag-out")
    check(any(e.startswith("AREA release") for e in seg) and "AREA leave normal" not in seg,
          "a balloon under a held button takes neither the pointer nor the release")
    seg = between("drag-out", "drag-end")
    check(any(e.startswith("AREA release") for e in seg), "a drag out of the window gets its release")
    seg = between("scroll", "steal-wait")
    check(sum(e.startswith("AREA scroll") for e in seg) >= 2, "the wheel keeps working")
    seg = between("steal-wait", "steal-end")
    try:
        asked = "activation requested" in open(sys.argv[3]).read()
    except OSError:
        asked = False
    check(asked and "WIN focus-out" not in seg and "ENTRY text 'abcdefa'" in seg,
          "an application presenting itself without user input does not take the keyboard")
    def after_notified(path, tag):
        try:
            lines = open(path).read().splitlines()
        except OSError:
            return []
        i = next((k for k, l in enumerate(lines) if "notified" in l), None)
        return [] if i is None else lines[i + 1:]
    fdo = after_notified(sys.argv[4], "FDO")
    check(any("FDO focus-in" in l for l in fdo),
          "a click on a balloon without a default action brings the application's window up")
    gapp = after_notified(sys.argv[5], "GAPP")
    check(any("GAPP action hello" in l for l in gapp) and any("GAPP focus-in" in l for l in gapp),
          "a click on a GNotification balloon runs the application's default action")
except StopIteration:
    check(False, "the scenario did not run to the end (see the logs)")
try:
    errors = open(sys.argv[2]).read().split()
except OSError:
    errors = []
check(not errors, "the server ran and took every notification" + (f" ({errors})" if errors else ""))
sys.exit(1 if fails else 0)
EOF
