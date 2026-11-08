# SPDX-License-Identifier: GPL-2.0-or-later
"""Big WindowMaker Welcome: the logic (no display needed)."""

import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "welcome"))

from bigwmwelcome import art, pages, system  # noqa: E402


class SessionTest(unittest.TestCase):
    def test_wayland_needs_a_socket(self):
        with tempfile.TemporaryDirectory() as run:
            open(os.path.join(run, "wayland-9"), "w").close()
            env = {"XDG_RUNTIME_DIR": run, "WAYLAND_DISPLAY": "wayland-9",
                   "XDG_SESSION_TYPE": "wayland"}
            self.assertEqual(system.session_type(env), system.WAYLAND)
            env["WAYLAND_DISPLAY"] = "wayland-gone"
            env["XDG_SESSION_TYPE"] = "x11"
            self.assertEqual(system.session_type(env), system.X11)
        self.assertEqual(system.session_type({"DISPLAY": ":0"}), system.X11)

    def test_live(self):
        with tempfile.TemporaryDirectory() as root:
            os.makedirs(os.path.join(root, "proc"))
            with open(os.path.join(root, "proc", "cmdline"), "w") as f:
                f.write("BOOT_IMAGE=/vmlinuz quiet")
            self.assertFalse(system.is_live_session(root))
            with open(os.path.join(root, "proc", "cmdline"), "w") as f:
                f.write("misobasedir=manjaro misolabel=X")
            self.assertTrue(system.is_live_session(root))


class PlistTest(unittest.TestCase):
    def test_comments_and_nesting(self):
        data = system.plist_loads('''{
            // a comment
            KeyBindings = {
                "Logo+V" = (ShellExecute, "big-wm-open clipboard");
                "Alt+Tab" = TaskNext; /* another */
            };
            List = (a, "b c");
        }''')
        self.assertEqual(data["KeyBindings"]["Alt+Tab"], "TaskNext")
        self.assertEqual(data["KeyBindings"]["Logo+V"],
                         ["ShellExecute", "big-wm-open clipboard"])
        self.assertEqual(data["List"], ["a", "b c"])

    def test_broken_file_is_none(self):
        with tempfile.NamedTemporaryFile("w", suffix=".plist", delete=False) as f:
            f.write("{ a = ")
        try:
            self.assertIsNone(system.plist_load(f.name))
        finally:
            os.unlink(f.name)


class ShortcutsTest(unittest.TestCase):
    def setUp(self):
        self.home = tempfile.TemporaryDirectory()
        self.env = {"HOME": self.home.name}

    def tearDown(self):
        self.home.cleanup()

    def write(self, rel, text):
        path = os.path.join(self.home.name, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)

    def test_wayland_reads_wlmaker_config(self):
        self.write(".config/wlmaker/Config.plist", '''{ KeyBindings = {
            "Logo+V" = (ShellExecute, "big-wm-open clipboard");
            "Alt+Tab" = TaskNext;
            "Logo+L" = LockScreen;
        }; }''')
        found = dict(system.shortcuts(system.WAYLAND, self.env))
        self.assertEqual(found["clipboard"], "Super+V")
        self.assertEqual(found["switch"], "Alt+Tab")
        self.assertEqual(found["lock"], "Super+L")
        # Not configured: not shown (no hoped-for defaults).
        self.assertNotIn("terminal", found)

    def test_x11_reads_windowmaker_and_menu(self):
        self.write("GNUstep/Defaults/WindowMaker", '''{
            FocusNextKey = "Mod1+Tab"; CloseKey = "Mod1+F4"; MaximizeKey = None; }''')
        self.write("GNUstep/Defaults/WMRootMenu", '''("Big WindowMaker",
            ("Terminal", SHORTCUT, "Control+Mod1+t", EXEC, "big-wm-open terminal"),
            ("Sub", ("Clip", SHORTCUT, "Mod4+v", EXEC, "big-wm-open clipboard")))''')
        found = dict(system.shortcuts(system.X11, self.env))
        self.assertEqual(found["switch"], "Alt+Tab")
        self.assertEqual(found["close"], "Alt+F4")
        self.assertEqual(found["terminal"], "Ctrl+Alt+T")
        self.assertEqual(found["clipboard"], "Super+V")
        self.assertNotIn("maximize", found)

    def test_pretty_combo(self):
        self.assertEqual(system.pretty_combo("Shift+Alt+ISO_Left_Tab"), "Alt+Shift+Tab")
        self.assertEqual(system.pretty_combo("Logo+Ctrl+Return"), "Ctrl+Super+Enter")


class PreferencesTest(unittest.TestCase):
    def test_first_login_then_only_if_asked(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "big-wm", "welcome.conf")
            prefs = system.Preferences(path)
            self.assertTrue(system.should_autostart(prefs))
            self.assertFalse(system.should_autostart(prefs, live=True))
            prefs.set("seen", True)
            prefs = system.Preferences(path)
            self.assertFalse(system.should_autostart(prefs))
            prefs.set("show-at-login", True)
            self.assertTrue(system.should_autostart(system.Preferences(path)))


class SafetyTest(unittest.TestCase):
    def test_set_default_browser_rejects_odd_names(self):
        for bad in ("", "a b.desktop", "../x.desktop", "x;rm -rf ~.desktop", "x"):
            self.assertFalse(system.set_default_browser(bad))

    def test_spawn_missing_program(self):
        self.assertFalse(system.spawn(["no-such-program-big-wm"]))

    def test_no_topic_shuts_down(self):
        for page in pages.PAGES:
            for topic in page.topics:
                for argvs in topic.open.values():
                    for argv in argvs:
                        self.assertNotIn(argv[0], ("systemctl", "shutdown", "poweroff",
                                                   "reboot", "loginctl"))


class PagesTest(unittest.TestCase):
    def test_four_pages_with_topics(self):
        self.assertEqual(len(pages.PAGES), 4)
        ids = set()
        for page in pages.PAGES:
            self.assertTrue(page.topics)
            for t in page.topics:
                self.assertNotIn(t.id, ids)
                ids.add(t.id)
                self.assertIn(t.demo or page.art, art.SCENES)

    def test_no_arandr_on_wayland(self):
        for page in pages.PAGES:
            for t in page.topics:
                for argv in t.open.get(system.WAYLAND, []):
                    self.assertNotEqual(argv[0], "arandr")


class ArtTest(unittest.TestCase):
    def test_every_scene_draws(self):
        import cairo
        ctx = {"logo": None, "files": "Files", "window": "Window",
               "workspace": "Workspace", "menu_items": ["A", "B"], "right_click": "x",
               "combos": [("Switch", "Alt+Tab")], "no_shortcuts": "-", "light": "L",
               "dark": "D", "tabs": ["A", "B"], "zones": "z", "tiling": "t",
               "clip_samples": ["a", "b", "c"], "editor": "E", "shutdown_q": "?",
               "shutdown_buttons": ["a", "b", "c"], "shutdown_note": "n",
               "dockapps": [(p, n) for p, n in pages.DOCKAPPS], "no_dockapps": "-",
               "tribute": "x", "browser": None}
        for w, h in ((680, 220), (420, 160)):
            surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, w, h)
            cr = cairo.Context(surface)
            for name in art.SCENES:
                for t in (0.0, 0.2, 0.45, 0.7, 0.99):
                    art.draw(cr, w, h, name, t, ctx, "Title", "Subtitle")


if __name__ == "__main__":
    unittest.main()
