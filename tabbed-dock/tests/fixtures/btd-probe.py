#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Aplicativo de teste: GApplication única (DBusActivatable) que registra cada
# ativação e o token xdg-activation recebido, e mede a área maximizada.
import os, sys
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib, Gio

LOG = os.environ.get('BTD_PROBE_LOG', '/work/probe.log')

def log(msg):
    with open(LOG, 'a') as f:
        f.write(msg + '\n')

class Probe(Gtk.Application):
    def __init__(self):
        GLib.set_prgname('org.btd.Probe')
        super().__init__(application_id='org.btd.Probe', flags=Gio.ApplicationFlags.DEFAULT_FLAGS)
        self.win = None
        self.count = 0

    def do_before_emit(self, platform_data):
        token = platform_data.lookup_value('activation-token', None)
        self.token = token.get_string() if token else ''
        Gtk.Application.do_before_emit(self, platform_data)

    def do_activate(self):
        self.count += 1
        log('activate %d token=%s' % (self.count, getattr(self, 'token', '')))
        if not self.win:
            self.win = Gtk.ApplicationWindow(application=self, title='btd-probe')
            self.win.set_default_size(300, 200)
            self.win.show_all()
            if os.environ.get('BTD_PROBE_MAXIMIZE'):
                GLib.timeout_add(800, lambda: self.win.maximize() or False)
                GLib.timeout_add(2500, lambda: log('size %d %d' % tuple(self.win.get_size())) or False)
        self.win.present()

sys.exit(Probe().run(sys.argv))
