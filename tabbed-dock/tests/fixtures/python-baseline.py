#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Referência de medição: o mínimo equivalente do dock em Python + GTK 3
# (janela tipo dock/layer-shell, área desenhada com cairo, 6 ícones do tema).
# Não é usado pelo dock; só existe para comparar memória e tempo de início.
import os, time
import cairo  # noqa: F401 (conversor de cairo.Surface do PyGObject)
t0 = time.monotonic()
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk, GLib
layer = None
if os.environ.get('WAYLAND_DISPLAY'):
    try:
        gi.require_version('GtkLayerShell', '0.1')
        from gi.repository import GtkLayerShell as layer
    except (ValueError, ImportError):
        layer = None
w = Gtk.Window(title='python-baseline')
w.set_type_hint(Gdk.WindowTypeHint.DOCK)
if layer:
    layer.init_for_window(w); layer.set_anchor(w, layer.Edge.BOTTOM, True)
area = Gtk.DrawingArea(); area.set_size_request(460, 101); w.add(area)
theme = Gtk.IconTheme.get_default()
icons = [theme.load_surface(n, 48, 1, None, 0) for n in
         ('folder', 'utilities-terminal', 'accessories-calculator', 'text-editor', 'web-browser', 'user-home')
         if theme.has_icon(n)]
first = [True]
def draw(widget, cr):
    cr.set_source_rgb(0.76, 0.75, 0.77); cr.paint()
    for i, s in enumerate(icons):
        cr.set_source_surface(s, 8 + i * 64, 40); cr.paint()
    if first[0]:
        first[0] = False
        print('python first frame %.1f ms' % ((time.monotonic() - t0) * 1000), flush=True)
area.connect('draw', draw)
w.show_all()
Gtk.main()
