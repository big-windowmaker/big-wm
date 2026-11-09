# SPDX-License-Identifier: GPL-2.0-or-later
"""Big WindowMaker Welcome: the window (GTK 3).

Layout of an "introducing" program of the 1990s: the illustration on top
(its headline and the demonstration of the selected topic), four page tabs,
a compact grid of classic icons, and the pale yellow instructions panel with
the 3D buttons on the right. The frame is Window Maker's (server-side), on
X11 and on Wayland.
"""

import gettext
import os

import gi

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
gi.require_version("GdkPixbuf", "2.0")
from gi.repository import Gdk, GdkPixbuf, Gio, GLib, Gtk  # noqa: E402

from . import art, system  # noqa: E402
from .pages import DOCKAPPS, PAGES, SHORTCUT_NAMES  # noqa: E402

VERSION = "26.11"
DATADIR = os.environ.get("BIGWM_WELCOME_DATADIR", "/usr/share/big-wm/welcome")
WM_ICONS = "/usr/share/WindowMaker/Icons"

# The classic icons of each topic: Window Maker's and Big WindowMaker's own,
# then the icon theme.
ICONS = {
    "history": "big-wm-logo", "menu": "big-wm-apps", "windows": "big-wm-defaultapp",
    "dock": "GNUstepGlow", "clip": "big-wm-clip", "shortcuts": "big-wm-notepad",
    "wprefs": "/usr/share/WPrefs/WPrefs.tiff", "monitors": "big-wm-display-resolution",
    "keyboard": "input-keyboard", "access": "big-wm-magnify", "browser": "big-wm-netscape",
    "software": "system-software-install",
    "theme": "big-wm-draw", "wmdock": "big-wm-gnustep", "tabbed": "big-wm-tabbed-dock",
    "wprefs2": "/usr/share/WPrefs/WPrefs.tiff", "zones": "view-grid",
    "cliphist": "big-wm-notepad", "shutdown": "system-shutdown",
    "keys2": "big-wm-notepad", "dockapps": "big-wm-timer", "github": "big-wm-logo",
    "contribute": "big-wm-write", "support": "emblem-favorite", "authors": "GNUstep",
    "kojima": "GNUstep3D", "about": "big-wm-logo",
}

PALETTES = {
    "light": dict(bg="#aeaaae", fg="#000000", light="#ffffff", dark="#515551",
                  tab="#8d8a8e", note="#ffffd2", note_fg="#000000", note_line="#8a8660",
                  sel="#ffffff", sel_fg="#000000", focus="#000000", entry="#c9c6c9",
                  disabled="#7a767a"),
    "dark": dict(bg="#3a3a3e", fg="#e8e8e8", light="#5e5e64", dark="#18181a",
                 tab="#2a2a2e", note="#3b3622", note_fg="#f4e9b8", note_line="#6e6440",
                 sel="#55555c", sel_fg="#ffffff", focus="#f4e9b8", entry="#2a2a2e",
                  disabled="#8a8a90"),
}

CSS = """
#welcome {{ background-color: {bg}; color: {fg}; }}
#welcome label {{ color: {fg}; }}
#welcome button {{
  background-image: none; background-color: {bg}; color: {fg};
  border-radius: 0; border-style: solid; border-width: 1px;
  border-color: {light} {dark} {dark} {light};
  box-shadow: inset -1px -1px 0 {dark}, inset 1px 1px 0 {light};
  padding: 3px 10px; min-height: 18px; text-shadow: none;
}}
#welcome button:hover {{ background-color: shade({bg}, 1.05); }}
#welcome button:active, #welcome button:checked {{
  border-color: {dark} {light} {light} {dark};
  box-shadow: inset 1px 1px 0 {dark}; background-color: shade({bg}, 0.93);
}}
#welcome button:disabled, #welcome button:disabled label {{ color: {disabled}; }}
#welcome button:focus {{ outline: 1px dotted {focus}; outline-offset: -4px; }}
#welcome .tab {{ background-color: {tab}; padding: 2px 12px; }}
#welcome .tab:checked {{
  background-color: {bg}; font-weight: bold;
  border-color: {light} {dark} {bg} {light};
  box-shadow: inset 1px 1px 0 {light};
}}
#welcome flowboxchild {{ padding: 2px; border: 1px solid transparent; border-radius: 0; }}
#welcome flowboxchild:selected {{
  background-color: {sel}; border-color: {dark} {light} {light} {dark};
}}
#welcome flowboxchild:selected label {{ color: {sel_fg}; }}
#welcome flowboxchild:focus {{ outline: 1px dotted {focus}; outline-offset: -2px; }}
#welcome .note {{
  background-color: {note}; color: {note_fg};
  border-style: solid; border-width: 2px;
  border-color: {dark} {light} {light} {dark};
}}
#welcome .note label {{ color: {note_fg}; }}
#welcome .note .state {{ font-weight: bold; }}
#welcome .pageinfo {{ font-size: 0.9em; }}
#welcome checkbutton, #welcome checkbutton label {{
  background-color: transparent; background-image: none; color: {fg};
}}
#welcome dialog, #welcome .dialog-vbox {{ background-color: {bg}; color: {fg}; }}
#welcome checkbutton check {{
  background-color: {entry}; border-radius: 0;
  border: 1px solid; border-color: {dark} {light} {light} {dark};
}}
#welcome .sunken {{
  border-style: solid; border-width: 1px;
  border-color: {dark} {light} {light} {dark};
}}
"""


def _(text):
    return gettext.gettext(text)


class Welcome(Gtk.Application):
    def __init__(self):
        super().__init__(application_id=system.APP_ID,
                         flags=Gio.ApplicationFlags.FLAGS_NONE)
        self.window = None

    def do_activate(self):
        if self.window is None:
            self.window = WelcomeWindow(self)
            prefs = system.Preferences()
            if not prefs.seen:
                prefs.set("seen", True)
        self.window.present()


class WelcomeWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title=_("Big WindowMaker Welcome"))
        self.set_name("welcome")
        self.set_icon_name("big-wm")
        self.session = system.session_type()
        self.prefs = system.Preferences()
        self.page = 0
        self.selected = {}            # page index -> topic index
        self.t = 0.0
        self.loops = 0
        self.timer = None
        self.logo_cache = {}
        self._scene_ctx = None
        self.icon_cache = {}
        self.provider = Gtk.CssProvider()
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), self.provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        self.mode = system.theme_mode()
        self._apply_css()

        settings = Gtk.Settings.get_default()
        settings.connect("notify::gtk-theme-name", lambda *_a: self._theme_changed())
        self.reduce_motion = not settings.props.gtk_enable_animations
        settings.connect("notify::gtk-enable-animations", self._animations_changed)

        self._build()
        self._fit_to_screen()
        self.connect("notify::is-active", lambda *_a: self._animate())
        self.connect("window-state-event", lambda *_a: self._animate())
        self.connect("key-press-event", self._on_key)
        self.show_all()
        self._show_page(0)

    # -- Look --------------------------------------------------------------

    def _apply_css(self):
        self.provider.load_from_data(CSS.format(**PALETTES[self.mode]).encode())

    def _theme_changed(self):
        mode = system.theme_mode()
        if mode != self.mode:
            self.mode = mode
            self._apply_css()
            self.art.queue_draw()

    def _animations_changed(self, settings, _pspec):
        self.reduce_motion = not settings.props.gtk_enable_animations
        self._restart_demo()

    def _workarea(self):
        display = Gdk.Display.get_default()
        monitor = display.get_primary_monitor() or display.get_monitor(0)
        if monitor:
            area = monitor.get_workarea()
            return area.width, area.height
        return 1024, 768

    def _fit_to_screen(self):
        """640x480-like, never larger than the monitor's work area: smaller
        screens get a compact illustration, and the content scrolls."""
        aw, ah = self._workarea()
        width = min(700, aw - 24)
        height = min(600, ah - 48)
        self.set_default_size(max(320, width), max(240, height))

    # -- Layout ------------------------------------------------------------

    def _build(self):
        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        outer.set_border_width(8)
        compact = self._workarea()[1] < 620

        self.art = Gtk.DrawingArea()
        self.art.set_size_request(-1, 130 if compact else 200)
        self.art.set_vexpand(True)
        self.art.add_events(Gdk.EventMask.BUTTON_PRESS_MASK)
        self.art.connect("draw", self._draw_art)
        self.art.connect("button-press-event", lambda *_a: self._restart_demo())
        self.art.set_tooltip_text(_("Click to play the demonstration again"))
        outer.pack_start(self.art, True, True, 0)

        # The four pages: classic tabs and "Page 1 of 4".
        tabs = Gtk.Box(spacing=2)
        self.tab_buttons = []
        group = None
        for i, page in enumerate(PAGES):
            b = Gtk.RadioButton.new_with_label_from_widget(group, f"{i + 1} {_(page.tab)}")
            b.set_tooltip_text(_(page.title))
            group = group or b
            b.set_mode(False)
            b.get_style_context().add_class("tab")
            b.connect("toggled", self._on_tab, i)
            tabs.pack_start(b, False, False, 0)
            self.tab_buttons.append(b)
        self.page_label = Gtk.Label(xalign=1)
        self.page_label.get_style_context().add_class("pageinfo")
        tabs.pack_end(self.page_label, False, False, 4)
        tabs_scroll = Gtk.ScrolledWindow(vscrollbar_policy=Gtk.PolicyType.NEVER,
                                         hscrollbar_policy=Gtk.PolicyType.AUTOMATIC)
        tabs_scroll.add(tabs)
        tabs_scroll.set_propagate_natural_height(True)
        outer.pack_start(tabs_scroll, False, False, 0)

        # The icons of the page.
        self.grid = Gtk.FlowBox(selection_mode=Gtk.SelectionMode.SINGLE,
                                homogeneous=True, max_children_per_line=8,
                                min_children_per_line=3, column_spacing=4,
                                row_spacing=2, activate_on_single_click=True)
        self.grid.connect("child-activated", self._on_topic)
        self.grid.connect("selected-children-changed", self._on_topic_selected)
        grid_frame = Gtk.ScrolledWindow(hscrollbar_policy=Gtk.PolicyType.NEVER,
                                        vscrollbar_policy=Gtk.PolicyType.AUTOMATIC)
        grid_frame.get_style_context().add_class("sunken")
        grid_frame.set_min_content_height(70 if compact else 86)
        grid_frame.set_propagate_natural_height(True)
        grid_frame.add(self.grid)
        outer.pack_start(grid_frame, False, False, 0)

        # The instructions panel and the buttons.
        bottom = Gtk.Box(spacing=8)
        note_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        note = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        note.get_style_context().add_class("note")
        note.set_border_width(8)
        self.note_title = Gtk.Label(xalign=0)
        self.note_title.set_line_wrap(True)
        self.note_text = Gtk.Label(xalign=0, yalign=0)
        self.note_text.set_line_wrap(True)
        self.note_text.set_selectable(False)
        self.note_state = Gtk.Label(xalign=0)
        self.note_state.set_line_wrap(True)
        self.note_state.get_style_context().add_class("state")
        note.pack_start(self.note_title, False, False, 0)
        note.pack_start(self.note_text, True, True, 0)
        note.pack_start(self.note_state, False, False, 0)
        note_box.pack_start(note, True, True, 0)
        self.startup = Gtk.CheckButton(label=_("Show at login"))
        self.startup.set_active(self.prefs.show_at_login)
        self.startup.connect("toggled", self._on_startup)
        note_box.pack_start(self.startup, False, False, 0)
        bottom.pack_start(note_box, True, True, 0)

        buttons = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        self.back = Gtk.Button(label=_("< Back"))
        self.next = Gtk.Button(label=_("Next >"))
        self.open = Gtk.Button(label=_("Open"))
        self.install = Gtk.Button(label=_("Install"))
        self.quit = Gtk.Button(label=_("Exit"))
        self.back.connect("clicked", lambda *_a: self._go(-1))
        self.next.connect("clicked", lambda *_a: self._go(1))
        self.open.connect("clicked", lambda *_a: self._do_open())
        self.install.connect("clicked", lambda *_a: self._do_install())
        self.quit.connect("clicked", lambda *_a: self.close())
        for b, tip in ((self.back, _("Previous page (Alt+Left)")),
                       (self.next, _("Next page (Alt+Right)")),
                       (self.open, _("Open the selected item (Enter)")),
                       (self.install, _("Install the selected program")),
                       (self.quit, _("Close the welcome (Ctrl+Q)"))):
            b.set_tooltip_text(tip)
            buttons.pack_start(b, False, False, 0)
        bottom.pack_start(buttons, False, False, 0)
        outer.pack_start(bottom, False, False, 0)
        # Smaller than its content (640x480, large text): it scrolls.
        scroll = Gtk.ScrolledWindow(hscrollbar_policy=Gtk.PolicyType.AUTOMATIC,
                                    vscrollbar_policy=Gtk.PolicyType.AUTOMATIC)
        scroll.set_propagate_natural_width(True)
        scroll.set_propagate_natural_height(True)
        scroll.add(outer)
        self.add(scroll)

    # -- Icons -------------------------------------------------------------

    def _icon(self, tid, size=48):
        key = (tid, size)
        if key in self.icon_cache:
            return self.icon_cache[key]
        name = ICONS.get(tid, "")
        pix = None
        paths = [name] if name.startswith("/") else [
            os.path.join(WM_ICONS, name + ext) for ext in (".png", ".tiff", ".xpm")]
        for p in paths:
            if os.path.isfile(p):
                try:
                    pix = GdkPixbuf.Pixbuf.new_from_file_at_scale(p, size, size, True)
                    break
                except GLib.Error:
                    pass
        if pix is None:
            theme = Gtk.IconTheme.get_default()
            for n in (name, "application-x-executable"):
                try:
                    pix = theme.load_icon(n, size, Gtk.IconLookupFlags.FORCE_SIZE)
                    break
                except GLib.Error:
                    continue
        self.icon_cache[key] = pix
        return pix

    def _logo_painter(self, cr, x, y, size):
        size = int(size)
        if size <= 0:
            return
        pix = self.logo_cache.get(size)
        if pix is None:
            for p in (os.path.join(DATADIR, "big-wm.svg"),
                      os.path.join(DATADIR, "data", "big-wm.svg"),
                      "/usr/share/icons/hicolor/scalable/apps/big-wm.svg"):
                try:
                    pix = GdkPixbuf.Pixbuf.new_from_file_at_size(p, size, size)
                    break
                except GLib.Error:
                    continue
            self.logo_cache[size] = pix
        if pix is not None:
            Gdk.cairo_set_source_pixbuf(cr, pix, x, y)
            cr.paint()

    # -- Pages and topics --------------------------------------------------

    def _go(self, delta):
        self._show_page(max(0, min(len(PAGES) - 1, self.page + delta)))

    def _on_tab(self, button, index):
        if button.get_active() and index != self.page:
            self._show_page(index)

    def _show_page(self, index):
        self.page = index
        page = PAGES[index]
        if not self.tab_buttons[index].get_active():
            self.tab_buttons[index].set_active(True)
        self.page_label.set_text(_("Page {n} of {total}").format(n=index + 1, total=len(PAGES)))
        self.back.set_sensitive(index > 0)
        self.next.set_sensitive(index < len(PAGES) - 1)
        for child in self.grid.get_children():
            self.grid.remove(child)
        for i, topic in enumerate(page.topics):
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
            pix = self._icon(topic.id)
            image = Gtk.Image.new_from_pixbuf(pix) if pix else Gtk.Image()
            box.pack_start(image, False, False, 0)
            label = Gtk.Label(label=_(topic.name))
            label.set_max_width_chars(12)
            label.set_ellipsize(3)
            box.pack_start(label, False, False, 0)
            box.set_tooltip_text(_(topic.name))
            child = Gtk.FlowBoxChild()
            child.add(box)
            child.topic_index = i
            atk = child.get_accessible()
            if atk:
                atk.set_name(_(topic.name))
                atk.set_description(self._topic_text(topic))
            self.grid.add(child)
        self.grid.show_all()
        sel = self.selected.get(index)
        if sel is None:
            self.grid.unselect_all()
            self._show_intro()
        else:
            child = self.grid.get_child_at_index(sel)
            self.grid.select_child(child)
            self._show_topic(sel)

    def _show_intro(self):
        page = PAGES[self.page]
        self.topic = None
        self.note_title.set_markup(f"<b>{GLib.markup_escape_text(_(page.title))}</b> — "
                                   f"{GLib.markup_escape_text(_(page.subtitle))}")
        self.note_text.set_text(_(page.intro))
        self.note_state.set_text("")
        self.scene = page.art
        self.open.set_label(_("Open"))
        self.open.set_sensitive(False)
        self.install.set_sensitive(False)
        self._restart_demo()

    def _on_topic_selected(self, flowbox):
        children = flowbox.get_selected_children()
        if children:
            index = children[0].topic_index
            self.selected[self.page] = index
            self._show_topic(index)

    def _on_topic(self, _flowbox, child):
        self.selected[self.page] = child.topic_index
        self._show_topic(child.topic_index)

    def _topic_text(self, topic):
        text = _(topic.text)
        if "{pix}" in text:
            text = text.replace("{pix}", topic.arg or "")
        if "{version}" in text:
            session = "Wayland (wlmaker)" if self.session == system.WAYLAND else "X11 (Window Maker)"
            text = text.replace("{version}", VERSION).replace("{session}", session)
        note = topic.note.get(self.session)
        if note:
            text += " " + _(note)
        if topic.id == "cliphist":
            combo = system.shortcut("clipboard", self.session)
            if combo:
                text += " " + _("Open the history with {keys}.").format(keys=combo)
        return text

    def _show_topic(self, index):
        topic = PAGES[self.page].topics[index]
        self.topic = topic
        self.note_title.set_markup(f"<b>{GLib.markup_escape_text(_(topic.name))}</b>")
        self.note_text.set_text(self._topic_text(topic))
        state, can_open, can_install = self._state(topic)
        self.note_state.set_text(state)
        self.open.set_label(_("Copy") if topic.action == "copy-pix" else _("Open"))
        self.open.set_sensitive(can_open)
        self.install.set_sensitive(can_install)
        self.scene = topic.demo or PAGES[self.page].art
        self._restart_demo()

    def _program(self, topic):
        """The argv "Open" starts in this session, or None."""
        for argv in topic.open.get(self.session, []):
            if system.which(argv[0]):
                return argv
        return None

    def _package(self, topic):
        pkg = topic.package
        if isinstance(pkg, dict):
            pkg = pkg.get(self.session)
        return pkg

    def _state(self, topic):
        """(text of the state, can open, can install)."""
        if topic.action in ("url", "copy-pix", "about", "shortcuts", "browser", "theme",
                            "access"):
            return "", True, False
        if topic.action == "dockapps":
            n = len(self._installed_dockapps())
            return _("{n} dockapps installed. Add them from the main menu: Settings > "
                     "Dockapps.").format(n=n), False, False
        if topic.needs_running:
            if not system.which(topic.needs_running):
                return _("Not installed."), False, False
            if system.session_running(topic.needs_running):
                return _("Running in the Dock."), False, False
            return _("Installed; add it from the main menu: Settings > Dockapps."), False, False
        if not topic.open:
            return "", False, False
        if self.session not in topic.open:
            return _("Not available in this session."), False, False
        if self._program(topic):
            return _("Installed."), True, False
        pkg = self._package(topic)
        if pkg and system.which("pamac-manager"):
            return _("Not installed. Install opens Add/Remove Software to review "
                     "it."), False, True
        return _("Not installed."), False, False

    def _installed_dockapps(self):
        return [(p, n) for p, n in DOCKAPPS if system.which(p)]

    # -- Actions -----------------------------------------------------------

    def _do_open(self):
        topic = self.topic
        if topic is None:
            return
        if topic.action == "url":
            Gtk.show_uri_on_window(self, topic.arg, Gdk.CURRENT_TIME)
        elif topic.action == "copy-pix":
            clip = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD)
            clip.set_text(topic.arg, -1)
            clip.store()
            self.note_state.set_text(_("Pix key copied. Thank you!"))
        elif topic.action == "shortcuts":
            self._dialog_shortcuts()
        elif topic.action == "browser":
            self._dialog_browser()
        elif topic.action == "theme":
            self._dialog_theme()
        elif topic.action == "access":
            self._dialog_access()
        elif topic.action == "about":
            self._dialog_about()
        else:
            argv = self._program(topic)
            if argv and system.spawn(argv):
                self.note_state.set_text(_("Opening {name}...").format(name=_(topic.name)))
            else:
                self.note_state.set_text(_("Could not open {name}.").format(name=_(topic.name)))

    def _do_install(self):
        pkg = self._package(self.topic) if self.topic else None
        if pkg and system.spawn(["pamac-manager", "--install", pkg]):
            self.note_state.set_text(_("Add/Remove Software shows what will be installed; "
                                       "confirm there."))

    def _on_startup(self, button):
        self.prefs.set("show-at-login", button.get_active())

    def _on_key(self, _widget, event):
        alt = event.state & Gdk.ModifierType.MOD1_MASK
        ctrl = event.state & Gdk.ModifierType.CONTROL_MASK
        if alt and event.keyval == Gdk.KEY_Left:
            self._go(-1)
            return True
        if alt and event.keyval == Gdk.KEY_Right:
            self._go(1)
            return True
        if ctrl and event.keyval in (Gdk.KEY_q, Gdk.KEY_Q):
            self.close()
            return True
        if event.keyval in (Gdk.KEY_Return, Gdk.KEY_KP_Enter):
            focus = self.get_focus()
            if isinstance(focus, Gtk.FlowBoxChild):
                # Enter on an icon: select it and run its action, as Open.
                self.grid.select_child(focus)
                self.selected[self.page] = focus.topic_index
                self._show_topic(focus.topic_index)
                if self.open.get_sensitive():
                    self._do_open()
                return True
        if event.keyval == Gdk.KEY_F1:
            self.grid.unselect_all()
            self.selected.pop(self.page, None)
            self._show_intro()
            return True
        return False

    # -- Dialogs -----------------------------------------------------------

    def _dialog(self, title):
        d = Gtk.Dialog(title=title, transient_for=self, modal=True)
        d.set_name("welcome")
        d.add_button(_("Close"), Gtk.ResponseType.CLOSE)
        d.get_content_area().set_border_width(10)
        d.get_content_area().set_spacing(8)
        return d

    def _dialog_shortcuts(self):
        d = self._dialog(_("Shortcuts"))
        found = system.shortcuts(self.session)
        if not found:
            d.get_content_area().pack_start(
                Gtk.Label(label=_("No shortcut configured in this session.")), False, False, 0)
        grid = Gtk.Grid(column_spacing=24, row_spacing=4)
        for row, (sid, combo) in enumerate(found):
            name = Gtk.Label(label=_(SHORTCUT_NAMES.get(sid, sid)), xalign=0)
            keys = Gtk.Label(xalign=0)
            keys.set_markup(f"<b>{GLib.markup_escape_text(combo)}</b>")
            grid.attach(name, 0, row, 1, 1)
            grid.attach(keys, 1, row, 1, 1)
        d.get_content_area().pack_start(grid, False, False, 0)
        hint = Gtk.Label(label=_("As configured now. Change them in WPrefs."), xalign=0)
        d.get_content_area().pack_start(hint, False, False, 0)
        d.show_all()
        d.run()
        d.destroy()

    def _dialog_browser(self):
        d = self._dialog(_("Default browser"))
        current = system.default_browser()
        apps = [a for a in Gio.AppInfo.get_all_for_type("x-scheme-handler/https")
                if a.should_show()]
        if not apps:
            d.get_content_area().pack_start(Gtk.Label(label=_("No browser found.")),
                                            False, False, 0)
        group = None
        buttons = []
        for app in apps:
            b = Gtk.RadioButton.new_with_label_from_widget(group, app.get_display_name())
            group = group or b
            b.app_id = app.get_id()
            if app.get_id() == current:
                b.set_active(True)
            d.get_content_area().pack_start(b, False, False, 0)
            buttons.append(b)
        if apps:
            d.add_button(_("Make default"), Gtk.ResponseType.OK)
        d.show_all()
        if d.run() == Gtk.ResponseType.OK:
            for b in buttons:
                if b.get_active():
                    ok = system.set_default_browser(b.app_id)
                    self.note_state.set_text(
                        _("{name} is now the default browser.").format(name=b.get_label())
                        if ok else _("Could not change the default browser."))
        d.destroy()

    def _dialog_theme(self):
        d = self._dialog(_("Light and dark"))
        light = Gtk.RadioButton.new_with_label_from_widget(None, _("Light theme"))
        dark = Gtk.RadioButton.new_with_label_from_widget(light, _("Dark theme"))
        (dark if system.theme_mode() == "dark" else light).set_active(True)
        for b in (light, dark):
            d.get_content_area().pack_start(b, False, False, 0)
        if not system.which("big-wm-theme"):
            light.set_sensitive(False)
            dark.set_sensitive(False)
        else:
            d.add_button(_("Apply"), Gtk.ResponseType.OK)
        d.show_all()
        if d.run() == Gtk.ResponseType.OK:
            mode = "dark" if dark.get_active() else "light"
            if mode != system.theme_mode():
                system.spawn(["big-wm-theme", mode])
                self.mode = mode
                self._apply_css()
                self.art.queue_draw()
        d.destroy()

    def _dialog_access(self):
        d = self._dialog(_("Accessibility"))
        area = d.get_content_area()
        added = False
        # Nas duas sessões: no Wayland o wlmaker lê a mesma configuração.
        if system.which("big-wm-locate-pointer"):
            b = Gtk.Button(label=_("Locate the pointer when shaking the mouse: on/off"))
            b.connect("clicked", lambda *_a: system.spawn(["big-wm-locate-pointer", "--toggle"]))
            area.pack_start(b, False, False, 0)
            added = True
        if self.session == system.X11 and system.which("onboard"):
            b = Gtk.Button(label=_("On-screen keyboard (Onboard)"))
            b.connect("clicked", lambda *_a: system.spawn(["onboard"]))
            area.pack_start(b, False, False, 0)
            added = True
        if self.session == system.X11 and system.which("wmagnify"):
            b = Gtk.Button(label=_("Magnifier: a window with the area around the pointer"))
            b.connect("clicked", lambda *_a: system.spawn(["wmagnify"]))
            area.pack_start(b, False, False, 0)
            added = True
        if system.which("big-wm-theme"):
            b = Gtk.Button(label=_("Dark theme, more contrast at night"))
            b.connect("clicked", lambda *_a: (d.response(Gtk.ResponseType.CLOSE),
                                              self._dialog_theme()))
            area.pack_start(b, False, False, 0)
            added = True
        if not added:
            area.pack_start(Gtk.Label(label=_("No accessibility tool in this session.")),
                            False, False, 0)
        area.pack_start(Gtk.Label(label=_("Screen zoom: Ctrl+Alt+wheel zooms in and out, "
                                          "Ctrl+Alt+0 goes back to normal; also in the main "
                                          "menu: Appearance > Zoom in and Zoom out."),
                                  xalign=0, wrap=True), False, False, 0)
        if self.session == system.WAYLAND:
            area.pack_start(Gtk.Label(label=_("The magnifier window and the on-screen keyboard "
                                              "are available in the X11 session."),
                                      xalign=0, wrap=True), False, False, 0)
        d.show_all()
        d.run()
        d.destroy()

    def _dialog_about(self):
        d = Gtk.AboutDialog(transient_for=self, modal=True)
        d.set_program_name("Big WindowMaker Welcome")
        d.set_version(VERSION)
        d.set_comments(_("The classic is back. Discover a new way to use your desktop."))
        d.set_website("https://github.com/big-windowmaker")
        d.set_license_type(Gtk.License.GPL_2_0)
        d.set_authors(["Rafael Ruscher", "Barnabé Di Kartola"])
        d.add_credit_section(_("Window Maker"), ["Alfredo Kojima", "Dan Pascu",
                                                 "Window Maker Team"])
        d.add_credit_section(_("wlmaker"), ["Philipp Kaeser"])
        d.add_credit_section(_("Base"), ["BigLinux"])
        pix = self._icon("history", 96)
        if pix:
            d.set_logo(pix)
        d.run()
        d.destroy()

    # -- The illustration --------------------------------------------------

    def _ctx(self):
        page = PAGES[self.page]
        combos = [(_(SHORTCUT_NAMES.get(s, s)), c) for s, c in system.shortcuts(self.session)]
        return {
            "logo": self._logo_painter,
            "files": _("Files"), "window": _("Window"), "workspace": _("Workspace"),
            "menu_items": [_("Programs"), _("Terminal"), _("Files"), _("Appearance"),
                           _("Settings"), _("Session")],
            "right_click": _("Right-click the desktop"),
            "combos": combos[:6], "no_shortcuts": _("No shortcut configured."),
            "light": _("Light"), "dark": _("Dark"),
            "tabs": [_("General"), _("Internet"), _("Office"), _("System")],
            "zones": _("Zones: drag onto a zone"), "tiling": _("Tiling: windows side by side"),
            "clip_samples": [_("Hello, Big WindowMaker!"), "https://github.com/big-windowmaker",
                             _("A copied note")],
            "editor": _("Editor"),
            "shutdown_q": _("Shut down the computer?"),
            "shutdown_buttons": [_("Cancel"), _("Restart"), _("Shut down")],
            "shutdown_note": _("Only an example: nothing is shut down here."),
            "dockapps": [(p, _(n)) for p, n in self._installed_dockapps()],
            "no_dockapps": _("No dockapp installed."),
            "tribute": _("creator of Window Maker"),
            "browser": self._browser_name(),
            "page": page.id,
        }

    def _browser_name(self):
        current = system.default_browser()
        if current:
            try:
                info = Gio.DesktopAppInfo.new(current)
                if info:
                    return info.get_display_name()
            except TypeError:
                pass
        return None

    def _backdrop(self, w, h):
        """The still part (sky, grid, headline, logo), drawn once per size and
        theme: each frame of a demonstration only paints the scene over it."""
        import cairo
        key = (w, h, self.mode)
        if getattr(self, "_backdrop_key", None) != key:
            surface = cairo.ImageSurface(cairo.FORMAT_RGB24, max(1, w), max(1, h))
            cr = cairo.Context(surface)
            art.background(cr, w, h, self.mode == "dark")
            art.headline(cr, w, h, _("Introducing Big WindowMaker"),
                         _("The classic is back. Discover a new way to use your desktop."))
            # The logo at the top right, the emblem of the "introducing" box.
            size = min(56, h // 4)
            self._logo_painter(cr, w - size - 12, 8, size)
            surface.flush()
            self._backdrop_surface, self._backdrop_key = surface, key
        return self._backdrop_surface

    def _draw_art(self, widget, cr):
        w, h = widget.get_allocated_width(), widget.get_allocated_height()
        cr.set_source_surface(self._backdrop(w, h), 0, 0)
        cr.paint()
        if self._scene_ctx is None:
            self._scene_ctx = self._ctx()
        fn = art.SCENES.get(self.scene, art.scene_desktop)
        cr.save()
        fn(cr, w, h, self.t, self._scene_ctx)
        cr.restore()
        return False

    def _restart_demo(self):
        self._scene_ctx = None
        self.t = 0.0 if not self.reduce_motion else 0.7
        self.loops = 0
        self._animate()
        self.art.queue_draw()

    def _animate(self):
        """Runs only while the window is shown and active, at most 3 loops."""
        run = (not self.reduce_motion and self.get_mapped() and self.is_active()
               and self.loops < 3
               and not (self.get_window() and self.get_window().get_state()
                        & Gdk.WindowState.ICONIFIED))
        if run and self.timer is None:
            self.timer = GLib.timeout_add(66, self._tick)
        elif not run and self.timer is not None:
            GLib.source_remove(self.timer)
            self.timer = None

    def _tick(self):
        self.t += 0.066 / 4.0
        if self.t >= 1.0:
            self.t = 0.0
            self.loops += 1
            if self.loops >= 3:
                self.t = 0.7
                self.timer = None
                self.art.queue_draw()
                return False
        self.art.queue_draw()
        return True


def run(argv):
    app = Welcome()
    return app.run(argv)
