#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# big-wm-mic-tray (filtro de ruído do microfone na bandeja) como caixa-preta:
# o programa de verdade com biglinux-microphone-cli, wpctl, pactl e
# biglinux-microphone falsos (só comandos embutidos do sh, PATH só com eles)
# e, para o item, um dbus-daemon próprio:
#   - --state: JSON do CLI (esquema inválido, código de saída), [MUTED] do
#     wpctl, sem wpctl não inventa o silêncio, filtro parado pede atenção,
#     Hidden=true do autostart;
#   - StatusNotifierItem: propriedades, registro no StatusNotifierWatcher
#     (também quando a bandeja aparece depois), clique esquerdo abre as
#     configurações, o do meio silencia;
#   - menu (com.canonical.dbusmenu): filtros, silêncio, Iniciar com a sessão,
#     Fechar;
#   - "pactl subscribe" (LC_ALL=C): evento de fonte relê o estado e avisa
#     (NewIcon/NewStatus), volume de saída não; o pactl morre junto;
#   - uma instância por sessão; sem o CLI sai sem erro.
#
#   python3 -I tests/mic-tray/test_mic_tray.py   (da raiz)
#   BIG_WM_MIC_TRAY=session/bin/big-wm-mic-tray ...  (outra implementação)

import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# O programa em C (make mic-tray); outro caminho pelo ambiente.
TRAY = os.path.abspath(os.environ.get("BIG_WM_MIC_TRAY") or os.path.join(
    ROOT, "session", "mic-tray", "big-wm-mic-tray"))

ICON_LIVE = "audio-input-microphone-symbolic"
ICON_MUTED = "microphone-sensitivity-muted-symbolic"
ICON_ATTENTION = "dialog-warning-symbolic"

# Só comandos embutidos: o PATH dos testes tem só estes falsos.
CLI = r"""#!/bin/sh
printf '%s\n' "$*" >>"$T/cli.log"
case "$1" in
status)
  rc=0; [ -f "$T/status.rc" ] && read -r rc <"$T/status.rc"
  while IFS= read -r l || [ -n "$l" ]; do printf '%s\n' "$l"; done <"$T/status.json"
  exit "$rc" ;;
esac
exit 0
"""

WPCTL = r"""#!/bin/sh
printf '%s\n' "$*" >>"$T/wpctl.log"
case "$1" in
get-volume) while IFS= read -r l || [ -n "$l" ]; do printf '%s\n' "$l"; done <"$T/volume" ;;
esac
exit 0
"""

# Linhas vindas de um FIFO que o teste mantém aberto.
PACTL = r"""#!/bin/sh
printf '%s %s %s\n' "$$" "${LC_ALL:-unset}" "$*" >"$T/pactl.pid"
while IFS= read -r l; do printf '%s\n' "$l"; done <"$T/pactl.fifo"
"""

APP = r"""#!/bin/sh
printf 'aberto\n' >>"$T/app.log"
"""

OK = {"audio_available": True, "mic_enabled": True, "mic_running": True,
      "output_enabled": False, "output_running": False, "settings": {"a": [1.5, None, "x"]}}


class Base(unittest.TestCase):
    tools = ("biglinux-microphone-cli", "wpctl", "pactl", "biglinux-microphone")

    def setUp(self):
        self.t = tempfile.mkdtemp()
        self.bin = os.path.join(self.t, "bin")
        os.mkdir(self.bin)
        texts = {"biglinux-microphone-cli": CLI, "wpctl": WPCTL, "pactl": PACTL,
                 "biglinux-microphone": APP}
        for name in self.tools:
            p = os.path.join(self.bin, name)
            with open(p, "w") as f:
                f.write(texts[name])
            os.chmod(p, 0o755)
        self.config = os.path.join(self.t, "config")
        os.makedirs(os.path.join(self.config, "biglinux-microphone"))
        self.autostart = os.path.join(self.config, "autostart", "big-wm-mic-tray.desktop")
        os.mkfifo(os.path.join(self.t, "pactl.fifo"))
        self.set_status(OK)
        self.set_volume("Volume: 0.80")
        self.env = {"PATH": self.bin, "T": self.t, "XDG_CONFIG_HOME": self.config,
                    "HOME": self.t, "LANG": "C", "LC_ALL": "C"}

    def tearDown(self):
        shutil.rmtree(self.t)

    def path(self, name):
        return os.path.join(self.t, name)

    def set_status(self, data, rc=0):
        with open(self.path("status.json"), "w") as f:
            f.write(data if isinstance(data, str) else json.dumps(data))
        with open(self.path("status.rc"), "w") as f:
            f.write("%d\n" % rc)

    def set_volume(self, text):
        with open(self.path("volume"), "w") as f:
            f.write(text + "\n")

    def log(self, name):
        try:
            with open(self.path(name)) as f:
                return f.read().splitlines()
        except FileNotFoundError:
            return []


class State(Base):
    """--state: o estado lido agora, sem bandeja."""

    def state(self):
        out = subprocess.run([TRAY, "--state"], env=self.env, capture_output=True, text=True,
                             timeout=20, check=False)
        self.assertEqual(out.returncode, 0, out.stderr)
        head, _, body = out.stdout.partition("\n")
        return head, body.rstrip("\n").split("\n")

    def test_active(self):
        head, body = self.state()
        self.assertEqual(head, "status=Active icon=%s autostart=True" % ICON_LIVE)
        self.assertEqual(body, ["Microphone active", "Microphone filter: on", "System sound filter: off"])

    def test_muted(self):
        self.set_volume("Volume: 0.80 [MUTED]")
        head, body = self.state()
        self.assertEqual(head, "status=Active icon=%s autostart=True" % ICON_MUTED)
        self.assertEqual(body[0], "Microphone muted")

    def test_no_wpctl_invents_nothing(self):
        os.unlink(os.path.join(self.bin, "wpctl"))
        head, body = self.state()
        self.assertEqual(head, "status=Active icon=%s autostart=True" % ICON_LIVE)
        self.assertEqual(body, ["Microphone filter: on", "System sound filter: off"])

    def test_unreadable_volume(self):
        self.set_volume("")
        head, body = self.state()
        self.assertEqual(head, "status=Active icon=%s autostart=True" % ICON_LIVE)
        self.assertEqual(body[0], "Microphone filter: on")

    def test_filter_not_running(self):
        self.set_status(dict(OK, mic_running=False))
        head, body = self.state()
        self.assertEqual(head, "status=NeedsAttention icon=%s autostart=True" % ICON_ATTENTION)
        self.assertEqual(body[-1], "Some filters are not ready: open the settings.")

    def test_output_not_running_and_no_audio(self):
        self.set_status(dict(OK, output_enabled=True))
        self.assertTrue(self.state()[0].startswith("status=NeedsAttention "))
        self.set_status(dict(OK, audio_available=False))
        self.assertTrue(self.state()[0].startswith("status=NeedsAttention "))
        # running ausente ou não booleano: parado
        data = dict(OK)
        data["mic_running"] = "true"
        self.set_status(data)
        self.assertTrue(self.state()[0].startswith("status=NeedsAttention "))

    def test_invalid_status(self):
        for text, rc in (('{"mic_enabled":"sim","output_enabled":false}', 0), ("", 0),
                         ("[true]", 0), ("{nada", 0), ('{"output_enabled":false}', 0),
                         (json.dumps(OK) + " lixo", 0), (json.dumps(OK), 3)):
            self.set_status(text, rc)
            head, body = self.state()
            self.assertEqual(head, "status=NeedsAttention icon=%s autostart=True" % ICON_ATTENTION, text)
            self.assertEqual(body, ["Could not read the audio status. Open the settings."], text)

    def test_autostart_hidden(self):
        os.makedirs(os.path.dirname(self.autostart))
        with open(self.autostart, "w") as f:
            f.write("[Desktop Entry]\nName=x\n Hidden = TRUE \n")
        self.assertTrue(self.state()[0].endswith(" autostart=False"))
        with open(self.autostart, "w") as f:
            f.write("[Desktop Entry]\nHidden=false\n")
        self.assertTrue(self.state()[0].endswith(" autostart=True"))

    def test_translated(self):
        if not shutil.which("msgfmt"):
            self.skipTest("sem msgfmt")
        if "pt_BR.utf8" not in subprocess.run(["locale", "-a"], capture_output=True,
                                              text=True).stdout.split():
            self.skipTest("sem o locale pt_BR.UTF-8")
        mo = os.path.join(self.t, "locale", "pt_BR", "LC_MESSAGES")
        os.makedirs(mo)
        subprocess.run(["msgfmt", "-o", os.path.join(mo, "big-wm.mo"),
                        os.path.join(ROOT, "po", "pt_BR.po")], check=True)
        self.env.update(LANG="pt_BR.UTF-8", LC_ALL="", BIGWM_LOCALEDIR=os.path.join(self.t, "locale"))
        del self.env["LC_ALL"]
        head, body = self.state()
        self.assertEqual(head, "status=Active icon=%s autostart=True" % ICON_LIVE)
        self.assertEqual(body[0], "Microfone ativo")

    def test_without_cli_exits_quietly(self):
        os.unlink(os.path.join(self.bin, "biglinux-microphone-cli"))
        env = dict(self.env, DBUS_SESSION_BUS_ADDRESS="unix:path=" + self.path("nada"))
        out = subprocess.run([TRAY], env=env, capture_output=True, text=True, timeout=20, check=False)
        self.assertEqual(out.returncode, 0)
        self.assertIn("biglinux-microphone-cli", out.stderr)


class Tray(Base):
    """O item de verdade num dbus-daemon próprio."""

    def setUp(self):
        super().setUp()
        import gi
        gi.require_version("Gio", "2.0")
        from gi.repository import Gio, GLib
        self.Gio, self.GLib = Gio, GLib
        self.dbus = subprocess.Popen(["dbus-daemon", "--session", "--nofork", "--print-address=1"],
                                     stdout=subprocess.PIPE, text=True)
        self.address = self.dbus.stdout.readline().strip()
        self.env["DBUS_SESSION_BUS_ADDRESS"] = self.address
        self.bus = Gio.DBusConnection.new_for_address_sync(
            self.address, Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT |
            Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION, None, None)
        self.signals = []
        self.bus.signal_subscribe(None, "org.kde.StatusNotifierItem", None, "/StatusNotifierItem",
                                  None, Gio.DBusSignalFlags.NONE,
                                  lambda *a: self.signals.append(a[4]))
        self.registered = []
        self.watcher_id = 0
        # o FIFO do pactl falso fica aberto durante o teste
        self.fifo = os.open(self.path("pactl.fifo"), os.O_RDWR | os.O_NONBLOCK)
        self.procs = []

    def tearDown(self):
        for p in self.procs:
            if p.poll() is None:
                p.terminate()
                p.wait(5)
        os.close(self.fifo)
        self.bus.close_sync(None)
        self.dbus.terminate()
        self.dbus.wait(5)
        self.dbus.stdout.close()
        super().tearDown()

    # -- ajudantes --------------------------------------------------------

    def pump(self, cond, timeout=10):
        ctx = self.GLib.MainContext.default()
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            while ctx.iteration(False):
                pass
            if cond():
                return True
            time.sleep(0.02)
        return False

    def start(self):
        p = subprocess.Popen([TRAY], env=self.env)
        self.procs.append(p)
        self.assertTrue(self.pump(lambda: self.owner() is not None), "nome org.bigwindowmaker.MicTray")
        self.unique = self.owner()
        # leitura inicial feita (o item já tem o estado)
        self.assertTrue(self.pump(lambda: self.prop("Status") is not None and self.log("cli.log")))
        self.assertTrue(self.pump(lambda: os.path.exists(self.path("pactl.pid"))), "pactl subscribe")
        self.pump(lambda: False, 0.5)
        return p

    def owner(self):
        try:
            r = self.bus.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                                   "GetNameOwner", self.GLib.Variant("(s)", ("org.bigwindowmaker.MicTray",)),
                                   None, self.Gio.DBusCallFlags.NONE, 2000, None)
            return r.unpack()[0]
        except self.GLib.Error:
            return None

    def call(self, iface, path, method, sig=None, args=None, dest=None):
        params = self.GLib.Variant(sig, args) if sig else None
        r = self.bus.call_sync(dest or self.unique, path, iface, method, params, None,
                               self.Gio.DBusCallFlags.NONE, 5000, None)
        return r.unpack() if r is not None else None

    def prop(self, name):
        try:
            return self.call("org.freedesktop.DBus.Properties", "/StatusNotifierItem", "Get", "(ss)",
                             ("org.kde.StatusNotifierItem", name), dest=self.owner())[0]
        except self.GLib.Error:
            return None

    def layout(self):
        _rev, (_id, _props, children) = self.call("com.canonical.dbusmenu", "/MenuBar", "GetLayout",
                                                  "(iias)", (0, -1, []))
        items = []
        for c in children:
            cid, props, _sub = c
            items.append(dict(props, id=cid))
        return items

    def item(self, label):
        for it in self.layout():
            if it.get("label") == label:
                return it
        self.fail("item %r não está no menu" % label)

    def click(self, label):
        it = self.item(label)
        self.call("com.canonical.dbusmenu", "/MenuBar", "Event", "(isvu)",
                  (it["id"], "clicked", self.GLib.Variant("s", ""), 0))

    def pactl_event(self, line):
        os.write(self.fifo, (line + "\n").encode())

    def own_watcher(self):
        Gio, GLib = self.Gio, self.GLib
        node = Gio.DBusNodeInfo.new_for_xml(
            "<node><interface name='org.kde.StatusNotifierWatcher'>"
            "<method name='RegisterStatusNotifierItem'><arg type='s' direction='in'/></method>"
            "</interface></node>")

        def on_call(_c, sender, _p, _i, _m, params, inv):
            self.registered.append((sender, params.unpack()[0]))
            inv.return_value(None)
        self.bus.register_object("/StatusNotifierWatcher", node.interfaces[0], on_call, None, None)
        self.bus.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                           "RequestName", GLib.Variant("(su)", ("org.kde.StatusNotifierWatcher", 4)),
                           None, Gio.DBusCallFlags.NONE, 2000, None)

    # -- testes -----------------------------------------------------------

    def test_properties(self):
        self.start()
        self.assertEqual(self.prop("Id"), "big-wm-mic-tray")
        self.assertEqual(self.prop("Category"), "Hardware")
        self.assertEqual(self.prop("Title"), "Microphone (noise filter)")
        self.assertEqual(self.prop("Status"), "Active")
        self.assertEqual(self.prop("IconName"), ICON_LIVE)
        self.assertEqual(self.prop("AttentionIconName"), ICON_ATTENTION)
        self.assertEqual(self.prop("ItemIsMenu"), False)
        self.assertEqual(self.prop("Menu"), "/MenuBar")
        self.assertEqual(self.prop("WindowId"), 0)
        self.assertEqual(self.prop("IconPixmap"), [])
        self.assertEqual(self.prop("ToolTip"),
                         (ICON_LIVE, [], "Microphone",
                          "Microphone active\nMicrophone filter: on\nSystem sound filter: off"))

    def test_registers_with_watcher_now_and_later(self):
        self.own_watcher()
        self.start()
        self.assertTrue(self.pump(lambda: self.registered), "registro na bandeja")
        self.assertEqual(self.registered[0], (self.unique, "/StatusNotifierItem"))
        # bandeja reiniciada: registra de novo
        self.bus.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                           "ReleaseName", self.GLib.Variant("(s)", ("org.kde.StatusNotifierWatcher",)),
                           None, self.Gio.DBusCallFlags.NONE, 2000, None)
        self.pump(lambda: False, 0.3)
        self.bus.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                           "RequestName", self.GLib.Variant("(su)", ("org.kde.StatusNotifierWatcher", 4)),
                           None, self.Gio.DBusCallFlags.NONE, 2000, None)
        self.assertTrue(self.pump(lambda: len(self.registered) >= 2), "novo registro")

    def test_activate_and_middle_click(self):
        self.start()
        self.call("org.kde.StatusNotifierItem", "/StatusNotifierItem", "Activate", "(ii)", (10, 20))
        self.assertTrue(self.pump(lambda: self.log("app.log") == ["aberto"]), "configurações abertas")
        self.set_volume("Volume: 0.80 [MUTED]")
        self.call("org.kde.StatusNotifierItem", "/StatusNotifierItem", "SecondaryActivate", "(ii)", (10, 20))
        self.assertTrue(self.pump(lambda: "set-mute @DEFAULT_AUDIO_SOURCE@ toggle" in self.log("wpctl.log")))
        self.assertTrue(self.pump(lambda: self.prop("IconName") == ICON_MUTED), "ícone silenciado")
        self.assertIn("NewIcon", self.signals)
        self.assertIn("NewToolTip", self.signals)
        # Scroll e ContextMenu não fazem nada, mas respondem
        self.call("org.kde.StatusNotifierItem", "/StatusNotifierItem", "Scroll", "(is)", (1, "vertical"))
        self.call("org.kde.StatusNotifierItem", "/StatusNotifierItem", "ContextMenu", "(ii)", (1, 1))

    def test_menu(self):
        self.start()
        labels = [it.get("label", "") for it in self.layout() if it.get("visible", True)
                  and it.get("type") != "separator"]
        self.assertEqual(labels, ["Microphone filter", "System sound filter", "Mute microphone",
                                  "Open settings...", "Start with the session", "Close"])
        self.assertEqual(self.item("Microphone filter").get("toggle-state"), 1)
        self.assertEqual(self.item("System sound filter").get("toggle-state"), 0)
        self.assertEqual(self.item("Mute microphone").get("toggle-state"), 0)
        self.assertEqual(self.item("Start with the session").get("toggle-state"), 1)
        self.assertEqual(self.item("Microphone filter").get("toggle-type"), "checkmark")
        self.assertNotEqual(self.item("Microphone filter").get("enabled"), False)
        # filtro do microfone: "set mic off" e uma nova leitura
        n = self.log("cli.log").count("status")
        self.click("Microphone filter")
        self.assertTrue(self.pump(lambda: "set mic off" in self.log("cli.log")), self.log("cli.log"))
        self.assertTrue(self.pump(lambda: self.log("cli.log").count("status") > n))
        self.click("System sound filter")
        self.assertTrue(self.pump(lambda: "set output on" in self.log("cli.log")), self.log("cli.log"))
        # silenciar pelo menu
        self.click("Mute microphone")
        self.assertTrue(self.pump(lambda: "set-mute @DEFAULT_AUDIO_SOURCE@ toggle" in self.log("wpctl.log")))
        # Iniciar com a sessão: Hidden=true, e de volta apaga o arquivo
        self.click("Start with the session")
        self.assertTrue(self.pump(lambda: os.path.exists(self.autostart)))
        with open(self.autostart) as f:
            text = f.read()
        self.assertIn("Hidden=true", text.splitlines())
        self.assertIn("Exec=big-wm-mic-tray", text.splitlines())
        self.assertTrue(self.pump(lambda: self.item("Start with the session").get("toggle-state") == 0))
        self.click("Start with the session")
        self.assertTrue(self.pump(lambda: not os.path.exists(self.autostart)))
        # Abrir configurações...
        self.click("Open settings...")
        self.assertTrue(self.pump(lambda: self.log("app.log") == ["aberto"]))

    def test_close_from_menu(self):
        p = self.start()
        self.click("Close")
        self.assertEqual(p.wait(10), 0)

    def test_attention_notice_in_menu(self):
        self.set_status(dict(OK, mic_running=False))
        self.start()
        self.assertEqual(self.prop("Status"), "NeedsAttention")
        self.assertEqual(self.prop("IconName"), ICON_ATTENTION)
        notice = self.layout()[0]
        self.assertEqual(notice.get("label"), "Some filters are not ready: open the settings.")
        self.assertEqual(notice.get("enabled"), False)
        self.assertNotEqual(notice.get("visible"), False)
        # sem áudio: os filtros ficam desabilitados
        self.set_status(dict(OK, audio_available=False))
        self.pactl_event("Event 'change' on source #56")
        self.assertTrue(self.pump(lambda: self.item("Microphone filter").get("enabled") is False))

    def test_pactl_events(self):
        self.start()
        with open(self.path("pactl.pid")) as f:
            pid, lc_all, args = f.read().split(" ", 2)
        self.assertEqual((lc_all, args.strip()), ("C", "subscribe"))
        n = len(self.log("cli.log"))
        self.pactl_event("Event 'change' on sink #54")
        self.pactl_event("Event 'change' on sink-input #30476")
        self.pactl_event("Event 'remove' on client #30475")
        self.pump(lambda: False, 1.2)
        self.assertEqual(len(self.log("cli.log")), n, "volume de saída ou cliente acorda o item")
        self.set_status(dict(OK, mic_running=False))
        self.signals.clear()
        self.pactl_event("Event 'remove' on source #29443")
        self.assertTrue(self.pump(lambda: self.prop("Status") == "NeedsAttention"), "evento de fonte relê")
        self.assertTrue(self.pump(lambda: "NewStatus" in self.signals and "NewIcon" in self.signals))
        for line in ("Event 'change' on server #-1", "Event 'new' on sink #812",
                     "Event 'remove' on sink #812"):
            n = self.log("cli.log").count("status")
            self.pactl_event(line)
            self.assertTrue(self.pump(lambda: self.log("cli.log").count("status") > n, 3), line)

    def test_config_change_rereads(self):
        self.start()
        n = self.log("cli.log").count("status")
        with open(os.path.join(self.config, "biglinux-microphone", "outro.json"), "w") as f:
            f.write("{}")
        self.pump(lambda: False, 1)
        self.assertEqual(self.log("cli.log").count("status"), n, "outro arquivo relê")
        with open(os.path.join(self.config, "biglinux-microphone", "settings.json"), "w") as f:
            f.write("{}")
        self.assertTrue(self.pump(lambda: self.log("cli.log").count("status") > n, 3), "settings.json relê")

    def test_single_instance_and_sigterm(self):
        p = self.start()
        with open(self.path("pactl.pid")) as f:
            pactl = int(f.read().split()[0])
        second = subprocess.run([TRAY], env=self.env, timeout=20, check=False)
        self.assertEqual(second.returncode, 0)
        self.assertEqual(self.owner(), self.unique)
        p.send_signal(signal.SIGTERM)
        self.assertEqual(p.wait(10), 0)

        def gone():
            try:
                with open("/proc/%d/stat" % pactl) as f:
                    return f.read().split(") ", 1)[1].startswith("Z")
            except FileNotFoundError:
                return True
        if not self.pump(gone, 5):
            # Falhou uma vez no CI (2026-11-26) e não se reproduz aqui (0 de 40
            # com a CPU carregada): o estado do processo diz o porquê.
            try:
                with open("/proc/%d/stat" % pactl) as f:
                    stat = f.read().split(") ", 1)[1].split()
                with open("/proc/%d/cmdline" % pactl, "rb") as f:
                    cmd = f.read().replace(b"\0", b" ").decode(errors="replace")
                info = "estado %s, pai %s, %s" % (stat[0], stat[1], cmd)
            except OSError as e:
                info = str(e)
            self.fail("pactl subscribe ficou rodando (%s)" % info)

    def test_without_wpctl(self):
        os.unlink(os.path.join(self.bin, "wpctl"))
        self.start()
        self.assertEqual(self.item("Mute microphone").get("visible"), False)
        self.call("org.kde.StatusNotifierItem", "/StatusNotifierItem", "SecondaryActivate", "(ii)", (1, 1))
        self.assertEqual(self.prop("IconName"), ICON_LIVE)


if __name__ == "__main__":
    unittest.main(verbosity=1)
