#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Testes sem tela dos wrappers de aplicativos de terceiros (chamado pelo
# tests/run.sh, na raiz do repositório):
#   big-wm-scrcpy    leitura do "adb devices -l" (adb falso)
#   big-wm-airplay   ufw bloqueando ou não o AirPlay, conexões TCP do uxplay
#   big-wm-mic-tray  JSON do biglinux-microphone-cli, wpctl e o ícone
#   big-wm-hp        PyQt5 ausente pelo python do HPLIP (python falso)
"""Testes dos wrappers big-wm-scrcpy, big-wm-airplay, big-wm-mic-tray e big-wm-hp."""

import os
import socket
import stat
import subprocess
import sys
import tempfile
from importlib.machinery import SourceFileLoader
from importlib.util import module_from_spec, spec_from_loader

FAILURES = []
# Os módulos carregados de session/bin não deixam __pycache__ no repositório.
sys.dont_write_bytecode = True


def load(path, env=None):
    os.environ.update(env or {})
    loader = SourceFileLoader(os.path.basename(path).replace("-", "_"), path)
    mod = module_from_spec(spec_from_loader(loader.name, loader))
    loader.exec_module(mod)
    return mod


def check(cond, msg):
    if not cond:
        FAILURES.append(msg)


def script(path, text):
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    os.chmod(path, stat.S_IRWXU)
    return path


def test_scrcpy(tmp):
    m = load("session/bin/big-wm-scrcpy")
    adb = script(os.path.join(tmp, "adb"), """#!/bin/sh
cat <<'E'
* daemon not running; starting now at tcp:5037
* daemon started successfully
List of devices attached
R58M123ABC             device usb:1-2 product:a52q model:SM_A525M device:a52q transport_id:3
0123456789ABCDEF       unauthorized usb:1-3 transport_id:4
AAAA                   no permissions (missing udev rules? user is in the plugdev group); see [http://developer.android.com/tools/device.html] usb:1-4 transport_id:5
192.168.0.7:5555       offline transport_id:6

E
""")
    devices = m.list_devices(adb)
    check(devices == [("R58M123ABC", "device", "SM A525M"),
                      ("0123456789ABCDEF", "unauthorized", "0123456789ABCDEF"),
                      ("AAAA", "no permissions", "AAAA"),
                      ("192.168.0.7:5555", "offline", "192.168.0.7:5555")],
          "big-wm-scrcpy: adb devices -l lido errado: %r" % (devices,))
    empty = script(os.path.join(tmp, "adb-empty"), "#!/bin/sh\necho 'List of devices attached'\necho\n")
    check(m.list_devices(empty) == [], "big-wm-scrcpy: sem aparelho deve dar lista vazia")
    check(m.list_devices(os.path.join(tmp, "nao-existe")) is None, "big-wm-scrcpy: adb ausente deve dar None")


def test_airplay(tmp):
    ufw = os.path.join(tmp, "ufw")
    os.makedirs(ufw)
    m = load("session/bin/big-wm-airplay", {"BIG_WM_UFW_DIR": ufw})

    def rules(enabled, lines):
        with open(os.path.join(ufw, "ufw.conf"), "w", encoding="utf-8") as f:
            f.write("ENABLED=%s\nLOGLEVEL=low\n" % ("yes" if enabled else "no"))
        with open(os.path.join(ufw, "user.rules"), "w", encoding="utf-8") as f:
            f.write("*filter\n" + "\n".join(lines) + "\nCOMMIT\n")

    base = ["-A ufw-user-input -p udp -m multiport --dports 137,138 -j ACCEPT -m comment --comment 'dapp_CIFS'",
            "-A ufw-user-input -p tcp -m multiport --dports 139,445 -j ACCEPT -m comment --comment 'dapp_CIFS'",
            "-A ufw-user-input -p udp --dport 5353 -j ACCEPT",
            "-A ufw-user-input -i virbr0 -j ACCEPT"]
    rules(True, base)
    check(m.firewall_blocks() is True, "big-wm-airplay: ufw da ISO (só mDNS) bloqueia o AirPlay")
    rules(False, base)
    check(m.firewall_blocks() is False, "big-wm-airplay: ufw desligado não bloqueia")
    rules(True, base + ["-A ufw-user-input -p tcp -m multiport --dports 7000,7001,7100 -j ACCEPT "
                        "-m comment --comment 'dapp_UxPlay'"])
    check(m.firewall_blocks() is False, "big-wm-airplay: perfil UxPlay liberado")
    rules(True, base + ["-A ufw-user-input -p tcp --dport 7000:7001 -j ACCEPT"])
    check(m.firewall_blocks() is False, "big-wm-airplay: faixa 7000:7001 liberada")
    os.unlink(os.path.join(ufw, "user.rules"))
    check(m.firewall_blocks() is False, "big-wm-airplay: regras ilegíveis não viram aviso inventado")

    srv = socket.socket()
    srv.bind(("127.0.0.1", 0))
    srv.listen(1)
    check(m.established_peers(os.getpid()) == set(), "big-wm-airplay: só LISTEN não é conexão")
    cli = socket.create_connection(srv.getsockname())
    conn, _ = srv.accept()
    check(len(m.established_peers(os.getpid())) >= 1, "big-wm-airplay: conexão TCP estabelecida não vista")
    for s in (cli, conn, srv):
        s.close()


def test_mic_tray(tmp):
    m = load("session/bin/big-wm-mic-tray", {"XDG_CONFIG_HOME": tmp})
    ok = m.parse_status('{"audio_available":true,"mic_enabled":true,"mic_running":true,'
                        '"output_enabled":false,"output_running":false,"settings":{}}')
    check(ok == {"mic_enabled": True, "output_enabled": False, "mic_running": True,
                 "output_running": False, "audio_available": True}, "big-wm-mic-tray: status lido errado")
    check(m.parse_status('{"mic_enabled":"sim"}') is None, "big-wm-mic-tray: esquema inválido aceito")
    check(m.parse_status("") is None, "big-wm-mic-tray: saída vazia aceita")
    check(m.parse_muted("Volume: 0.80 [MUTED]") is True and m.parse_muted("Volume: 1.00") is False
          and m.parse_muted("") is None, "big-wm-mic-tray: wpctl get-volume lido errado")
    check(m.describe(ok, False)[:2] == ("Active", m.ICON_LIVE), "big-wm-mic-tray: microfone ativo")
    check(m.describe(ok, True)[:2] == ("Active", m.ICON_MUTED), "big-wm-mic-tray: microfone silenciado")
    check(m.describe(ok, None)[:2] == ("Active", m.ICON_LIVE) and "silenciado" not in m.describe(ok, None)[3]
          and "ativo" not in m.describe(ok, None)[3].split("\n")[0],
          "big-wm-mic-tray: sem wpctl não inventa o estado de silêncio")
    broken = dict(ok, mic_running=False)
    check(m.describe(broken, False)[:2] == ("NeedsAttention", m.ICON_ATTENTION),
          "big-wm-mic-tray: filtro ligado e parado pede atenção")
    check(m.describe(None, None)[0] == "NeedsAttention", "big-wm-mic-tray: status ilegível pede atenção")
    check(m.autostart_enabled(), "big-wm-mic-tray: autostart ligado por padrão")
    m.set_autostart(False)
    check(not m.autostart_enabled(), "big-wm-mic-tray: Hidden=true desliga o autostart")
    m.set_autostart(True)
    check(m.autostart_enabled() and not os.path.exists(m.AUTOSTART), "big-wm-mic-tray: religar apaga o Hidden")


def test_hp(tmp):
    bindir = os.path.join(tmp, "hpbin")
    os.makedirs(bindir)
    script(os.path.join(bindir, "python"), "#!/bin/sh\nexit 1\n")  # sem PyQt5
    script(os.path.join(bindir, "hp-toolbox"), "#!/bin/sh\necho toolbox-aberto\n")
    env = dict(os.environ, PATH=bindir + ":/usr/bin")
    m = load("session/bin/big-wm-hp")
    saved = os.environ["PATH"]
    os.environ["PATH"] = env["PATH"]
    try:
        check(m.pyqt5_present() is False, "big-wm-hp: PyQt5 ausente não detectado")
        script(os.path.join(bindir, "python"), "#!/bin/sh\nexit 0\n")
        check(m.pyqt5_present() is True, "big-wm-hp: PyQt5 presente não detectado")
    finally:
        os.environ["PATH"] = saved
    out = subprocess.run([sys.executable, "-I", "session/bin/big-wm-hp", "toolbox"], env=env,
                         capture_output=True, text=True, timeout=30, check=False)
    check(out.stdout.strip() == "toolbox-aberto", "big-wm-hp: com PyQt5 executa o hp-toolbox (%r)" % out.stdout)


def main():
    with tempfile.TemporaryDirectory() as tmp:
        for name, test in (("scrcpy", test_scrcpy), ("airplay", test_airplay),
                           ("mic", test_mic_tray), ("hp", test_hp)):
            sub = os.path.join(tmp, name)
            os.makedirs(sub)
            test(sub)
    for msg in FAILURES:
        print(msg, file=sys.stderr)
    return 1 if FAILURES else 0


if __name__ == "__main__":
    sys.exit(main())
