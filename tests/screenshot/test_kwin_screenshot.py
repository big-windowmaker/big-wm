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

import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# O serviço em C (make kwin-screenshot); outro caminho pelo ambiente.
SERVICE = os.environ.get("BIG_WM_KWIN_SCREENSHOT") or os.path.join(
    ROOT, "session", "kwin-screenshot", "big-wm-kwin-screenshot")

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
echo "$*" >>"$GRIM_LOG"
w=64 h=32
while [ $# -gt 0 ]; do
  case "$1" in
  -o) case "$2" in DP-2) w=40 h=20 ;; DP-1) w=30 h=10 ;; esac; shift ;;
  esac; shift
done
printf 'P6\n%s %s\n255\n' $w $h
head -c $((w*h*3)) /dev/zero | tr '\0' '\001'
"""


# Escolha interativa: grava a entrada (as caixas das janelas) e responde a
# primeira; sai com 1 (Esc) com FAKE_SLURP_CANCEL.
SLURP = r"""#!/bin/sh
cat >"$SLURP_IN"
[ -n "$FAKE_SLURP_CANCEL" ] && exit 1
head -n 1 "$SLURP_IN"
"""


class Base(unittest.TestCase):
    def setUp(self):
        self.t = tempfile.mkdtemp()
        for name, text in (("wmake-zones", ZONES), ("grim", GRIM), ("slurp", SLURP)):
            p = os.path.join(self.t, name)
            with open(p, "w") as f:
                f.write(text)
            os.chmod(p, 0o755)
        self.env = dict(os.environ, PATH=self.t + ":/usr/bin:/bin",
                        GRIM_LOG=os.path.join(self.t, "grim.log"),
                        SLURP_IN=os.path.join(self.t, "slurp.in"))
        self.old_path = os.environ["PATH"]
        os.environ["PATH"] = self.env["PATH"]
        os.environ.pop("FAKE_POINTER", None)

    def tearDown(self):
        os.environ["PATH"] = self.old_path
        os.environ.pop("FAKE_POINTER", None)
        shutil.rmtree(self.t)


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
               "CaptureWorkspace": "(a{sv}h)", "CaptureActiveWindow": "(a{sv}h)",
               "CaptureArea": "(iiuua{sv}h)", "CaptureInteractive": "(ua{sv}h)"}[method]
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

    extra_env = {}

    def grim_args(self):
        with open(self.env["GRIM_LOG"]) as f:
            return f.read().splitlines()

    def setUp(self):
        super().setUp()
        self.dbus = subprocess.Popen(["dbus-daemon", "--session", "--nofork", "--print-address=1"],
                                     stdout=subprocess.PIPE, text=True)
        self.address = self.dbus.stdout.readline().strip()
        env = dict(self.env, DBUS_SESSION_BUS_ADDRESS=self.address, XDG_CURRENT_DESKTOP="wlmaker",
                   WAYLAND_DISPLAY="wayland-teste", **self.extra_env)
        self.service = subprocess.Popen([SERVICE], env=env)

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


    def test_active_window_box(self):
        meta, data = self.call("CaptureActiveWindow", ({},))
        self.assertNotIn("screen", meta)
        self.assertEqual(self.grim_args(), ["-t ppm -g 7000,100 800x600 -"])

    def test_area_negative_and_cursor(self):
        meta, _ = self.call("CaptureArea", (5231, -2, 1519, 985,
                                            {"include-cursor": GLib_bool(True)}))
        self.assertEqual(self.grim_args(), ["-t ppm -c -g 5231,-2 1519x985 -"])

    def test_interactive_window_choice(self):
        # a ativa primeiro, a de y negativo vale, a minimizada fica de fora
        meta, _ = self.call("CaptureInteractive", (0, {}))
        with open(self.env["SLURP_IN"]) as f:
            self.assertEqual(f.read().splitlines(),
                             ["7000,100 800x600", "5231,-2 1519x985"])
        self.assertEqual(self.grim_args(), ["-t ppm -g 7000,100 800x600 -"])

    def test_effects(self):
        import gi
        gi.require_version("Gio", "2.0")
        from gi.repository import Gio, GLib
        bus = Gio.DBusConnection.new_for_address_sync(
            self.address, Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT |
            Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION, None, None)
        for _ in range(50):
            try:
                r = bus.call_sync("org.kde.KWin", "/Effects", "org.kde.kwin.Effects",
                                  "isEffectLoaded", GLib.Variant("(s)", ("screenshot",)),
                                  None, Gio.DBusCallFlags.NONE, 5000, None)
                break
            except GLib.Error as e:
                if "ServiceUnknown" not in str(e) and "UnknownObject" not in str(e):
                    raise
                time.sleep(0.1)
        self.assertEqual(r.unpack(), (True,))
        v = bus.call_sync("org.kde.KWin.ScreenShot2", "/org/kde/KWin/ScreenShot2",
                          "org.freedesktop.DBus.Properties", "Get",
                          GLib.Variant("(ss)", ("org.kde.KWin.ScreenShot2", "Version")),
                          None, Gio.DBusCallFlags.NONE, 5000, None)
        self.assertEqual(v.unpack(), (4,))


class PointerBus(Bus):
    """Com o comando "pointer" do wlmaker: a saída sob o ponteiro."""
    extra_env = {"FAKE_POINTER": "4000 300 DP-1 3440x1440+3440+0"}

    def test_active_screen_is_one_screen(self):
        meta, data = self.call("CaptureActiveScreen", ({},))
        self.assertEqual(meta["screen"], "DP-1")
        self.assertEqual((meta["width"], meta["height"]), (30, 10))


class CancelBus(Bus):
    extra_env = {"FAKE_SLURP_CANCEL": "1"}

    def test_interactive_window_choice(self):
        import gi
        gi.require_version("Gio", "2.0")
        from gi.repository import GLib
        with self.assertRaises(GLib.Error) as cm:
            self.call("CaptureInteractive", (0, {}))
        self.assertIn("org.kde.KWin.ScreenShot2.Error.Cancelled", str(cm.exception))


def GLib_bool(v):
    import gi
    gi.require_version("Gio", "2.0")
    from gi.repository import GLib
    return GLib.Variant("b", v)


if __name__ == "__main__":
    unittest.main()
