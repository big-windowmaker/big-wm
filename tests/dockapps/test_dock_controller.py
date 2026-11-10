#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# big-wm-dock-controller: o controlador do Dock com Abas logo acima da
# bandeja nas contas existentes (X11 e Wayland).
#
#   python3 -I tests/dockapps/test_dock_controller.py   (da raiz do repositório)

import os
import re
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from test_dock_audio import (LOGO, CLOCK, TRAY, WMIX, CPU, Base, app, dock_positions,  # noqa: E402
                             wmstate)

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HELPER = os.path.join(ROOT, "session", "lib", "big-wm-dock-controller")
CTRL = "big-wm-tabbed-dock-dockapp.DockApp"


class Controller(Base):
    def run_ctrl(self, *args, height="1080"):
        env = dict(os.environ, WMAKER_USER_ROOT=self.gnustep, XDG_CONFIG_HOME=self.config,
                   BIG_WM_SCREEN_HEIGHT=height, LANG="C",
                   BIG_WM_DOCK_AUDIO=os.path.join(ROOT, "session", "lib", "big-wm-dock-audio"))
        env.pop("DISPLAY", None)
        return subprocess.run([sys.executable, "-I", HELPER, *args], env=env,
                              capture_output=True, text=True, check=True)

    def test_x11_above_tray(self):
        self.write(self.state, wmstate([LOGO, CLOCK, TRAY, WMIX, CPU]))
        self.run_ctrl("x11")
        pos = dock_positions(self.read(self.state))
        self.assertEqual(pos[CTRL], ["0,2"])
        self.assertEqual(pos["wmsystemtray0.wmsystemtray"], ["0,3"])
        self.assertEqual(pos["wmix.wmix"], ["0,4"])
        self.assertEqual(pos["wmcpuload.DockApp"], ["0,5"])
        self.assertEqual(pos["wmclock.WMClock"], ["0,1"])
        self.assertIn('Command = "big-wm-tabbed-dock-dockapp";', self.read(self.state))
        self.assert_wings(self.state)
        # uma vez: tirado pelo usuário depois, não volta
        text = re.sub(r',\n      \{\n        Name = "?big-wm-tabbed-dock-dockapp\.DockApp"?;.*?\n      \}', "",
                      self.read(self.state), flags=re.S)
        self.write(self.state, text)
        self.run_ctrl("x11")
        self.assertNotIn(CTRL, self.read(self.state))

    def test_x11_user_order_and_gap(self):
        # bandeja no lugar 12, um vago no 14: só a bandeja e o 13 descem
        dock = [LOGO, app("wmnet.WMNET", "wmnet", "0,5"),
                app("wmsystemtray0.wmsystemtray", "big-wm-systemtray", "0,12"),
                app("wmix.wmix", "big-wm-mixer", "0,13"), app("wmclock.WMClock", "big-wm-clock", "0,15")]
        self.write(self.state, wmstate(dock))
        self.run_ctrl("x11")
        pos = dock_positions(self.read(self.state))
        self.assertEqual(pos[CTRL], ["0,12"])
        self.assertEqual(pos["wmsystemtray0.wmsystemtray"], ["0,13"])
        self.assertEqual(pos["wmix.wmix"], ["0,14"])
        self.assertEqual(pos["wmclock.WMClock"], ["0,15"])
        self.assertEqual(pos["wmnet.WMNET"], ["0,5"])
        self.assert_wings(self.state)

    def test_x11_already_and_no_tray(self):
        clip = [app(CTRL, "big-wm-tabbed-dock-dockapp", "0,1")]
        text = wmstate([LOGO, CLOCK, TRAY], clip)
        self.write(self.state, text)
        self.run_ctrl("x11")
        self.assertEqual(self.read(self.state), text)
        os.unlink(os.path.join(self.config, "big-wm", "migrations", "dock-controller-2026-11-x11"))
        text = wmstate([LOGO, CLOCK, CPU])
        self.write(self.state, text)
        self.run_ctrl("x11")
        self.assertEqual(self.read(self.state), text)

    def test_x11_screen_too_short(self):
        dock = [LOGO] + [app(f"d{i}.DockApp", f"d{i}", f"0,{i}") for i in range(1, 11)]
        dock[2] = app("wmsystemtray0.wmsystemtray", "big-wm-systemtray", "0,2")
        text = wmstate(dock)
        self.write(self.state, text)
        r = self.run_ctrl("x11", height="720")
        self.assertEqual(self.read(self.state), text)
        self.assertIn("não cabe", r.stderr)
        self.assertFalse(os.path.exists(os.path.join(self.config, "big-wm", "migrations",
                                                     "dock-controller-2026-11-x11")))

    def test_wayland(self):
        dockapps = os.path.join(self.config, "wlmaker", "Dockapps.plist")
        self.write(dockapps, '{\n  Dock = ("big-wm-tray", wmpulsemixer, wmclock);\n  Removed = (wmfsm);\n'
                   '  AfterLaunchers = {\n    Dock = {\n      "big-wm-tray" = 2;\n    };\n  };\n}\n')
        state = os.path.join(self.config, "wlmaker", "State.plist")
        self.write(state, '{\n  Dock = {\n    Dockapps = (wmclock, "big-wm-tray", wmpulsemixer);\n  };\n}\n')
        self.run_ctrl("wayland")
        text = self.read(dockapps)
        dock = re.search(r"Dock = \((.*?)\);", text, re.S).group(1)
        self.assertEqual([t.strip().strip('"') for t in dock.split(",")],
                         ["big-wm-tabbed-dock-dockapp", "big-wm-tray", "wmpulsemixer", "wmclock"])
        self.assertRegex(text, r'"big-wm-tabbed-dock-dockapp" = 2;')
        self.assertIn('Dockapps = (wmclock, "big-wm-tabbed-dock-dockapp", "big-wm-tray", wmpulsemixer);',
                      self.read(state))

    def test_wayland_removed(self):
        dockapps = os.path.join(self.config, "wlmaker", "Dockapps.plist")
        text = '{\n  Dock = ("big-wm-tray", wmclock);\n  Removed = ("big-wm-tabbed-dock-dockapp");\n}\n'
        self.write(dockapps, text)
        self.run_ctrl("wayland")
        self.assertEqual(self.read(dockapps), text)


if __name__ == "__main__":
    unittest.main(verbosity=1)
