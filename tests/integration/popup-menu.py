#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Janela GTK 3 de teste com um menu de contexto como o do GParted: clique
# direito abre um GtkMenu no ponteiro, com um submenu. Fundo #303030 e itens
# magenta, para achar a janela e o menu numa captura (wayland-popups.sh).
# GDK_BACKEND escolhe Wayland nativo ou X11 (XWayland).
#
#   popup-menu.py [TÍTULO]
import sys
import gi
gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gdk, Gtk  # noqa: E402

css = Gtk.CssProvider()
css.load_from_data(b"""
window.main { background: #303030; }
menu, menuitem { background: #ff00ff; color: #000000; }
menu menuitem:hover { background: #ff00ff; }
""")
Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(), css, 800)

win = Gtk.Window(title=sys.argv[1] if len(sys.argv) > 1 else "menuapp")
win.get_style_context().add_class("main")
win.set_default_size(420, 300)
box = Gtk.EventBox()
win.add(box)

menu = Gtk.Menu()
for name in ("Novo", "Excluir", "Redimensionar/Mover", "Copiar", "Colar"):
    menu.append(Gtk.MenuItem(label=name))
sub = Gtk.MenuItem(label="Formatar para")
subm = Gtk.Menu()
for name in ("ext4", "btrfs", "fat32"):
    subm.append(Gtk.MenuItem(label=name))
sub.set_submenu(subm)
menu.append(sub)
menu.show_all()
menu.attach_to_widget(box, None)


def press(_w, ev):
    if ev.button == 3:
        print("click %d %d" % (ev.x_root, ev.y_root), flush=True)
        menu.popup_at_pointer(ev)
    return True


box.connect("button-press-event", press)
win.connect("destroy", Gtk.main_quit)
win.show_all()
Gtk.main()
