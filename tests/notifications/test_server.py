#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Conformance and robustness of big-wm-notifications on a private bus.

Run inside dbus-run-session with DISPLAY on a private X server (Xvfb):
    test_server.py /path/to/big-wm-notifications
HOME and the XDG directories point to a temporary directory.
"""
import os
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time

from gi.repository import Gio, GLib

BIN = sys.argv[1]
FDO = ("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications")
CTL = ("org.bigwindowmaker.Notifications", "/org/bigwindowmaker/Notifications", "org.bigwindowmaker.Notifications")
failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print("FAIL:", msg, flush=True)


tmp = tempfile.mkdtemp(prefix="bwn-test-")
env = dict(os.environ, HOME=tmp, XDG_CONFIG_HOME=tmp + "/config", XDG_STATE_HOME=tmp + "/state",
           XDG_DATA_HOME=tmp + "/data", XDG_CACHE_HOME=tmp + "/cache", LC_ALL="C.UTF-8",
           GDK_BACKEND="x11", NO_AT_BRIDGE="1", GIO_USE_VFS="local", GVFS_DISABLE_FUSE="1")
env.pop("XDG_SESSION_ID", None)
bus = Gio.bus_get_sync(Gio.BusType.SESSION)
signals = []
bus.signal_subscribe(None, FDO[2], None, FDO[1], None, Gio.DBusSignalFlags.NONE,
                     lambda c, s, p, i, name, params: signals.append((name, params.unpack())))


def pump(seconds):
    end = time.monotonic() + seconds
    ctx = GLib.MainContext.default()
    while time.monotonic() < end:
        ctx.iteration(False)
        time.sleep(0.01)


def call(target, method, args=None, sig=None):
    return bus.call_sync(target[0], target[1], target[2], method, args, None,
                         Gio.DBusCallFlags.NO_AUTO_START, 5000, None).unpack()


def notify(app="Test", rid=0, icon="", summary="s", body="b", actions=(), hints=None, timeout=-1):
    return call(FDO, "Notify", GLib.Variant("(susssasa{sv}i)",
                (app, rid, icon, summary, body, list(actions), hints or {}, timeout)))[0]


def status():
    dnd, unread, total, toasts = call(CTL, "GetStatus")
    return {"dnd": dnd, "unread": unread, "total": total, "toasts": toasts}


def start():
    p = subprocess.Popen([BIN], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    for _ in range(100):
        try:
            call(FDO, "GetServerInformation")
            return p
        except GLib.Error:
            time.sleep(0.05)
    p.kill()  # alive but silent: stop it, then read what it said
    raise SystemExit("server did not start: " + p.communicate(timeout=5)[1].decode(errors="replace")[-2000:])


def closed(nid):
    return [s for s in signals if s[0] == "NotificationClosed" and s[1][0] == nid]


p = start()

# 1. Information and capabilities (nothing announced that is not done).
info = call(FDO, "GetServerInformation")
check(info[0] == "Big WM Notifications" and info[3] == "1.2", f"server information {info}")
caps = set(call(FDO, "GetCapabilities")[0])
check(caps == {"actions", "body", "body-markup", "icon-static"}, f"capabilities {caps}")

# 2. Notify / CloseNotification -> NotificationClosed(id, 3).
a = notify(summary="close me")
check(a > 0, "Notify returns an id")
call(FDO, "CloseNotification", GLib.Variant("(u)", (a,)))
pump(0.3)
check([s[1][1] for s in closed(a)] == [3], f"closed by call: {closed(a)}")
call(FDO, "CloseNotification", GLib.Variant("(u)", (999999,)))  # unknown: no error

# 3. replaces_id: same sender keeps the id; unknown id gives a new one.
b = notify(summary="v1")
b2 = notify(rid=b, summary="v2")
check(b2 == b, "replaces_id keeps the id")
c = notify(rid=424242, summary="unknown replace")
check(c not in (0, 424242), f"unknown replaces_id gets a new id ({c})")

# 4. Expiry: NotificationClosed(id, 1) after the sender's timeout.
d = notify(summary="expire", timeout=1500)
pump(2.5)
check([s[1][1] for s in closed(d)] == [1], f"expired: {closed(d)}")

# 5. Critical never expires by itself.
e = notify(summary="critical", hints={"urgency": GLib.Variant("y", 2)}, timeout=-1)
pump(1.0)
check(closed(e) == [], "critical stays open")
call(FDO, "CloseNotification", GLib.Variant("(u)", (e,)))

# 6. Malformed and hostile input: the server answers and keeps running.
bad_images = [
    GLib.Variant("(iiibiiay)", (100000, 100000, 400000, True, 8, 4, b"\x00" * 16)),
    GLib.Variant("(iiibiiay)", (10, 10, 40, True, 8, 4, b"\x00" * 10)),
    GLib.Variant("(iiibiiay)", (-1, 5, 0, False, 8, 3, b"")),
    GLib.Variant("(iiibiiay)", (4, 4, 16, True, 16, 4, b"\x00" * 64)),
    GLib.Variant("s", "not an image"),
]
for img in bad_images:
    notify(summary="bad image", hints={"image-data": img}, timeout=100)
good = GLib.Variant("(iiibiiay)", (2, 2, 8, True, 8, 4, b"\xff\x00\x00\xff" * 4))
check(notify(summary="good image", hints={"image-data": good}, timeout=100) > 0, "valid image-data")
notify(summary="x" * 100000, body="<b>" * 5000 + "y" * 1000000, timeout=100)
notify(app="\x00\x01ctl", summary="\x1b[31mred", body="<a href='file:///etc/passwd'>x</a><img src=x>",
       actions=["", "empty key", "a\nb", "newline", "ok", "OK"] + [f"k{i}" for i in range(40)], timeout=100)
notify(summary="path", hints={"image-path": GLib.Variant("s", "/nonexistent/x.png"),
                              "desktop-entry": GLib.Variant("s", "../../etc/passwd"),
                              "urgency": GLib.Variant("u", 99)}, timeout=100)
check(p.poll() is None, "server survives hostile input")

# 6b. A replacement after the balloon is gone pops nothing up; its old
#     (hidden) expiry never closes the replacement.
h = notify(app="Repl", summary="first", timeout=1200)
pump(0.2)
call(CTL, "ToggleCenter")  # balloons go away, h expires later (hidden)
pump(0.3)
call(CTL, "ToggleCenter")
h2 = notify(app="Repl", rid=h, summary="second", timeout=0)
check(h2 == h, "replacement keeps the id")
pump(1.8)
check(closed(h) == [], f"the superseded expiry did not close the replacement: {closed(h)}")
call(FDO, "CloseNotification", GLib.Variant("(u)", (h,)))
pump(0.2)

# 7. Flood: one sender, 80 notifications; at most 3 balloons, still alive.
for i in range(80):
    notify(summary=f"flood {i}", timeout=60000)
pump(0.5)
st = status()
check(p.poll() is None and st["toasts"] <= 3, f"flood: {st}")
# Beyond 20 in 10 s, this sender gets no balloon: still in the history.
check(st["total"] >= 80, f"flood kept in the history: {st}")

# 8. Do not disturb: no balloons, the history goes on, persisted.
call(CTL, "SetDoNotDisturb", GLib.Variant("(b)", (True,)))
before = status()
f = notify(summary="quiet", timeout=-1)
pump(0.3)
st = status()
check(st["dnd"] and st["toasts"] == 0 and st["total"] >= before["total"], f"dnd: {st}")
check([s[1][1] for s in closed(f)] == [1], "dnd: no balloon, expired for the sender")
conf = open(tmp + "/config/big-wm/notifications.conf").read()
check("DoNotDisturb=true" in conf, "dnd persisted")
call(CTL, "SetDoNotDisturb", GLib.Variant("(b)", (False,)))

# 9. Clicking an action button (X11, xdotool): ActionInvoked + closed(2).
if shutil.which("xdotool"):
    # Another application (the flood above used up this sender's rate).
    other = Gio.DBusConnection.new_for_address_sync(
        Gio.dbus_address_get_for_bus_sync(Gio.BusType.SESSION, None),
        Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION,
        None, None)
    g = other.call_sync(FDO[0], FDO[1], FDO[2], "Notify", GLib.Variant(
        "(susssasa{sv}i)", ("Clicker", 0, "", "action", "body", ["go", "Go"], {}, 0)),
        None, Gio.DBusCallFlags.NONE, 5000, None).unpack()[0]
    pump(0.8)
    ids = subprocess.run(["xdotool", "search", "--name", "^big-wm-notification$"], capture_output=True,
                         text=True, env=env).stdout.split()
    target = None
    for w in ids:
        geo = subprocess.run(["xdotool", "getwindowgeometry", "--shell", w], capture_output=True,
                             text=True).stdout
        vals = dict(line.split("=") for line in geo.split())
        if target is None or int(vals["Y"]) < target[1]:  # the newest is at the top
            target = (int(vals["X"]), int(vals["Y"]), int(vals["WIDTH"]), int(vals["HEIGHT"]))
    if target:
        x, y, w, h = target
        # The button needs the pointer's enter before the click (as a person).
        subprocess.run(["xdotool", "mousemove", str(x + 28), str(y + h - 20)])
        pump(0.3)
        subprocess.run(["xdotool", "click", "1"])
        pump(0.8)
        inv = [s for s in signals if s[0] == "ActionInvoked" and s[1][0] == g]
        check(inv and inv[0][1][1] == "go", f"ActionInvoked: {inv}")
        check([s[1][1] for s in closed(g)] == [2], f"action dismisses: {closed(g)}")
    else:
        check(False, "no toast window found for the click")

# 10. A second server exits and leaves the name alone.
p2 = subprocess.Popen([BIN], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
check(p2.wait(timeout=10) == 0, "second instance exits 0")
check(p.poll() is None, "first instance still running")

# 11. History: written on exit, 0600 in a 0700 directory, read back.
total = status()["total"]
p.send_signal(signal.SIGTERM)
p.wait(timeout=10)
hist = tmp + "/state/big-wm/notifications-history"
check(os.path.exists(hist), "history written")
if os.path.exists(hist):
    check(stat.S_IMODE(os.stat(hist).st_mode) == 0o600, "history mode 0600")
    check(stat.S_IMODE(os.stat(os.path.dirname(hist)).st_mode) == 0o700, "history dir 0700")
    with open(hist, "wb") as fh_bad:  # damaged file: the server still starts
        fh_bad.write(b"\x00garbage" * 100)
p = start()
check(p.poll() is None, "starts with a damaged history")
p.send_signal(signal.SIGTERM)
p.wait(timeout=10)
shutil.rmtree(tmp + "/state")
p = start()
for i in range(3):
    notify(summary=f"keep {i}", timeout=100)
pump(0.5)
p.send_signal(signal.SIGTERM)
p.wait(timeout=10)
p = start()
st = status()
check(st["total"] == 3, f"history read back: {st} (before restart {total})")

# 11b. org.gtk.Notifications (GNotification): added, replaced by id,
#      removed; invalid application ids refused.
GTK = ("org.gtk.Notifications", "/org/gtk/Notifications", "org.gtk.Notifications")
before = status()["total"]
note = {"title": GLib.Variant("s", "Mensagem"), "body": GLib.Variant("s", "<b>oi</b> & tchau"),
        "default-action": GLib.Variant("s", "app.open"),
        "buttons": GLib.Variant("aa{sv}", [{"label": GLib.Variant("s", "Responder"),
                                           "action": GLib.Variant("s", "app.reply"),
                                           "target": GLib.Variant("s", "x")},
                                          {"label": GLib.Variant("s", "mau"),
                                           "action": GLib.Variant("s", "win.not-app")}])}
call(GTK, "AddNotification", GLib.Variant("(ssa{sv})", ("org.test.Gapp", "m1", note)))
call(GTK, "AddNotification", GLib.Variant("(ssa{sv})", ("org.test.Gapp", "m1", note)))
check(status()["total"] == before + 1, "GNotification added once (same id replaces)")
since = len(signals)
call(GTK, "RemoveNotification", GLib.Variant("(ss)", ("org.test.Gapp", "m1")))
pump(0.2)
check(any(x[0] == "NotificationClosed" and x[1][1] == 3 for x in signals[since:]), "GNotification removed")
try:
    call(GTK, "AddNotification", GLib.Variant("(ssa{sv})", ("not an id", "m2", note)))
    check(False, "invalid application id refused")
except GLib.Error:
    pass
check(p.poll() is None, "server alive after GNotification calls")

# 12. Clear empties the history and dismisses what is still open (2).
k = notify(app="Open", summary="still open", timeout=0)
since = len(signals)  # ids restart after a restart: only this instance's
call(CTL, "Clear")
pump(0.3)
check(status()["total"] == 0, "clear")
got = [x[1][1] for x in signals[since:] if x[0] == "NotificationClosed" and x[1][0] == k]
check(got == [2], f"clear tells the sender: {got}")

p.send_signal(signal.SIGTERM)
p.wait(timeout=10)
shutil.rmtree(tmp, ignore_errors=True)
if failures:
    print(f"{len(failures)} failure(s)")
    sys.exit(1)
print("ok")
