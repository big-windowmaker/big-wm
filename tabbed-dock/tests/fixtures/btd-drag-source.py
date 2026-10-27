#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Fonte de arrasto de teste (como um gerenciador de arquivos): arrasta os
# arquivos da linha de comando como text/uri-list, oferecendo COPY e MOVE.
import sys
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk, Gio

paths = sys.argv[1:]
w = Gtk.Window(title='btd-drag-source')
w.set_default_size(160, 120)
w.move(100, 100)
b = Gtk.Button(label='drag me')
w.add(b)
b.drag_source_set(Gdk.ModifierType.BUTTON1_MASK, [], Gdk.DragAction.COPY | Gdk.DragAction.MOVE)
b.drag_source_add_uri_targets()

def get(widget, ctx, sel, info, time):
    sel.set_uris([Gio.File.new_for_path(p).get_uri() for p in paths])

def end(widget, ctx):
    # A MOVE would mean "delete the source": record what the dock chose.
    with open('/work/drag-action.txt', 'w') as f:
        f.write(str(ctx.get_selected_action()))

b.connect('drag-data-get', get)
b.connect('drag-end', end)
w.connect('destroy', Gtk.main_quit)
w.show_all()
Gtk.main()
