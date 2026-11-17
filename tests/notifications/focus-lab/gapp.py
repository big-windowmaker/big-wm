#!/usr/bin/env python3
"""GTK application using GNotification (org.gtk.Notifications when owned)."""
import sys, time, gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gio, GLib
T0 = time.monotonic()
def log(*a): print(f"{time.monotonic()-T0:8.3f}", *a, flush=True)
class App(Gtk.Application):
    def __init__(self):
        super().__init__(application_id="org.test.Gapp")
    def do_startup(self):
        Gtk.Application.do_startup(self)
        for name, ptype in (("hello", None), ("reply", GLib.VariantType.new("s"))):
            a = Gio.SimpleAction.new(name, ptype)
            a.connect("activate", lambda a, p, n=name: (log("GAPP action", n, p.unpack() if p else None), self.win.present()))
            self.add_action(a)
    def do_activate(self):
        if getattr(self, "win", None):
            log("GAPP activate"); self.win.present(); return
        self.win = Gtk.ApplicationWindow(application=self, title="gapp"); self.win.set_default_size(300, 200)
        self.win.connect("focus-in-event", lambda *a: log("GAPP focus-in") or False)
        self.win.connect("focus-out-event", lambda *a: log("GAPP focus-out") or False)
        self.win.show_all()
        GLib.timeout_add(int(sys.argv[1]), self.notify)
        log("GAPP READY")
    def notify(self):
        n = Gio.Notification.new("Mensagem de Maria")
        n.set_body("Oi! Podemos conversar amanhã?")
        n.set_default_action("app.hello")
        n.add_button_with_target("Responder", "app.reply", GLib.Variant("s", "maria"))
        self.send_notification("msg-1", n)
        log("GAPP notified")
        return False
App().run([sys.argv[0]])
