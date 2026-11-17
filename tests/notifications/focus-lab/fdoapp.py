#!/usr/bin/env python3
"""Plain app: fdo notification with NO default action (as Sunshine, scripts)."""
import sys, time, gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gio, GLib
GLib.set_prgname("org.test.Fdo")
T0 = time.monotonic()
def log(*a): print(f"{time.monotonic()-T0:8.3f}", *a, flush=True)
w = Gtk.Window(title="fdoapp"); w.set_default_size(300, 200); w.show_all()
w.connect("focus-in-event", lambda *a: log("FDO focus-in") or False)
w.connect("focus-out-event", lambda *a: log("FDO focus-out") or False)
bus = Gio.bus_get_sync(Gio.BusType.SESSION)
def notify():
    r = bus.call_sync("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify",
        GLib.Variant("(susssasa{sv}i)", ("Fdo App", 0, "", "Stream Started", "Sem ação default", [], {"desktop-entry": GLib.Variant("s", "org.test.Fdo")}, 0)), None, 0, -1, None)
    log("FDO notified", r[0]); return False
GLib.timeout_add(int(sys.argv[1]), notify)
log("FDO READY")
Gtk.main()
