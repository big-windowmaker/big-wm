#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# big-wm-doctor numa conta e num /proc falsos com os defeitos achados na
# auditoria de 2026-11-10: IconSize 48, estilo claro no modo escuro (salvo
# pelo WPrefs, sem o IconBack), autostart sem permissão de execução,
# temporários do WINGs, um binário substituído e um dockapp em dobro.
#
#   python3 -I tests/doctor/test_doctor.py   (da raiz do repositório)

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOCTOR = os.path.join(ROOT, "session", "bin", "big-wm-doctor")
STYLES = os.path.join(ROOT, "themes", "WindowMaker", "Styles")


class Doctor(unittest.TestCase):
    def setUp(self):
        self.t = tempfile.mkdtemp()
        self.gs = os.path.join(self.t, "GNUstep")
        self.cfg = os.path.join(self.t, "config")
        self.proc = os.path.join(self.t, "proc")
        os.makedirs(os.path.join(self.gs, "Defaults"))
        os.makedirs(os.path.join(self.gs, "Library", "WindowMaker"))
        os.makedirs(os.path.join(self.cfg, "big-wm"))
        os.makedirs(self.proc)
        self.glob = os.path.join(self.t, "etc-wm")
        os.makedirs(self.glob)
        with open(os.path.join(self.glob, "WindowMaker"), "w") as f:
            f.write('{\n  IconBack = (dgradient, "rgb:a6/a6/b6", "rgb:51/55/61");\n}\n')

    def tearDown(self):
        shutil.rmtree(self.t)

    def write(self, path, text, mode=0o644):
        with open(path, "w") as f:
            f.write(text)
        os.chmod(path, mode)

    def fake_proc(self, pid, exe, comm, env):
        d = os.path.join(self.proc, str(pid))
        os.makedirs(d)
        os.symlink(exe, os.path.join(d, "exe"))
        self.write(os.path.join(d, "comm"), comm + "\n")
        with open(os.path.join(d, "environ"), "wb") as f:
            f.write(b"\0".join(e.encode() for e in env) + b"\0")

    def doctor(self, *args):
        env = dict(os.environ, WMAKER_USER_ROOT=self.gs, XDG_CONFIG_HOME=self.cfg, XDG_CACHE_HOME=os.path.join(self.t, "cache"),
                   BIGWM_DOCTOR_PROC=self.proc, BIGWM_LIBDIR=os.path.join(ROOT, "session", "lib"),
                   BIGWM_WMSTYLES=STYLES, BIGWM_WM_GLOBAL=self.glob, LANG="C", PATH="/usr/bin:/bin")
        env.pop("LC_ALL", None)
        r = subprocess.run([sys.executable, "-I", DOCTOR, "--json", *args], env=env, capture_output=True, text=True)
        return r.returncode, json.loads(r.stdout)

    def level(self, report, cid):
        return next((c["level"] for c in report["checks"] if c["id"] == cid), None)

    def test_finds_the_audit_problems(self):
        defaults = os.path.join(ROOT, "data", "GNUstep", "Defaults", "WindowMaker")
        with open(defaults) as f:
            text = "".join(line for line in f if "IconBack =" not in line)
        text = text.replace("{", "{\n  IconSize = 48;", 1)
        self.write(os.path.join(self.gs, "Defaults", "WindowMaker"), text)
        self.write(os.path.join(self.cfg, "big-wm", "theme"), "dark\n")
        self.write(os.path.join(self.gs, "Library", "WindowMaker", "autostart"), "xset s 1800\n")
        for name in ("WMState.Ab12Cd", "WMState.Zz9Yy8", "WindowMaker.Qw3Er4"):
            self.write(os.path.join(self.gs, "Defaults", name), "")
        self.write(os.path.join(self.gs, "Defaults", "WMState.bak-dock"), "")  # não é um temporário
        self.fake_proc(101, "/usr/bin/wmfoo (deleted)", "wmfoo", ["DISPLAY=:9"])
        self.fake_proc(102, "/usr/bin/wmbar", "wmbar", ["DISPLAY=:9"])
        self.fake_proc(103, "/usr/bin/wmbar", "wmbar", ["DISPLAY=:9.0"])
        self.fake_proc(104, "/usr/bin/wmbar", "wmbar", ["DISPLAY=:7"])
        code, rep = self.doctor()
        self.assertEqual(code, 1)
        self.assertEqual(self.level(rep, "iconsize"), "warn")
        self.assertEqual(self.level(rep, "wmstyle"), "warn")
        self.assertEqual(self.level(rep, "autostart"), "warn")
        self.assertEqual(self.level(rep, "wmstate-temps"), "info")
        temps = next(c for c in rep["checks"] if c["id"] == "wmstate-temps")["data"]["files"]
        self.assertEqual(temps, ["WMState.Ab12Cd", "WMState.Zz9Yy8", "WindowMaker.Qw3Er4"])
        self.assertEqual(self.level(rep, "stale"), "warn")
        dups = next(c for c in rep["checks"] if c["id"] == "duplicates")
        self.assertEqual(dups["level"], "warn")
        self.assertEqual(list(dups["data"]["duplicates"]), ["wmbar (:9)"])
        # só lê: nada mudou
        self.assertTrue(os.path.exists(os.path.join(self.gs, "Defaults", "WMState.Ab12Cd")))
        # --repair: os temporários vão para o cache, não são apagados
        code, rep = self.doctor("--repair")
        self.assertEqual(len(rep["repaired"]), 1)
        self.assertFalse(os.path.exists(os.path.join(self.gs, "Defaults", "WMState.Ab12Cd")))
        moved = [f for _d, _s, fs in os.walk(os.path.join(self.t, "cache", "big-wm", "doctor")) for f in fs]
        self.assertEqual(sorted(moved), ["WMState.Ab12Cd", "WMState.Zz9Yy8", "WindowMaker.Qw3Er4"])
        self.assertTrue(os.path.exists(os.path.join(self.gs, "Defaults", "WMState.bak-dock")))

    def test_healthy(self):
        self.write(os.path.join(self.gs, "Defaults", "WindowMaker"),
                   open(os.path.join(ROOT, "data", "GNUstep", "Defaults", "WindowMaker")).read())
        self.write(os.path.join(self.cfg, "big-wm", "theme"), "light\n")
        _code, rep = self.doctor()
        for cid in ("iconsize", "wmstyle", "duplicates", "stale"):
            self.assertEqual(self.level(rep, cid), "ok", cid)
        self.assertIsNone(self.level(rep, "autostart"))
        self.assertIsNone(self.level(rep, "wmstate-temps"))


if __name__ == "__main__":
    unittest.main(verbosity=1)
