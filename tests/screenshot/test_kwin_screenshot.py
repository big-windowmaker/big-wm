#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# big-wm-kwin-screenshot (o org.kde.KWin.ScreenShot2 do Spectacle no wlmaker)
# com um grim e um wmake-zones falsos, num barramento D-Bus próprio:
#   - a resposta de CaptureScreen/CaptureActiveScreen traz a saída
#     ("screen"): sem ela o Spectacle juntava "Todas as telas" uma sobre a
#     outra;
#   - "Tela atual" é a saída do ponteiro (comando "pointer"), ou, com um
#     wlmaker que não o tem, a da janela ativa;
#   - geometrias com coordenadas negativas; a janela ativa primeiro na
#     escolha; janelas fora das telas ficam de fora.
#
#   python3 -I tests/screenshot/test_kwin_screenshot.py   (da raiz)

import importlib.machinery
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCRIPT = os.path.join(ROOT, "session", "bin", "big-wm-kwin-screenshot")

ZONES = r"""#!/bin/sh
case "$1" in
status) cat <<E
backend=wlmaker
monitor=HDMI-A-1 3440x1440+0+0 work=3440x1440+0+0 layout=halves autotile=off scale=1.00
monitor=DP-1 3440x1440+3440+0 work=3312x1328+3504+0 layout=halves autotile=off scale=1.00
monitor=DP-2 2560x1080+6880+0 work=2560x1080+6880+0 layout=halves autotile=off scale=1.25
E
;;
pointer) [ -n "$FAKE_POINTER" ] && echo "$FAKE_POINTER" || { echo "error=comando desconhecido: pointer"; exit 1; } ;;
active) echo "800x600+7000+100	Ativa" ;;
windows) printf 'DP-1\tzone=-1\ttiled=0\t1519x985+5231-2\tA\tUma\n'
         printf 'DP-2\tzone=-1\ttiled=0\t800x600+7000+100\tB\tAtiva\n'
         printf '?\tzone=-1\ttiled=0\t300x200-32000-32000\tC\tMinimizada\n' ;;
esac
"""

# PPM do tamanho da saída pedida (-o NOME), ou 64x32
GRIM = r"""#!/bin/sh
w=64 h=32
while [ $# -gt 0 ]; do
  case "$1" in
  -o) case "$2" in DP-2) w=40 h=20 ;; DP-1) w=30 h=10 ;; esac; shift ;;
  esac; shift
done
printf 'P6\n%s %s\n255\n' $w $h
head -c $((w*h*3)) /dev/zero | tr '\0' '\001'
"""


def load():
    # sem __pycache__ em session/bin (o make install e o lint o pegariam)
    sys.dont_write_bytecode = True
    loader = importlib.machinery.SourceFileLoader("kwinss", SCRIPT)
    spec = importlib.util.spec_from_loader("kwinss", loader)
    mod = importlib.util.module_from_spec(spec)
    loader.exec_module(mod)
    return mod


class Base(unittest.TestCase):
    def setUp(self):
        self.t = tempfile.mkdtemp()
        for name, text in (("wmake-zones", ZONES), ("grim", GRIM)):
            p = os.path.join(self.t, name)
            with open(p, "w") as f:
                f.write(text)
            os.chmod(p, 0o755)
        self.env = dict(os.environ, PATH=self.t + ":/usr/bin:/bin")
        self.old_path = os.environ["PATH"]
        os.environ["PATH"] = self.env["PATH"]
        os.environ.pop("FAKE_POINTER", None)

    def tearDown(self):
        os.environ["PATH"] = self.old_path
        os.environ.pop("FAKE_POINTER", None)
        shutil.rmtree(self.t)


class Functions(Base):
    def test_geometry(self):
        m = load()
        self.assertEqual(m.rect_of("1519x985+5231-2"), (5231, -2, 1519, 985))
        self.assertEqual(m.box_of("10x20-5+7"), "-5,7 10x20")
        self.assertIsNone(m.box_of("lixo"))

    def test_outputs_and_scale(self):
        m = load()
        self.assertEqual([o[0] for o in m.outputs()], ["HDMI-A-1", "DP-1", "DP-2"])
        self.assertEqual(m.output_at(3440, 0), "DP-1")
        self.assertEqual(m.output_scale("DP-2"), 1.25)

    def test_pointer_output(self):
        m = load()
        os.environ["FAKE_POINTER"] = "4000 300 DP-1 3440x1440+3440+0"
        self.assertEqual(m.pointer_output(), "DP-1")
        # wlmaker sem o comando: a saída da janela ativa (7000,100 -> DP-2)
        os.environ.pop("FAKE_POINTER")
        self.assertEqual(m.pointer_output(), "DP-2")

    def test_window_boxes(self):
        m = load()
        # a ativa primeiro; a de y negativo vale; a minimizada fica de fora
        self.assertEqual(m.window_boxes(), ["7000,100 800x600", "5231,-2 1519x985"])


class Bus(Base):
    """O serviço de verdade num dbus-daemon próprio, chamado pelo Gio."""

    def call(self, method, args):
        import gi
        gi.require_version("Gio", "2.0")
        from gi.repository import Gio, GLib
        bus = Gio.DBusConnection.new_for_address_sync(
            self.address, Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT |
            Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION, None, None)
        r, w = os.pipe()
        fds = Gio.UnixFDList.new()
        idx = fds.append(w)
        os.close(w)
        sig = {"CaptureScreen": "(sa{sv}h)", "CaptureActiveScreen": "(a{sv}h)",
               "CaptureWorkspace": "(a{sv}h)"}[method]
        params = GLib.Variant(sig, tuple(args) + (idx,))
        for _ in range(50):
            try:
                ret, _out = bus.call_with_unix_fd_list_sync(
                    "org.kde.KWin.ScreenShot2", "/org/kde/KWin/ScreenShot2",
                    "org.kde.KWin.ScreenShot2", method, params, None,
                    Gio.DBusCallFlags.NONE, 5000, fds, None)
                break
            except GLib.Error as e:
                if "ServiceUnknown" not in str(e) and "UnknownObject" not in str(e):
                    raise
                time.sleep(0.1)
        # a lista guarda uma cópia da ponta de escrita: sem fechá-la, a
        # leitura não termina
        for fd in fds.steal_fds():
            os.close(fd)
        meta = ret.unpack()[0]
        data = b""
        while True:
            chunk = os.read(r, 65536)
            if not chunk:
                break
            data += chunk
        os.close(r)
        return meta, data

    def setUp(self):
        super().setUp()
        self.dbus = subprocess.Popen(["dbus-daemon", "--session", "--nofork", "--print-address=1"],
                                     stdout=subprocess.PIPE, text=True)
        self.address = self.dbus.stdout.readline().strip()
        env = dict(self.env, DBUS_SESSION_BUS_ADDRESS=self.address, XDG_CURRENT_DESKTOP="wlmaker",
                   WAYLAND_DISPLAY="wayland-teste")
        self.service = subprocess.Popen([sys.executable, "-Is", SCRIPT], env=env)

    def tearDown(self):
        self.service.terminate()
        self.service.wait(5)
        self.dbus.terminate()
        self.dbus.wait(5)
        self.dbus.stdout.close()
        super().tearDown()

    def test_screen_key_and_bytes(self):
        meta, data = self.call("CaptureScreen", ("DP-2", {}))
        self.assertEqual(meta["screen"], "DP-2")
        self.assertEqual((meta["width"], meta["height"], meta["stride"]), (40, 20, 160))
        self.assertEqual(meta["scale"], 1.25)
        self.assertEqual(len(data), 40 * 20 * 4)

    def test_active_screen_is_one_screen(self):
        # sem o "pointer" do wlmaker: a saída da janela ativa (DP-2), não tudo
        meta, data = self.call("CaptureActiveScreen", ({},))
        self.assertEqual(meta["screen"], "DP-2")
        self.assertEqual((meta["width"], meta["height"]), (40, 20))
        self.assertEqual(len(data), 40 * 20 * 4)

    def test_workspace_is_everything(self):
        meta, data = self.call("CaptureWorkspace", ({},))
        self.assertNotIn("screen", meta)
        self.assertEqual((meta["width"], meta["height"]), (64, 32))
        self.assertEqual(len(data), 64 * 32 * 4)


if __name__ == "__main__":
    unittest.main()
