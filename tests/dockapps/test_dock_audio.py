#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# big-wm-dock-audio: volume (wmpulsemixer) e música (wmusic) logo abaixo da
# bandeja, nas contas existentes (X11: WMState; Wayland: Dockapps.plist e
# State.plist) e na disposição recomendada pedida pelo menu.
#
#   python3 -I tests/dockapps/test_dock_audio.py   (da raiz do repositório)

import os
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HELPER = os.path.join(ROOT, "session", "lib", "big-wm-dock-audio")
PLISTCHECK = os.path.join(ROOT, "tests", "plistcheck.py")


def app(name, command, pos, autolaunch="Yes"):
    return (f"      {{\n        Name = {name};\n        Command = \"{command}\";\n"
            f"        AutoLaunch = {autolaunch};\n        Lock = Yes;\n        Position = \"{pos}\";\n      }}")


def wmstate(dock, clip=(), per_height=None):
    clip_apps = ",\n".join(a.replace("\n      ", "\n              ").replace("      {", "              {", 1)
                           for a in clip)
    extra = ""
    if per_height:
        key, apps = per_height
        extra = f"    Applications{key} = (\n" + ",\n".join(apps) + "\n    );\n"
    return ("{\n  Dock = {\n    Position = \"100000,0\";\n    Applications = (\n"
            + ",\n".join(dock) + "\n    );\n" + extra + "  };\n"
            "  Clip = {\n    Name = Logo.WMClip;\n    Position = \"0,0\";\n  };\n"
            "  Workspaces = (\n    {\n      Name = \"1\";\n      Clip = {\n        Applications = (\n"
            + clip_apps + "\n        );\n      };\n    }\n  );\n}\n")


LOGO = app("Logo.WMDock", "-", "0,0", "No")
CLOCK = app("wmclock.WMClock", "big-wm-clock", "0,1")
TRAY = app("wmsystemtray0.wmsystemtray", "big-wm-systemtray", "0,2")
WMIX = app("wmix.wmix", "big-wm-mixer", "0,3")
CPU = app("wmcpuload.DockApp", "wmcpuload", "0,4")
MEM = app("wmmemload.DockApp", "wmmemload", "0,5")


def dock_positions(text, key="Applications"):
    """Nome → posição das entradas de uma lista do Dock (o do topo)."""
    m = re.search(r"^  Dock = \{$(.*?)^  \};$", text, re.M | re.S)
    a = re.search(r"^    " + key + r" = \($(.*?)^    \);$", m.group(1), re.M | re.S)
    out = {}
    for e in re.finditer(r"\{[^{}]*\}", a.group(1)):
        name = re.search(r"Name = ([^;]+);", e.group(0)).group(1).strip('"')
        pos = re.search(r'Position = "([^"]+)";', e.group(0)).group(1)
        out.setdefault(name, []).append(pos)
    return out


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="dock-audio.")
        self.gnustep = os.path.join(self.tmp, "GNUstep")
        self.config = os.path.join(self.tmp, "config")
        os.makedirs(os.path.join(self.gnustep, "Defaults"))
        os.makedirs(os.path.join(self.config, "wlmaker"))
        self.state = os.path.join(self.gnustep, "Defaults", "WMState")

    def tearDown(self):
        subprocess.run(["rm", "-rf", self.tmp], check=True)

    def run_helper(self, *args, height="1080"):
        env = dict(os.environ, WMAKER_USER_ROOT=self.gnustep, XDG_CONFIG_HOME=self.config,
                   BIG_WM_SCREEN_HEIGHT=height, LANG="C", BIG_WM_DOCK_AUDIO_NOTIFY="0")
        env.pop("DISPLAY", None)
        return subprocess.run([sys.executable, "-I", HELPER, *args], env=env,
                              capture_output=True, text=True, check=True)

    def write(self, path, text):
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)

    def read(self, path):
        with open(path, encoding="utf-8") as f:
            return f.read()

    def assert_wings(self, path):
        r = subprocess.run([sys.executable, "-I", PLISTCHECK, "--wings", path], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


class X11(Base):
    def test_default_layout(self):
        self.write(self.state, wmstate([LOGO, CLOCK, TRAY, WMIX, CPU, MEM]))
        self.run_helper("x11")
        pos = dock_positions(self.read(self.state))
        self.assertEqual(pos["wmsystemtray0.wmsystemtray"], ["0,2"])
        self.assertEqual(pos["wmpulsemixer.WmPulseMixer"], ["0,3"])
        self.assertEqual(pos["wmusic.DockApp"], ["0,4"])
        self.assertEqual(pos["wmcpuload.DockApp"], ["0,5"])
        self.assertEqual(pos["wmmemload.DockApp"], ["0,6"])
        self.assertNotIn("wmix.wmix", pos)
        text = self.read(self.state)
        self.assertIn('Command = "wmpulsemixer -w";', text)
        self.assertIn('Command = "wmusic -c \\"big-wm-open music\\"";', text)
        self.assert_wings(self.state)
        # Uma vez: o usuário que mexer depois não vê voltar.
        before = text
        self.run_helper("x11")
        self.assertEqual(self.read(self.state), before)

    def test_user_order_kept(self):
        # Volume no lugar 10, bandeja no 12 (como reorganizado pelo usuário).
        dock = [LOGO, app("wmnet.WMNET", "wmnet", "0,5"), app("wmix.wmix", "big-wm-mixer", "0,10"),
                app("wmclock.WMClock", "big-wm-clock", "0,11"),
                app("wmsystemtray0.wmsystemtray", "big-wm-systemtray", "0,12")]
        self.write(self.state, wmstate(dock))
        self.run_helper("x11")
        pos = dock_positions(self.read(self.state))
        self.assertEqual(pos["wmnet.WMNET"], ["0,5"])
        self.assertEqual(pos["wmpulsemixer.WmPulseMixer"], ["0,10"])
        self.assertEqual(pos["wmusic.DockApp"], ["0,11"])
        self.assertEqual(pos["wmclock.WMClock"], ["0,12"])
        self.assertEqual(pos["wmsystemtray0.wmsystemtray"], ["0,13"])
        self.assert_wings(self.state)

    def test_volume_removed_by_user(self):
        text = wmstate([LOGO, CLOCK, TRAY, CPU])
        self.write(self.state, text)
        self.run_helper("x11")
        self.assertEqual(self.read(self.state), text)
        self.assertTrue(os.path.exists(os.path.join(self.config, "big-wm", "migrations",
                                                    "dock-audio-2026-11-x11")))

    def test_wmusic_already_in_clip(self):
        clip = [app("wmusic.DockApp", "wmusic", "0,1")]
        self.write(self.state, wmstate([LOGO, CLOCK, TRAY, WMIX, CPU], clip))
        self.run_helper("x11")
        text = self.read(self.state)
        pos = dock_positions(text)
        self.assertEqual(pos["wmpulsemixer.WmPulseMixer"], ["0,3"])
        self.assertNotIn("wmusic.DockApp", pos)
        self.assertEqual(text.count("wmusic.DockApp"), 1)
        self.assert_wings(self.state)

    def test_screen_too_short(self):
        dock = [LOGO] + [app(f"d{i}.DockApp", f"d{i}", f"0,{i}") for i in range(1, 11)]
        dock[3] = app("wmix.wmix", "big-wm-mixer", "0,3")
        self.write(self.state, wmstate(dock))
        r = self.run_helper("x11", height="720")      # 11 ladrilhos = 715
        pos = dock_positions(self.read(self.state))
        self.assertEqual(pos["wmpulsemixer.WmPulseMixer"], ["0,3"])
        self.assertNotIn("wmusic.DockApp", pos)
        self.assertIn("não cabe", r.stderr)
        self.assertFalse(os.path.exists(os.path.join(self.config, "big-wm", "migrations",
                                                     "dock-audio-2026-11-x11")))

    def test_lists_per_screen_height(self):
        # O Window Maker grava uma lista por altura de tela além da comum:
        # cada uma muda sozinha; a de 600 linhas não tem lugar para o wmusic.
        tall = [LOGO, CLOCK, TRAY, WMIX, CPU, MEM]
        short = [LOGO] + [app(f"d{i}.DockApp", f"d{i}", f"0,{i}") for i in range(1, 9)]
        short[3] = app("wmix.wmix", "big-wm-mixer", "0,3")
        self.write(self.state, wmstate(tall, per_height=("600", short)))
        self.run_helper("x11")
        text = self.read(self.state)
        pos = dock_positions(text)
        self.assertEqual(pos["wmpulsemixer.WmPulseMixer"], ["0,3"])
        self.assertEqual(pos["wmusic.DockApp"], ["0,4"])
        self.assertEqual(pos["wmcpuload.DockApp"], ["0,5"])
        small = dock_positions(text, "Applications600")
        self.assertEqual(small["wmpulsemixer.WmPulseMixer"], ["0,3"])
        self.assertNotIn("wmusic.DockApp", small)
        self.assertEqual(small["d4.DockApp"], ["0,4"])
        self.assert_wings(self.state)
        # Sem lugar numa das listas: tenta de novo no próximo login.
        self.assertFalse(os.path.exists(os.path.join(self.config, "big-wm", "migrations",
                                                     "dock-audio-2026-11-x11")))

    def test_restore(self):
        dock = [LOGO, app("wmusic.DockApp", "wmusic", "0,1"), CLOCK, TRAY, CPU,
                app("wmix.wmix", "big-wm-mixer", "0,7")]
        clip = [app("wmpulsemixer.WmPulseMixer", "wmpulsemixer -w", "0,1")]
        self.write(self.state, wmstate(dock, clip))
        self.run_helper("--restore")
        self.run_helper("x11")
        text = self.read(self.state)
        pos = dock_positions(text)
        self.assertEqual(pos["wmsystemtray0.wmsystemtray"], ["0,2"])
        self.assertEqual(pos["wmpulsemixer.WmPulseMixer"], ["0,3"])
        self.assertEqual(pos["wmusic.DockApp"], ["0,4"])
        self.assertEqual(pos["wmcpuload.DockApp"], ["0,5"])
        self.assertNotIn("wmix.wmix", pos)
        self.assertEqual(text.count("wmpulsemixer.WmPulseMixer"), 1)
        self.assertEqual(text.count("wmusic.DockApp"), 1)
        self.assert_wings(self.state)
        # O pedido vale uma vez por sessão; o Wayland ainda tem o dele.
        self.assertEqual(self.read(os.path.join(self.config, "big-wm", "dock-audio-restore")), "wayland\n")


class Wayland(Base):
    def dockapps(self):
        return os.path.join(self.config, "wlmaker", "Dockapps.plist")

    def lists(self):
        text = self.read(self.dockapps())
        out = {}
        for m in re.finditer(r'("?[A-Za-z0-9_.\-]+"?) = \((.*?)\);', text, re.S):
            out[m.group(1).strip('"')] = [t.strip().strip('"') for t in m.group(2).split(",") if t.strip()]
        return out

    def test_replace_and_insert(self):
        self.write(self.dockapps(), '{\n  Dock = (\n    "big-wm-tray",\n    wmix,\n    wmclock\n  );\n'
                   '  Order = (wlmeyes, wlmmixer);\n  Removed = (wmfsm);\n'
                   '  AfterLaunchers = {\n    Dock = {\n      wmix = 2;\n    };\n  };\n}\n')
        self.run_helper("wayland")
        lists = self.lists()
        self.assertEqual(lists["Dock"], ["big-wm-tray", "wmpulsemixer", "wmusic", "wmclock"])
        self.assertEqual(lists["Order"], ["wlmeyes", "wlmmixer"])
        self.assertEqual(lists["Removed"], ["wmfsm"])
        text = self.read(self.dockapps())
        self.assertRegex(text, r"wmpulsemixer = 2;")
        self.assertRegex(text, r"wmusic = 2;")
        self.assertNotIn("wmix", text)
        r = subprocess.run([sys.executable, "-I", PLISTCHECK, self.dockapps()], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_volume_removed_by_user(self):
        self.write(self.dockapps(), '{\n  Dock = ("big-wm-tray", wmclock);\n  Removed = (wmix);\n}\n')
        self.run_helper("wayland")
        lists = self.lists()
        self.assertEqual(lists["Dock"], ["big-wm-tray", "wmclock"])
        self.assertEqual(lists["Removed"], ["wmix", "wmpulsemixer", "wmusic"])

    def test_state_plist_default_list(self):
        state = os.path.join(self.config, "wlmaker", "State.plist")
        self.write(state, '{\n  Dock = {\n    Dockapps = (wmclock, "big-wm-tray", wmix, wmcpuload, wmmemload, '
                   'wmnet, "wmake-zones-wl", wmcliphist, wmshutdown, wlmeyes);\n  };\n}\n')
        self.run_helper("wayland")
        self.assertIn('Dockapps = (wmclock, "big-wm-tray", wmpulsemixer, wmusic, wmcpuload, wmmemload, '
                      'wmnet, "wmake-zones-wl", wmcliphist, wmshutdown, wlmeyes);', self.read(state))

    def test_state_plist_rewritten(self):
        # Regravado pelo big-wm-dock-settings: várias linhas, sem comentários.
        state = os.path.join(self.config, "wlmaker", "State.plist")
        self.write(state, '{\n  Clip = {\n    Dockapps = (\n      wlmmixer\n    );\n  };\n'
                   '  Dock = {\n    Dockapps = (\n      wmclock,\n      "big-wm-tray",\n      wmix,\n'
                   '      wmnet\n    );\n    Edge = RIGHT;\n  };\n}\n')
        self.run_helper("wayland")
        text = self.read(state)
        dock = re.search(r"Dock = \{\s*Dockapps = \((.*?)\);", text, re.S).group(1)
        self.assertEqual([t.strip().strip('"') for t in dock.split(",")],
                         ["wmclock", "big-wm-tray", "wmpulsemixer", "wmusic", "wmnet"])
        clip = re.search(r"Clip = \{\s*Dockapps = \((.*?)\);", text, re.S).group(1)
        self.assertEqual(clip.strip(), "wlmmixer")
        self.assertIn("Edge = RIGHT;", text)

    def test_restore(self):
        self.write(self.dockapps(), '{\n  Dock = ("big-wm-tray", wmclock, wmusic);\n'
                   '  Order = (wmpulsemixer);\n  Removed = (wmix);\n}\n')
        self.run_helper("--restore")
        self.run_helper("wayland")
        lists = self.lists()
        self.assertEqual(lists["Dock"], ["big-wm-tray", "wmpulsemixer", "wmusic", "wmclock"])
        self.assertEqual(lists.get("Order", []), [])
        self.assertEqual(lists.get("Removed", []), [])


if __name__ == "__main__":
    unittest.main(verbosity=1)
