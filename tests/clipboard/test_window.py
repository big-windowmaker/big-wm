#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The Clipboard window (Super+V): right-click menu, star and live reload.

Run with DISPLAY on a private X server (Xvfb):
    test_window.py /path/to/big-wm-clipboard
The window is driven from inside GTK (no xdotool): the button handler gets
synthetic events and the menu items are activated as a click would.
"""
import importlib.machinery
import importlib.util
import os
import sqlite3
import sys
import tempfile

sys.dont_write_bytecode = True    # nothing in session/bin/__pycache__

tmp = tempfile.mkdtemp(prefix="bwm-clip-")
os.environ.update(HOME=tmp, XDG_CONFIG_HOME=tmp + "/config", XDG_DATA_HOME=tmp + "/data",
                  XDG_RUNTIME_DIR=tmp + "/rt", XDG_SESSION_TYPE="x11", LC_ALL="C.UTF-8",
                  LANGUAGE="C", GDK_BACKEND="x11", NO_AT_BRIDGE="1", PATH="/usr/bin:/bin")
os.makedirs(tmp + "/rt", mode=0o700)
loader = importlib.machinery.SourceFileLoader("big_wm_clipboard", sys.argv[1])
spec = importlib.util.spec_from_loader(loader.name, loader)
cb = importlib.util.module_from_spec(spec)
loader.exec_module(cb)

import gi  # noqa: E402
gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gdk, Gtk  # noqa: E402

conf = cb.load_conf()
for i, text in enumerate(("alpha", "beta", "gamma")):
    cb.store(conf, text, now=1000 + i)
cb.set_favorite(conf, "alpha", True)
DB = cb.db_path(conf)
failures = []
menus = []
Gtk.Menu.popup_at_pointer = lambda menu, _event: menus.append(menu)


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print("FAIL:", msg, flush=True)


def favorite(text):
    row = sqlite3.connect(DB).execute("SELECT favorite FROM items WHERE text = ?", (text,)).fetchone()
    return None if row is None else row[0]


def pump():
    while Gtk.events_pending():
        Gtk.main_iteration_do(False)


def click(win, text, button, column=1):
    pump()                      # the rows are laid out before the "click"
    view = win.view()
    for row in view.get_model():
        if win.data[row[0]][0] == text:
            rect = view.get_cell_area(row.path, view.get_column(column))
            break
    else:
        raise AssertionError(f"{text} not listed")
    ev = Gdk.EventButton()
    ev.type = Gdk.EventType.BUTTON_PRESS
    ev.button = button
    ev.x, ev.y = rect.x + 4, rect.y + rect.height / 2
    return win.on_button(view, ev)


def menu_labels(menu):
    return [i.get_label() for i in menu.get_children() if not isinstance(i, Gtk.SeparatorMenuItem)]


def activate(menu, label):
    for item in menu.get_children():
        if not isinstance(item, Gtk.SeparatorMenuItem) and item.get_label() == label:
            item.activate()
            pump()
            return
    raise AssertionError(f"{label} not in the menu")


def scenario():
    win = next(w for w in Gtk.Window.list_toplevels() if w.get_title() == "Clipboard")
    pump()
    check([win.data[r[0]][0] for r in win.view().get_model()] == ["gamma", "beta", "alpha"],
          "history in copy order, favorites included")

    click(win, "beta", 3)
    check(menu_labels(menus[-1]) == ["Copy", "Add to favorites", "Delete"], "menu of an ordinary item")
    activate(menus[-1], "Add to favorites")
    check(favorite("beta") == 1, "Add to favorites marks the item")

    click(win, "beta", 3)
    check(menu_labels(menus[-1]) == ["Copy", "Remove from favorites", "Delete"], "menu of a favorite")
    activate(menus[-1], "Remove from favorites")
    check(favorite("beta") == 0, "Remove from favorites unmarks the item")

    click(win, "gamma", 1, column=0)
    check(favorite("gamma") == 1, "a click on the star marks the favorite")

    click(win, "alpha", 3)
    activate(menus[-1], "Delete")
    check(favorite("alpha") is None, "Delete removes a favorite from history and favorites")

    # A change made by someone else (wmcliphist) shows up without reopening.
    cb.set_favorite(conf, "beta", True)
    os.utime(DB, ns=(1, 1))     # the same second may keep mtime_ns on coarse clocks
    win.poll()
    pump()
    check(win.lists["favorites"][3].get_text() == "Favorites (2)", "the window follows the database")
    win.notebook.set_current_page(1)
    pump()
    check(sorted(win.data[r[0]][0] for r in win.view().get_model()) == ["beta", "gamma"],
          "Favorites tab lists only favorites")
    win.destroy()


Gtk.main = scenario
Gtk.main_quit = lambda *_: None
cb.gui(conf)
print("ok" if not failures else f"{len(failures)} failure(s)")
sys.exit(1 if failures else 0)
