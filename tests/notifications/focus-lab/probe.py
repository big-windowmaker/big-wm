#!/usr/bin/env python3
"""Probe app: logs every focus / pointer / grab / menu event, one per line.
Commands on the FIFO given as argv[1]: 'menu' pops a menu at the pointer."""
import os, sys, time, gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib, Gdk

T0 = time.monotonic()
def log(*a):
    print(f"{time.monotonic()-T0:8.3f}", *a, flush=True)

w = Gtk.Window(title="probe")
w.set_default_size(900, 700)
box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
entry = Gtk.Entry()
area = Gtk.EventBox()
area.set_size_request(880, 600)
area.add_events(Gdk.EventMask.ALL_EVENTS_MASK)
box.pack_start(entry, False, False, 0)
box.pack_start(area, True, True, 0)
w.add(box)

menu = Gtk.Menu()
for n in ("Um", "Dois", "Três"):
    it = Gtk.MenuItem(label=n)
    it.connect("activate", lambda i, n=n: log("MENU activate", n))
    menu.append(it)
menu.show_all()
menu.connect("deactivate", lambda m: log("MENU deactivate"))
menu.connect("hide", lambda m: log("MENU hide"))
menu.connect("show", lambda m: log("MENU show"))

w.connect("focus-in-event", lambda *a: log("WIN focus-in") or False)
w.connect("focus-out-event", lambda *a: log("WIN focus-out") or False)
w.connect("notify::is-active", lambda o, p: log("WIN is-active", o.is_active()))
entry.connect("changed", lambda e: log("ENTRY text", repr(e.get_text())))
entry.connect("focus-out-event", lambda *a: log("ENTRY focus-out") or False)
motion = {"n": 0}
def on_motion(wid, ev):
    motion["n"] += 1
    return False
area.connect("motion-notify-event", on_motion)
area.connect("enter-notify-event", lambda wd, e: log("AREA enter", e.mode.value_nick) or False)
area.connect("leave-notify-event", lambda wd, e: log("AREA leave", e.mode.value_nick) or False)
want_menu = {"on": False}
def on_press(wd, e):
    log("AREA press")
    if want_menu["on"]:
        want_menu["on"] = False
        menu.popup_at_pointer(e)
    return False
area.connect("button-press-event", on_press)
area.connect("button-release-event", lambda wd, e: log("AREA release motions=%d" % motion["n"]) or False)
area.connect("scroll-event", lambda wd, e: log("AREA scroll", e.direction.value_nick) or False)
area.connect("grab-broken-event", lambda wd, e: log("AREA grab-broken") or False)
w.connect("grab-broken-event", lambda wd, e: log("WIN grab-broken") or False)
w.connect("destroy", Gtk.main_quit)
w.show_all()
entry.grab_focus()

fifo = sys.argv[1]
def on_cmd(src, cond):
    line = os.read(fd, 256).decode().strip()
    for l in line.splitlines():
        if l.startswith("MARK "):
            log("----", l[5:])
    for c in line.split():
        if c == "menu":
            want_menu["on"] = True
        elif c == "MARK":
            pass
        elif c == "motions":
            log("MOTIONS", motion["n"])
    return True
fd = os.open(fifo, os.O_RDWR | os.O_NONBLOCK)
GLib.io_add_watch(fd, GLib.IO_IN, on_cmd)
log("READY")
Gtk.main()
