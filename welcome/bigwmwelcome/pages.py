# SPDX-License-Identifier: GPL-2.0-or-later
"""Big WindowMaker Welcome: the four pages and their topics.

Declarative: each topic says what it shows (icon, name, explanation for the
yellow panel), what its demonstration is, and what "Open" does in each
session. Strings are marked with N_() and translated when shown.
"""

from .system import WAYLAND, X11


def N_(text):
    return text


GITHUB = "https://github.com/big-windowmaker"
ISSUES = "https://github.com/big-windowmaker/big-wm/issues"
NEW_ISSUE = "https://github.com/big-windowmaker/big-wm/issues/new"
WMAKER_SITE = "https://www.windowmaker.org"
PIX_KEY = "08576883-c052-4beb-97e0-6f6807c147f6"


class Topic:
    """One icon of a page.

    open:     {session: [argv]} the program "Open" starts (the first that
              exists); missing session: not available there.
    package:  pacman package that provides it (for "Install").
    action:   a special action instead of a program (theme, browser, url,
              copy-pix, shortcuts, about, dockapps).
    demo:     the animation of the illustration area.
    """

    def __init__(self, tid, icon, name, text, demo=None, open=None, package=None,
                 action=None, arg=None, note=None, needs_running=None):
        self.id = tid
        self.icon = icon
        self.name = name
        self.text = text
        self.demo = demo
        self.open = open or {}
        self.package = package
        self.action = action
        self.arg = arg
        self.note = note or {}
        self.needs_running = needs_running


class Page:
    def __init__(self, pid, title, subtitle, art, intro, topics, tab=None):
        self.id = pid
        self.tab = tab or title
        self.title = title
        self.subtitle = subtitle
        self.art = art
        self.intro = intro
        self.topics = topics


BOTH = (X11, WAYLAND)


def _both(*argv):
    return {s: [list(argv)] for s in BOTH}


PAGES = [
    Page("welcome", N_("Welcome to Big WindowMaker"),
         N_("Get to know your new desktop."), "desktop",
         N_("Welcome to Big WindowMaker! Click an icon to learn about it. "
            "Use Next to continue."),
         [
             Topic("history", "big-wm", N_("A classic"),
                   N_("Window Maker is a window manager inspired by NeXTSTEP, created "
                      "by Alfredo Kojima in 1997: simple, fast and elegant. Big "
                      "WindowMaker brings it to BigLinux, in an X11 session (Window "
                      "Maker) and a Wayland one (wlmaker)."),
                   demo="desktop"),
             Topic("menu", "open-menu", N_("Main menu"),
                   N_("Right-click on an empty spot of the desktop to open the main "
                      "menu: programs, appearance, settings and the session. Drag its "
                      "title bar to keep it open."),
                   demo="menu"),
             Topic("windows", "window-new", N_("Windows"),
                   N_("Drag the title bar to move a window and the bottom bar to "
                      "resize it. The left button minimizes it, the right one closes "
                      "it. Double-click the title to roll the window up; right-click "
                      "the title for its menu, with Maximize."),
                   demo="window"),
             Topic("dock", "user-desktop", N_("Dock"),
                   N_("The Dock keeps your favorite programs. Double-click an icon to "
                      "open the program; drag a running program's icon onto the Dock "
                      "to keep it there."),
                   demo="dock"),
             Topic("clip", "edit-paste", N_("Clip"),
                   N_("The Clip, the paperclip at the top left, changes the workspace "
                      "with its arrows and holds programs that belong to a workspace."),
                   demo="clip"),
             Topic("shortcuts", "preferences-desktop-keyboard-shortcuts",
                   N_("Shortcuts"),
                   N_("The key combinations of this session, as they are configured "
                      "now. Click Open to see the list."),
                   action="shortcuts", demo="keys"),
         ], tab=N_("Welcome")),
    Page("start", N_("First Steps"),
         N_("Make Big WindowMaker your own."), "settings",
         N_("Here are the essential settings. Select an item and click Open."),
         [
             Topic("wprefs", "WPrefs", N_("WPrefs"),
                   N_("Customize menus, windows, the Dock, shortcuts and the behavior "
                      "of Window Maker."),
                   open=_both("WPrefs"), package="windowmaker", demo="wprefs",
                   note={WAYLAND: N_("On Wayland, what wlmaker understands from WPrefs "
                                     "is applied automatically: appearance, keys, "
                                     "mouse, focus and hot corners.")}),
             Topic("monitors", "big-wm-display-resolution", N_("Monitors"),
                   N_("Resolution, refresh rate, rotation and arrangement of the monitors, "
                      "with profiles that come back at login and when the same monitors "
                      "are plugged in. Applying asks for confirmation and goes back by "
                      "itself after 15 seconds."),
                   open=_both("big-wm-display-resolution"),
                   package="big-wm-display-resolution", demo="monitors"),
             Topic("keyboard", "input-keyboard", N_("Keyboard and mouse"),
                   N_("Key repeat, mouse speed and double-click are in WPrefs. The "
                      "language and the keyboard layout follow the system, chosen "
                      "during installation."),
                   open=_both("WPrefs"), package="windowmaker", demo="keys"),
             Topic("access", "preferences-desktop-accessibility", N_("Accessibility"),
                   N_("Locate the pointer by shaking the mouse, the on-screen keyboard "
                      "and the dark theme. Animations of this tutorial follow the "
                      "system setting to reduce motion."),
                   action="access", demo="access"),
             Topic("software", "system-software-install", N_("Add/Remove Software"),
                   N_("Install, update and remove programs: from the BigLinux "
                      "repositories, the AUR and Flatpak. Also in the main menu: "
                      "Settings > Add/Remove Software."),
                   open=_both("pamac-manager"), package="pamac-gtk", demo="settings"),
             Topic("browser", "web-browser", N_("Default browser"),
                   N_("Choose the browser that opens links."),
                   action="browser", demo="browser"),
             Topic("theme", "preferences-desktop-theme", N_("Light and dark"),
                   N_("Switch the whole desktop between the light and the dark theme: "
                      "windows, menus, GTK and Qt programs."),
                   action="theme", demo="theme"),
         ], tab=N_("First Steps")),
    Page("tour", N_("Meet Window Maker"),
         N_("Discover the tools that make this desktop special."), "tools",
         N_("Select a tool to see it in action."),
         [
             Topic("wmdock", "user-desktop", N_("WMDock"),
                   N_("Keep your favorite programs at hand and follow important "
                      "information through the dockapps."),
                   demo="dock"),
             Topic("tabbed", "big-wm-tabbed-dock", N_("Tabbed Dock"),
                   N_("Programs and shortcuts in tabs, at the bottom of the screen, "
                      "inspired by OPENSTEP. Click a tab to change it; drag a program "
                      "to keep it."),
                   open=_both("big-wm-tabbed-dock", "--preferences"),
                   package="big-wm-tabbed-dock", demo="tabbed"),
             Topic("wprefs2", "WPrefs", N_("WPrefs"),
                   N_("Customize the look and the behavior of Window Maker."),
                   open=_both("WPrefs"), package="windowmaker", demo="wprefs"),
             Topic("zones", "view-grid", N_("wmake-zones"),
                   N_("Arrange windows in zones or automatic tiling. While you drag a "
                      "window, a panel shows the layouts: first the layout, then the "
                      "zone."),
                   open=_both("wmake-zones-prefs"), package="wmake-zones",
                   demo="zones"),
             Topic("cliphist", "edit-paste", N_("wmcliphist"),
                   N_("The clipboard history: what you copied stays in a list to use "
                      "again."),
                   demo="clipboard", needs_running="wmcliphist"),
             Topic("shutdown", "system-shutdown", N_("wmshutdown"),
                   N_("Shut down, restart or suspend from the dockapp's button. "
                      "This tutorial never does it for you."),
                   demo="shutdown", needs_running="wmshutdown"),
             Topic("keys2", "preferences-desktop-keyboard-shortcuts",
                   N_("Shortcuts"),
                   N_("The most useful key combinations of this session."),
                   action="shortcuts", demo="keys"),
             Topic("dockapps", "utilities-system-monitor", N_("Dockapps"),
                   N_("The little programs of the Dock: a clock, the processor, the "
                      "memory, the network, the volume. Each one fits in a tile."),
                   action="dockapps", demo="dockapps"),
         ], tab=N_("Window Maker")),
    Page("community", N_("Community and About"),
         N_("Be part of the evolution of Big WindowMaker."), "community",
         N_("Big WindowMaker is made in the open. Select an item to take part."),
         [
             Topic("github", "big-wm", N_("GitHub"),
                   N_("See the source code, follow the development and find what is "
                      "new."),
                   action="url", arg=GITHUB, demo="community"),
             Topic("contribute", "mail-send", N_("Contribute"),
                   N_("Report a problem, suggest an improvement, help with "
                      "translations or with the code: everything starts with an "
                      "issue."),
                   action="url", arg=NEW_ISSUE, demo="community"),
             Topic("support", "emblem-favorite", N_("Support"),
                   N_("Big WindowMaker is made with dedication and collaboration. If "
                      "you like the project, you can contribute voluntarily: Pix key "
                      "{pix}. Click Copy to copy it."),
                   action="copy-pix", arg=PIX_KEY, demo="community"),
             Topic("authors", "system-users", N_("Developers"),
                   N_("Big WindowMaker: Rafael Ruscher and Barnabé Di Kartola. Window "
                      "Maker: Alfredo Kojima, Dan Pascu and the Window Maker Team. "
                      "wlmaker: Philipp Kaeser. Built on BigLinux."),
                   demo="community"),
             Topic("kojima", "big-wm", N_("Alfredo Kojima"),
                   N_("Big WindowMaker is also a tribute to Alfredo Kojima, creator of "
                      "Window Maker, whose work helped preserve the elegance and "
                      "simplicity of desktops inspired by NeXTSTEP."),
                   action="url", arg=WMAKER_SITE, demo="tribute"),
             Topic("about", "help-about", N_("About"),
                   N_("Big WindowMaker Welcome {version}, GPL-2.0-or-later. Session: "
                      "{session}."),
                   action="about", demo="community"),
         ], tab=N_("Community")),
]


# The dockapps the gallery knows (program, readable name). Shown only if
# installed.
DOCKAPPS = [
    ("wmclock", N_("Clock")), ("wmcpuload", N_("Processor")),
    ("wmmemload", N_("Memory")), ("wmnet", N_("Network")), ("wmix", N_("Volume")),
    ("wmeyes", N_("Eyes")), ("wmcube", N_("3D cube")), ("wmglobe", N_("Globe")),
    ("wmgtemp", N_("Temperature")), ("wmfsm", N_("Disks")),
]

# Readable names of the shortcuts (system.shortcuts ids).
SHORTCUT_NAMES = {
    "switch": N_("Switch windows"), "terminal": N_("Terminal"), "files": N_("Files"),
    "apps": N_("Search programs"), "clipboard": N_("Clipboard history"),
    "lock": N_("Lock the screen"), "close": N_("Close the window"),
    "maximize": N_("Maximize"), "minimize": N_("Minimize"),
    "zones": N_("Choose a zone"), "screenshot": N_("Screenshot"),
    "workspace": N_("Next workspace"),
}
