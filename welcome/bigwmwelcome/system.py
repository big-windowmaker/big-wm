# SPDX-License-Identifier: GPL-2.0-or-later
"""Big WindowMaker Welcome: the session and the system, without GTK.

Session detection, the real key bindings of each session, which programs
exist, how to launch them (argument lists, never a shell), the user's
preferences and the first-login decision. Kept free of GTK so the tests run
anywhere.
"""

import configparser
import os
import re
import shutil
import subprocess

APP_ID = "org.bigwindowmaker.Welcome"

# ---------------------------------------------------------------------------
# Session

X11, WAYLAND = "x11", "wayland"


def session_type(env=None):
    """'wayland' (wlmaker) or 'x11' (Window Maker).

    XDG_SESSION_TYPE alone can be stale (a terminal started elsewhere), so a
    Wayland session also needs WAYLAND_DISPLAY to name a socket that exists.
    """
    env = os.environ if env is None else env
    wl = env.get("WAYLAND_DISPLAY", "")
    if wl:
        runtime = env.get("XDG_RUNTIME_DIR", "")
        path = wl if wl.startswith("/") else os.path.join(runtime, wl)
        if runtime and os.path.exists(path):
            return WAYLAND
    if env.get("XDG_SESSION_TYPE") == "wayland" and wl:
        return WAYLAND
    return X11


def is_live_session(root="/"):
    """The live image (BigLinux ISO): the installer is there, not a user."""
    if os.path.exists(os.path.join(root, "run/miso/bootmnt")):
        return True
    try:
        with open(os.path.join(root, "proc/cmdline"), encoding="utf-8") as f:
            return "misobasedir=" in f.read()
    except OSError:
        return False


# ---------------------------------------------------------------------------
# Property lists (GNUstep / wlmaker), read only

_TOKEN = re.compile(r'''
    (?P<space>\s+)
  | (?P<comment>//[^\n]*|/\*.*?\*/)
  | (?P<string>"(?:[^"\\]|\\.)*")
  | (?P<data><[0-9a-fA-F\s]*>)
  | (?P<word>[^\s{}()=;,"<>]+)
  | (?P<punct>[{}()=;,])
''', re.VERBOSE | re.DOTALL)


class PlistError(ValueError):
    pass


def plist_loads(text):
    tokens = []
    pos = 0
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m:
            raise PlistError(f"unexpected {text[pos]!r}")
        if m.lastgroup not in ("space", "comment"):
            tokens.append((m.lastgroup, m.group()))
        pos = m.end()
    i = 0

    def take():
        nonlocal i
        if i >= len(tokens):
            raise PlistError("unexpected end")
        i += 1
        return tokens[i - 1]

    def peek():
        if i >= len(tokens):
            raise PlistError("unexpected end")
        return tokens[i][1]

    def string(kind, text):
        if kind == "string":
            return re.sub(r'\\(.)', r'\1', text[1:-1])
        if kind in ("word", "data"):
            return text
        raise PlistError(f"invalid {text!r}")

    def value():
        kind, text = take()
        if text == "{":
            d = {}
            while peek() != "}":
                key = string(*take())
                if take()[1] != "=":
                    raise PlistError("expected =")
                d[key] = value()
                if peek() == ";":
                    take()
            take()
            return d
        if text == "(":
            items = []
            while peek() != ")":
                items.append(value())
                if peek() == ",":
                    take()
            take()
            return items
        return string(kind, text)

    v = value()
    return v


def plist_load(path):
    try:
        with open(path, encoding="utf-8") as f:
            return plist_loads(f.read())
    except (OSError, PlistError, UnicodeDecodeError):
        return None


# ---------------------------------------------------------------------------
# Key bindings, as configured (never the defaults we hope for)

# Readable names of the keys, the same in both sessions.
_KEY_NAMES = {
    "mod1": "Alt", "alt": "Alt", "mod4": "Super", "logo": "Super",
    "super": "Super", "control": "Ctrl", "ctrl": "Ctrl", "shift": "Shift",
    "return": "Enter", "backspace": "Backspace", "escape": "Esc",
    "iso_left_tab": "Tab", "print": "Print",
}


def pretty_combo(combo):
    parts = [p for p in re.split(r"\+", combo) if p]
    out = []
    for p in parts:
        name = _KEY_NAMES.get(p.lower())
        if name is None:
            name = p.upper() if len(p) == 1 else p[:1].upper() + p[1:]
        if name not in out:
            out.append(name)
    order = {"Ctrl": 0, "Super": 1, "Alt": 2, "Shift": 3}
    out.sort(key=lambda n: order.get(n, 9))
    return "+".join(out)


# What the Welcome explains, and how each session names it.
# id: (wlmaker action or (action, argument), Window Maker key or command)
_SHORTCUTS = [
    ("switch", "TaskNext", ("FocusNextKey",)),
    ("terminal", ("ShellExecute", "big-wm-open terminal"), ("cmd", "big-wm-open terminal")),
    ("files", ("ShellExecute", "big-wm-open files"), ("cmd", "big-wm-open files")),
    ("apps", ("ShellExecute", "big-wm-open apps"), ("cmd", "big-wm-open apps")),
    ("clipboard", ("ShellExecute", "big-wm-open clipboard"), ("cmd", "big-wm-open clipboard")),
    ("lock", "LockScreen", ("cmd", "big-wm-lock")),
    ("close", None, ("CloseKey",)),
    ("maximize", None, ("MaximizeKey",)),
    ("minimize", None, ("MiniaturizeKey",)),
    ("zones", ("Zones", "picker"), ("cmd", "wmake-zones picker")),
    ("screenshot", ("ShellExecute", "big-wm-screenshot full"), ("cmd", "big-wm-screenshot full")),
    ("workspace", "WorkspaceNext", ("NextWorkspaceKey",)),
]


def _home(env):
    return env.get("HOME") or os.path.expanduser("~")


def _wlmaker_bindings(env):
    home = _home(env)
    config_home = env.get("XDG_CONFIG_HOME") or os.path.join(home, ".config")
    for path in (os.path.join(config_home, "wlmaker", "Config.plist"),
                 "/usr/share/big-wm/wlmaker/Config.plist.in"):
        data = plist_load(path)
        if isinstance(data, dict) and isinstance(data.get("KeyBindings"), dict):
            return data["KeyBindings"]
    return {}


def _wmaker_files(env):
    home = _home(env)
    root = env.get("GNUSTEP_USER_ROOT") or os.path.join(home, "GNUstep")
    return (os.path.join(root, "Defaults"),
            ["/etc/WindowMaker", "/usr/share/WindowMaker"])


def _wmaker_bindings(env):
    """(keys of the WindowMaker domain, {command: combo} of WMRootMenu)."""
    user, system = _wmaker_files(env)
    keys = {}
    for d in reversed([user] + system):
        data = plist_load(os.path.join(d, "WindowMaker"))
        if isinstance(data, dict):
            keys.update(data)
    menu = None
    for d in [user] + system:
        menu = plist_load(os.path.join(d, "WMRootMenu"))
        if isinstance(menu, list):
            break
    commands = {}

    def walk(items):
        for item in items:
            if not isinstance(item, list):
                continue
            if len(item) >= 5 and item[1] == "SHORTCUT" and item[3] in ("EXEC", "SHEXEC"):
                commands.setdefault(item[4], item[2])
            elif len(item) >= 2:
                walk(item[1:])
    if isinstance(menu, list):
        walk(menu)
    return keys, commands


def shortcuts(session=None, env=None):
    """[(id, combo)] really configured in the session, in a stable order."""
    env = os.environ if env is None else env
    session = session or session_type(env)
    found = []
    if session == WAYLAND:
        bindings = _wlmaker_bindings(env)
        reverse = {}
        for combo, action in bindings.items():
            key = tuple(action) if isinstance(action, list) else action
            reverse.setdefault(key, combo)
        for sid, wl, _x in _SHORTCUTS:
            if wl is None:
                continue
            combo = reverse.get(tuple(wl) if isinstance(wl, tuple) else wl)
            if combo:
                found.append((sid, pretty_combo(combo)))
    else:
        keys, commands = _wmaker_bindings(env)
        for sid, _wl, x in _SHORTCUTS:
            if x[0] == "cmd":
                combo = commands.get(x[1])
            else:
                combo = keys.get(x[0])
            if isinstance(combo, str) and combo and combo.lower() != "none":
                found.append((sid, pretty_combo(combo)))
    return found


def shortcut(sid, session=None, env=None):
    for s, combo in shortcuts(session, env):
        if s == sid:
            return combo
    return None


# ---------------------------------------------------------------------------
# Programs

def which(program, env=None):
    path = (env or os.environ).get("PATH", os.defpath)
    return shutil.which(program, path=path)


def has_package(name):
    """Installed package (pacman). False when pacman is not there."""
    if not shutil.which("pacman"):
        return False
    try:
        return subprocess.run(["pacman", "-Q", "--", name], stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL, timeout=5).returncode == 0
    except (OSError, subprocess.SubprocessError):
        return False


def session_running(program, env=None):
    """A process `program` of this user in this session (same display)."""
    env = os.environ if env is None else env
    display = env.get("DISPLAY", "")
    wayland = env.get("WAYLAND_DISPLAY", "")
    uid = os.getuid()
    try:
        pids = [p for p in os.listdir("/proc") if p.isdigit()]
    except OSError:
        return False
    for pid in pids:
        try:
            if os.stat(f"/proc/{pid}").st_uid != uid:
                continue
            with open(f"/proc/{pid}/comm", encoding="utf-8") as f:
                if f.read().strip() != program[:15]:
                    continue
            with open(f"/proc/{pid}/environ", "rb") as f:
                penv = f.read().split(b"\0")
        except OSError:
            continue
        if display and f"DISPLAY={display}".encode() in penv:
            return True
        if wayland and f"WAYLAND_DISPLAY={wayland}".encode() in penv:
            return True
    return False


def spawn(argv, env=None):
    """Starts a program detached (its own session), without a shell."""
    if not argv or not which(argv[0], env):
        return False
    try:
        subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, start_new_session=True,
                         env=env, close_fds=True)
    except OSError:
        return False
    return True


def default_browser():
    """Desktop file name of the default web browser, or None."""
    if not which("xdg-settings"):
        return None
    try:
        out = subprocess.run(["xdg-settings", "get", "default-web-browser"],
                             capture_output=True, text=True, timeout=5).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return None
    return out if out.endswith(".desktop") else None


def set_default_browser(desktop_id):
    if not re.fullmatch(r"[A-Za-z0-9._-]+\.desktop", desktop_id or ""):
        return False
    try:
        return subprocess.run(["xdg-settings", "set", "default-web-browser", desktop_id],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                              timeout=10).returncode == 0
    except (OSError, subprocess.SubprocessError):
        return False


def theme_mode():
    """'light' or 'dark', as big-wm-theme keeps it."""
    if which("big-wm-theme"):
        try:
            out = subprocess.run(["big-wm-theme", "get"], capture_output=True, text=True,
                                 timeout=5).stdout.strip()
            if out in ("light", "dark"):
                return out
        except (OSError, subprocess.SubprocessError):
            pass
    return "light"


# ---------------------------------------------------------------------------
# Preferences and the first login

def config_path(env=None):
    env = os.environ if env is None else env
    config_home = env.get("XDG_CONFIG_HOME") or os.path.join(_home(env), ".config")
    return os.path.join(config_home, "big-wm", "welcome.conf")


class Preferences:
    """~/.config/big-wm/welcome.conf: seen (shown once), show at login."""

    def __init__(self, path=None):
        self.path = path or config_path()
        self.parser = configparser.ConfigParser()
        self.parser.read(self.path, encoding="utf-8")
        if not self.parser.has_section("welcome"):
            self.parser.add_section("welcome")

    @property
    def seen(self):
        return self.parser.getboolean("welcome", "seen", fallback=False)

    @property
    def show_at_login(self):
        return self.parser.getboolean("welcome", "show-at-login", fallback=False)

    def set(self, key, value):
        self.parser.set("welcome", key, "true" if value else "false")
        self.save()

    def save(self):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        tmp = self.path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            self.parser.write(f)
        os.replace(tmp, self.path)


def should_autostart(prefs, live=False):
    """At login: the first time, and later only if the user asked.

    The live image keeps the installer in front: nothing at login there.
    """
    if live:
        return False
    if not prefs.seen:
        return True
    return prefs.show_at_login
