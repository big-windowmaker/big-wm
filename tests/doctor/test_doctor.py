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

    def doctor(self, *args, **extra):
        env = dict(os.environ, WMAKER_USER_ROOT=self.gs, XDG_CONFIG_HOME=self.cfg, XDG_CACHE_HOME=os.path.join(self.t, "cache"),
                   BIGWM_DOCTOR_PROC=self.proc, BIGWM_LIBDIR=os.path.join(ROOT, "session", "lib"),
                   BIGWM_WMSTYLES=STYLES, BIGWM_WM_GLOBAL=self.glob, LANG="C", PATH="/usr/bin:/bin")
        env.pop("LC_ALL", None)
        env.update(extra)
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


    def test_wayland_docks(self):
        # Um wlmdock que recomeçou com o tema pelo execv de /proc/self/exe
        # (comm "exe") e o controlador acoplado sem rodar.
        os.makedirs(os.path.join(self.cfg, "wlmaker"))
        self.write(os.path.join(self.cfg, "wlmaker", "Dockapps.plist"),
                   '{\n  Dock = (wmusic, "big-wm-tabbed-dock-dockapp");\n  Removed = (wmfsm);\n}\n')
        self.fake_proc(201, "/usr/bin/wlmdock", "exe", ["WAYLAND_DISPLAY=wayland-0"])
        self.fake_proc(202, "/usr/bin/wlmdock", "wlmdock", ["WAYLAND_DISPLAY=wayland-0"])
        _code, report = self.doctor()
        self.assertEqual(self.level(report, "wayland-docks"), "warn")
        shutil.rmtree(os.path.join(self.proc, "201"))
        self.fake_proc(201, "/usr/bin/wlmdock", "wlmdock", ["WAYLAND_DISPLAY=wayland-0"])
        _code, report = self.doctor()
        check = next(c for c in report["checks"] if c["id"] == "wayland-docks")
        self.assertEqual(check["level"], "warn")
        self.assertFalse(check["data"]["controller_running"])
        self.fake_proc(203, "/usr/bin/big-wm-tabbed-dock-dockapp", "big-wm-tabbed-d", ["DISPLAY=:0"])
        _code, report = self.doctor()
        self.assertEqual(self.level(report, "wayland-docks"), "ok")

    def test_graphics(self):
        # Servidor X sem aceleração 2D (modesetting sem glamor, como numa VM
        # com vídeo 2D): o picom com use-damage deixa pedaços de janelas.
        bin_ = os.path.join(self.t, "bin")
        os.makedirs(bin_)
        self.write(os.path.join(bin_, "xrandr"), '#!/bin/sh\nprintf "%s\\n" "$PROVIDERS"\n', 0o755)
        self.write(os.path.join(bin_, "systemd-detect-virt"), "#!/bin/sh\necho kvm\n", 0o755)
        sys_ = os.path.join(self.t, "sys")
        os.makedirs(os.path.join(sys_, "class", "drm", "card0", "device", "virtio0"))
        os.makedirs(os.path.join(sys_, "bus", "virtio_gpu"))
        os.symlink(os.path.join(sys_, "bus", "virtio_gpu"),
                   os.path.join(sys_, "class", "drm", "card0", "device", "virtio0", "driver"))
        ms2d = "Providers: number : 1\nProvider 0: id: 0x40 cap: 0x2, Sink Output name:modesetting"
        msgl = ("Providers: number : 1\nProvider 0: id: 0x40 cap: 0xf, Source Output, Sink Output, "
                "Source Offload, Sink Offload name:modesetting")
        self.fake_proc(301, "/usr/bin/picom", "picom", ["DISPLAY=:0"])
        self.write(os.path.join(self.proc, "301", "cmdline"), "picom\0--plugins\0zoom.so\0")
        x11 = dict(PATH=bin_ + ":/usr/bin:/bin", BIGWM_DOCTOR_SYS=sys_, DISPLAY=":0", WAYLAND_DISPLAY="",
                   XDG_SESSION_TYPE="x11")

        def graphics(**env):
            _code, rep = self.doctor(**dict(x11, **env))
            return next(c for c in rep["checks"] if c["id"] == "graphics")

        g = graphics(PROVIDERS=ms2d)
        self.assertEqual(g["level"], "warn")
        self.assertEqual(g["data"]["drm_drivers"], ["virtio_gpu"])
        self.assertFalse(g["data"]["x_2d_acceleration"])
        self.write(os.path.join(self.proc, "301", "cmdline"), "picom\0--no-use-damage\0")
        self.assertEqual(graphics(PROVIDERS=ms2d)["level"], "info")
        # Só o picom desta tela conta; um que saiu (sem linha de comando), não.
        self.write(os.path.join(self.proc, "301", "cmdline"), "picom\0")
        self.assertEqual(graphics(PROVIDERS=ms2d, DISPLAY=":1")["level"], "info")
        self.assertEqual(graphics(PROVIDERS=ms2d, DISPLAY=":0.0")["level"], "warn")
        self.write(os.path.join(self.proc, "301", "cmdline"), "")
        self.assertEqual(graphics(PROVIDERS=ms2d)["level"], "info")
        self.write(os.path.join(self.proc, "301", "cmdline"), "picom\0")
        g = graphics(PROVIDERS=msgl)
        self.assertEqual(g["level"], "info")
        self.assertTrue(g["data"]["x_2d_acceleration"])
        # Wayland: o renderizador que o wlmaker escolheu, do log dele
        os.makedirs(os.path.join(self.t, "cache"), exist_ok=True)
        self.write(os.path.join(self.t, "cache", "wlmaker.log"),
                   "x (INFO) render/gles2/renderer.c:541 GL renderer: llvmpipe\n"
                   "x (INFO) render/pixman/renderer.c:328 Creating pixman renderer\n")
        g = graphics(WAYLAND_DISPLAY="wayland-0", XDG_SESSION_TYPE="wayland")
        self.assertEqual((g["level"], g["data"]["renderer"]), ("info", "pixman"))
        self.assertIn("VMSVGA", g["hint"])

    def report(self, *args):
        """--report com journalctl, pacman e cia. falsos que despejam dados
        pessoais (usuário, máquina, IP, MAC, e-mail, home)."""
        import socket
        host = socket.gethostname()
        fake = os.path.join(self.t, "bin")
        os.makedirs(fake, exist_ok=True)
        dirty = (f"{host} app[1]: falhou para fulano em {self.t}/Documentos/x.txt "
                 f"de 192.168.7.2 e fe80:0:0:0:1:2:3:4, placa 3c:7c:3f:aa:bb:cc, fulano.silva@exemplo.com.br")
        for name, out in (("journalctl", dirty), ("pacman", "big-wm-session 26.11.26-1"),
                          ("lspci", '03:00.0 "VGA compatible controller" "AMD" "Navi 44" -rc0 "X" "Y"'),
                          ("systemctl", ""), ("coredumpctl", "")):
            self.write(os.path.join(fake, name), f"#!/bin/sh\necho '{out}'\n", 0o755)
        env = dict(os.environ, HOME=self.t, USER="fulano", WMAKER_USER_ROOT=self.gs, XDG_CONFIG_HOME=self.cfg,
                   XDG_CACHE_HOME=os.path.join(self.t, "cache"), BIGWM_DOCTOR_PROC=self.proc,
                   BIGWM_LIBDIR=os.path.join(ROOT, "session", "lib"), BIGWM_WMSTYLES=STYLES,
                   BIGWM_WM_GLOBAL=self.glob, LANG="C", PATH=fake + ":/usr/bin:/bin")
        for var in ("LC_ALL", "DISPLAY", "WAYLAND_DISPLAY"):
            env.pop(var, None)
        return subprocess.run([sys.executable, "-I", DOCTOR, *args], env=env, capture_output=True, text=True)

    def test_report_has_no_personal_data(self):
        r = self.report("--report", "--print")
        self.assertEqual(r.returncode, 0, r.stderr)
        saved = os.path.join(self.t, "cache", "big-wm", "relatorios")
        files = os.listdir(saved)
        self.assertEqual(len(files), 1)
        path = os.path.join(saved, files[0])
        self.assertEqual(os.stat(path).st_mode & 0o777, 0o600)
        with open(path) as f:
            text = f.read()
        import socket
        for secret in ("fulano", socket.gethostname(), self.t, "192.168.7.2", "fe80:0:0", "3c:7c:3f",
                       "@exemplo"):
            self.assertNotIn(secret, text)
        for mark in ("<user>", "<host>", "~/Documentos/x.txt", "<ip>", "<mac>", "<email>",
                     "big-wm-session 26.11.26-1", "AMD Navi 44"):
            self.assertIn(mark, text)

    def test_report_options(self):
        self.assertEqual(self.report("--print").returncode, 2)
        self.assertEqual(self.report("--report", "--json").returncode, 2)

    def test_issue_url_fits(self):
        from importlib.machinery import SourceFileLoader
        from importlib.util import module_from_spec, spec_from_loader
        sys.dont_write_bytecode = True  # nada de __pycache__ em session/bin (o lint de i18n lê a pasta)
        loader = SourceFileLoader("doctor", DOCTOR)
        doctor = module_from_spec(spec_from_loader("doctor", loader))
        loader.exec_module(doctor)
        url = doctor.issue_url("Sistema: x", "\n".join(f"linha {i} " + "x" * 80 for i in range(400)))
        self.assertLessEqual(len(url), doctor.URL_LIMIT)
        self.assertIn("template=bug.yml", url)
        self.assertIn("system=Sistema", url)


if __name__ == "__main__":
    unittest.main(verbosity=1)
