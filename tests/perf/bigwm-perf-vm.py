#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-vm.py - driver do HOST para o benchmark de memória do Big
# WindowMaker: VM QEMU/KVM do sistema INSTALADO, sempre a partir do mesmo
# estado (overlay qcow2 descartável sobre a imagem preparada), login de
# verdade pelo greeter do wdm com o teclado (QMP send-key) e coleta pelo
# qemu-guest-agent com tests/perf/bigwm-perf-collect.py.
#
#   bigwm-perf-vm.py boot --image BASE.qcow2 --session x11|wayland --out DIR
#                    --user openqa --password-file F [--label L] [--samples ...]
#                    [--setup 'comando root antes do login']...
#       Um boot medido: cria DIR/overlay.qcow2 sobre BASE, inicia a VM, grava a
#       sessão lembrada do wdm (/var/lib/wdm/sessions/<usuário>: x11/big-wm ou
#       wayland/big-wm-wayland), entra pelo greeter, espera o coletor terminar
#       (amostras a 30 s, 90 s, 5 min e 15 min depois do login), copia os
#       resultados para DIR/guest/ e desliga. O overlay é apagado no fim
#       (--keep-overlay para mantê-lo).
#   bigwm-perf-vm.py cycles ... --cycles N
#       Como boot, mas faz N logins/logouts pelo wdm (logout gracioso) e guarda
#       a lista de processos 90 s depois de cada login e 20 s depois de cada
#       logout, para achar órfãos e duplicados. Logout: ver logout().
#   bigwm-perf-vm.py prepare --base 00.qcow2 --image 01.qcow2 --out DIR
#                    --password-file F [--packages *.pkg.tar.zst]
#       Prepara a imagem medida (overlay 01 sobre a instalação 00): instala o
#       qemu-guest-agent pelo console hvc0 com a conta instalada, aplica os
#       pacotes do main (pacman -U), guarda o pacman -Q e faz um login de
#       aquecimento em cada sessão (primeiro login depois da atualização).
#   bigwm-perf-vm.py live --iso ISO --out DIR [--wizard-keys ret,ret,ret]
#       Sessão live da ISO na mesma VM (sem disco): login no hvc0 com o usuário
#       live, coletor via systemd-run (unidade bigwm-perf-collect, fora da
#       sessão) com t0 = início do wmaker, assistente concluído pelo teclado,
#       resultados enviados ao host por HTTP em 127.0.0.1 (10.0.2.2 no slirp).
#   bigwm-perf-vm.py shell --image IMG --out DIR [--writable]
#       Sobe a VM com o agente e executa como root cada linha da entrada
#       padrão (@push ORIGEM DESTINO e @pull ORIGEM DESTINO copiam arquivos).
#
# Configuração fixa da VM (igual em todos os boots): KVM, -cpu host, 2 vCPUs,
# 4096 MiB, machine pc (a mesma do teste de instalação BIOS do build-iso),
# virtio-blk, virtio-vga 1920x1080 sem 3D, intel-hda sem saída de áudio
# (como o openQA), virtio-net user, virtio-rng, qemu-xhci + usb-tablet. Sem
# console serial nos boots medidos (nenhum getty a mais no convidado).
import argparse
import base64
import json
import os
import shutil
import socket
import subprocess
import sys
import time
import zlib
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
COLLECTOR = os.path.join(HERE, "bigwm-perf-collect.py")
GUEST_DIR = "/var/lib/bigwm-perf"
SESSIONS = {"x11": "x11/big-wm", "wayland": "wayland/big-wm-wayland"}

VM = {"ram_mib": 4096, "vcpus": 2, "machine": "pc", "gpu": "virtio-vga", "xres": 1920, "yres": 1080}


def log(msg):
    print(f"[perf-vm {time.strftime('%H:%M:%S')}] {msg}", flush=True)


class JsonSock:
    def __init__(self, path, timeout=300):
        deadline = time.time() + 30
        while True:
            try:
                self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.s.connect(path)
                break
            except OSError:
                if time.time() > deadline:
                    raise
                time.sleep(0.2)
        self.s.settimeout(timeout)
        self.buf = b""

    def send(self, obj):
        self.s.sendall(json.dumps(obj).encode() + b"\n")

    def recv(self):
        while b"\n" not in self.buf:
            chunk = self.s.recv(1 << 20)
            if not chunk:
                raise ConnectionError("socket fechado")
            self.buf += chunk
        line, self.buf = self.buf.split(b"\n", 1)
        line = line.strip(b"\xff").strip()
        return json.loads(line) if line else self.recv()


class QMP(JsonSock):
    def __init__(self, path):
        super().__init__(path)
        self.recv()  # greeting
        self.cmd("qmp_capabilities")

    def cmd(self, name, **args):
        self.send({"execute": name, "arguments": args} if args else {"execute": name})
        while True:
            r = self.recv()
            if "event" in r:
                continue
            if "error" in r:
                raise RuntimeError(f"QMP {name}: {r['error']}")
            return r.get("return")

    def key(self, *names, hold=80):
        self.cmd("send-key", keys=[{"type": "qcode", "data": n} for n in names], **{"hold-time": hold})


class QGA:
    """qemu-guest-agent pelo socket do chardev (reconecta quando preciso)."""

    def __init__(self, path):
        self.path = path
        self.js = None

    def _connect(self, timeout):
        self.js = JsonSock(self.path, timeout)

    def sync(self, timeout=5):
        if self.js is None:
            self._connect(timeout)
        self.js.s.settimeout(timeout)
        token = int(time.time() * 1000) % 2**31
        self.js.s.sendall(b"\xff")
        self.js.send({"execute": "guest-sync-delimited", "arguments": {"id": token}})
        while True:
            r = self.js.recv()
            if r.get("return") == token:
                return True

    def wait(self, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                if self.sync(5):
                    self.js.s.settimeout(300)
                    return True
            except (OSError, ConnectionError, ValueError):
                try:
                    if self.js:
                        self.js.s.close()
                except OSError:
                    pass
                self.js = None
                time.sleep(2)
        return False

    def cmd(self, name, **args):
        self.js.send({"execute": name, "arguments": args} if args else {"execute": name})
        r = self.js.recv()
        if "error" in r:
            raise RuntimeError(f"QGA {name}: {r['error']}")
        return r.get("return")

    def exec(self, argv, wait=True, timeout=600, stdin=None):
        args = {"path": argv[0], "arg": argv[1:], "capture-output": wait}
        if stdin is not None:
            args["input-data"] = base64.b64encode(stdin).decode()
        pid = self.cmd("guest-exec", **args)["pid"]
        if not wait:
            return pid
        deadline = time.time() + timeout
        while time.time() < deadline:
            st = self.cmd("guest-exec-status", pid=pid)
            if st.get("exited"):
                out = base64.b64decode(st.get("out-data", "")).decode("utf-8", "replace")
                err = base64.b64decode(st.get("err-data", "")).decode("utf-8", "replace")
                return st.get("exitcode", st.get("signal", -1)), out, err
            time.sleep(0.5)
        raise TimeoutError(f"guest-exec {argv} passou de {timeout} s")

    def sh(self, script, timeout=600):
        return self.exec(["/bin/bash", "-c", script], timeout=timeout)

    def write_file(self, path, data, mode="w"):
        h = self.cmd("guest-file-open", path=path, mode=mode)
        for i in range(0, len(data), 1 << 20):
            self.cmd("guest-file-write", handle=h, **{"buf-b64": base64.b64encode(data[i:i + (1 << 20)]).decode()})
        self.cmd("guest-file-close", handle=h)

    def read_file(self, path):
        h = self.cmd("guest-file-open", path=path, mode="r")
        out = b""
        while True:
            r = self.cmd("guest-file-read", handle=h, count=1 << 20)
            out += base64.b64decode(r.get("buf-b64", ""))
            if r.get("eof"):
                break
        self.cmd("guest-file-close", handle=h)
        return out


def ppm_to_png(ppm, png):
    with open(ppm, "rb") as f:
        data = f.read()
    parts = data.split(maxsplit=4)
    w, h = int(parts[1]), int(parts[2])
    pix = parts[4]
    raw = b"".join(b"\x00" + pix[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(png, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
    os.unlink(ppm)


class VMRun:
    def __init__(self, image, out, writable=False, console=False, keep_overlay=False, iso=False):
        self.out = os.path.abspath(out)
        os.makedirs(self.out, exist_ok=True)
        self.run = os.path.join(self.out, "run")
        os.makedirs(self.run, exist_ok=True)
        self.keep_overlay = keep_overlay
        image = os.path.abspath(image)
        if iso:
            self.disk = self.overlay = None
        elif writable:
            self.disk = image
            self.overlay = None
        else:
            self.overlay = self.disk = os.path.join(self.out, "overlay.qcow2")
            if os.path.exists(self.overlay):
                os.unlink(self.overlay)
            subprocess.run(["qemu-img", "create", "-q", "-f", "qcow2", "-F", "qcow2", "-b", image, self.overlay],
                           check=True)
        self.qmp_path = os.path.join(self.run, "qmp.sock")
        self.qga_path = os.path.join(self.run, "qga.sock")
        self.con_path = os.path.join(self.run, "console.sock")
        for p in (self.qmp_path, self.qga_path, self.con_path):
            if os.path.exists(p):
                os.unlink(p)
        argv = [
            "qemu-system-x86_64", "-name", "bigwm-perf", "-machine", f"{VM['machine']},accel=kvm",
            "-cpu", "host", "-smp", str(VM["vcpus"]), "-m", str(VM["ram_mib"]),
            "-device", f"{VM['gpu']},xres={VM['xres']},yres={VM['yres']}",
            "-display", "none", "-vga", "none",
            "-audiodev", "none,id=snd0", "-device", "intel-hda", "-device", "hda-output,audiodev=snd0",
            "-netdev", "user,id=net0", "-device", "virtio-net,netdev=net0,mac=52:54:00:12:34:56",
            "-object", "rng-random,filename=/dev/urandom,id=rng0", "-device", "virtio-rng-pci,rng=rng0",
            "-device", "qemu-xhci", "-device", "usb-tablet",
        ]
        if iso:
            argv += ["-drive", f"file={image},media=cdrom,readonly=on,if=ide,format=raw", "-boot", "d"]
        else:
            argv += ["-drive", f"file={self.disk},if=virtio,format=qcow2,cache=writeback,discard=unmap"]
        argv += [
            "-device", "virtio-serial",
            "-chardev", f"socket,path={self.qga_path},server=on,wait=off,id=qga0",
            "-device", "virtserialport,chardev=qga0,name=org.qemu.guest_agent.0",
            "-qmp", f"unix:{self.qmp_path},server=on,wait=off",
            "-serial", f"file:{os.path.join(self.out, 'serial.log')}",
            "-no-reboot",
        ]
        if console:
            argv += ["-chardev", f"socket,path={self.con_path},server=on,wait=off,id=con0",
                     "-device", "virtconsole,chardev=con0"]
        with open(os.path.join(self.out, "qemu-cmd.txt"), "w") as f:
            f.write(" ".join(argv) + "\n")
        self.t_start = time.time()
        self.proc = subprocess.Popen(argv, stdout=open(os.path.join(self.out, "qemu.log"), "w"),
                                     stderr=subprocess.STDOUT)
        self.qmp = QMP(self.qmp_path)
        self.qga = QGA(self.qga_path)

    def screenshot(self, name):
        ppm = os.path.join(self.out, name + ".ppm")
        try:
            self.qmp.cmd("screendump", filename=ppm)
            time.sleep(1)
            ppm_to_png(ppm, os.path.join(self.out, name + ".png"))
        except Exception as e:  # noqa: BLE001
            log(f"captura {name} falhou: {e}")

    def type_text(self, text):
        names = {"-": "minus", "_": "shift-minus", ".": "dot", " ": "spc", "/": "slash"}
        for ch in text:
            if ch.isdigit() or (ch.isalpha() and ch.islower()):
                self.qmp.key(ch)
            elif ch.isalpha():
                self.qmp.key("shift", ch.lower())
            elif ch in names:
                n = names[ch]
                self.qmp.key(*n.split("-")) if n.startswith("shift-") else self.qmp.key(n)
            else:
                raise ValueError(f"caractere sem tecla mapeada: {ch!r}")
            time.sleep(0.05)

    def poweroff(self, timeout=180):
        try:
            self.qga.cmd("guest-shutdown", mode="powerdown")
        except Exception:  # noqa: BLE001 - o agente some no desligamento
            try:
                self.qmp.cmd("system_powerdown")
            except Exception:  # noqa: BLE001
                pass
        try:
            self.proc.wait(timeout)
        except subprocess.TimeoutExpired:
            log("a VM não desligou a tempo; quit")
            try:
                self.qmp.cmd("quit")
            except Exception:  # noqa: BLE001
                self.proc.kill()
            self.proc.wait(30)
        if self.overlay and not self.keep_overlay:
            os.unlink(self.overlay)


def guest_greeter_up(qga):
    rc, out, _ = qga.sh("pgrep -x wdmLogin >/dev/null && echo up", 30)
    return out.strip() == "up"


def prepare_guest(vm, args, label):
    """Antes do login: coletor no convidado e sessão lembrada do wdm."""
    qga = vm.qga
    with open(COLLECTOR, "rb") as f:
        code = f.read()
    qga.sh(f"rm -rf {GUEST_DIR}/{label}; mkdir -p {GUEST_DIR}/{label}")
    qga.write_file(f"{GUEST_DIR}/bigwm-perf-collect.py", code)
    sess = SESSIONS[args.session]
    rc, out, err = qga.sh(f"mkdir -p /var/lib/wdm/sessions && printf '%s\\n' '{sess}' >/var/lib/wdm/sessions/{args.user}"
                          f" && cat /var/lib/wdm/sessions/{args.user}")
    log(f"sessão lembrada do wdm: {out.strip()} (rc {rc})")
    for cmd in args.setup or []:
        rc, out, err = qga.sh(cmd, 300)
        log(f"setup: {cmd!r} -> rc {rc} {out.strip()[:200]} {err.strip()[:200]}")
        if rc != 0:
            raise RuntimeError(f"setup falhou: {cmd}")


def wait_greeter(vm, timeout=300):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if guest_greeter_up(vm.qga):
            return True
        time.sleep(1)
    return False


def login(vm, password):
    # wdm: a tela abre na lista de usuários com o último já escolhido; Enter
    # leva à senha (mesma sequência do openQA, tests/installed_login.pm).
    time.sleep(3)
    vm.qmp.key("ret")
    time.sleep(2)
    vm.type_text(password)
    vm.qmp.key("ret")
    return time.time()


def logout(vm, user, session):
    """Saída da sessão para os ciclos. X11: SIGTERM ao wmaker, que o trata como
    o "Sair" do menu (Shutdown(WSExitMode)). Wayland: o atalho Quit do
    wlmaker (Ctrl+Alt+Backspace) enviado por QMP send-key não encerrou a
    sessão (2026-10-28), e o wlmaker não trata SIGTERM; o SIGTERM o encerra
    sem despedida dos clientes, e o startbigwm-wayland segue para o
    "big-wm-session-ctl stop" como num "Sair" normal."""
    comp = "wmaker" if session == "x11" else "wlmaker"
    rc, out, err = vm.qga.sh(f"pkill -TERM -u {user} -x {comp}; echo $?")
    return f"SIGTERM {comp} rc=" + out.strip()


def fetch_results(vm, label, dest):
    qga = vm.qga
    tgz = f"{GUEST_DIR}/{label}.tar.gz"
    rc, out, err = qga.sh(f"tar -C {GUEST_DIR}/{label} -czf {tgz} . && ls -l {tgz}", 120)
    data = qga.read_file(tgz)
    os.makedirs(dest, exist_ok=True)
    path = os.path.join(dest, "guest.tar.gz")
    with open(path, "wb") as f:
        f.write(data)
    subprocess.run(["tar", "-C", dest, "-xzf", path], check=True)
    os.unlink(path)


def cmd_boot(args):
    label = args.label or os.path.basename(os.path.abspath(args.out))
    password = open(args.password_file).read().strip()
    meta = {"label": label, "session": args.session, "image": os.path.abspath(args.image),
            "vm": VM, "samples": args.samples, "cpu_window": args.cpu_window,
            "host_start": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "setup": args.setup or []}
    vm = VMRun(args.image, args.out, keep_overlay=args.keep_overlay)
    try:
        if not vm.qga.wait(300):
            raise RuntimeError("qemu-ga não respondeu em 300 s")
        meta["t_qga"] = time.time() - vm.t_start
        prepare_guest(vm, args, label)
        samples = args.samples
        last = max(int(x) for x in samples.split(","))
        vm.qga.exec(["/usr/bin/python3", "-I", f"{GUEST_DIR}/bigwm-perf-collect.py", "run", args.user,
                     f"{GUEST_DIR}/{label}", "--samples", samples, "--cpu-window", str(args.cpu_window)],
                    wait=False)
        if not wait_greeter(vm):
            vm.screenshot("greeter-timeout")
            raise RuntimeError("greeter do wdm não apareceu")
        meta["t_greeter"] = time.time() - vm.t_start
        vm.screenshot("00-greeter")
        t_login = login(vm, password)
        meta["t_login"] = t_login - vm.t_start
        log(f"login enviado ({meta['t_login']:.1f} s após iniciar a VM); esperando {last} s + janela")
        time.sleep(60)
        vm.screenshot("01-desktop-60s")
        # Sem consultas ao convidado até o fim das amostras (não perturbar).
        time.sleep(max(0, last + args.cpu_window + 10 - 60))
        deadline = time.time() + 600
        while time.time() < deadline:
            rc, out, _ = vm.qga.sh(f"cat {GUEST_DIR}/{label}/done 2>/dev/null", 30)
            if out.strip():
                break
            time.sleep(10)
        meta["done"] = out.strip()
        vm.screenshot("02-desktop-fim")
        fetch_results(vm, label, os.path.join(args.out, "guest"))
    finally:
        meta["host_end"] = time.strftime("%Y-%m-%dT%H:%M:%S%z")
        with open(os.path.join(args.out, "meta.json"), "w") as f:
            json.dump(meta, f, indent=1)
        vm.poweroff()
    log(f"boot {label}: {meta.get('done')}")
    return 0 if meta.get("done") == "ok" else 1


def cmd_cycles(args):
    """Logins/logouts repetidos pelo wdm; listas de processos para diff."""
    label = args.label or os.path.basename(os.path.abspath(args.out))
    password = open(args.password_file).read().strip()
    vm = VMRun(args.image, args.out, keep_overlay=args.keep_overlay)
    meta = {"label": label, "session": args.session, "cycles": args.cycles, "events": []}
    gd = f"{GUEST_DIR}/{label}"
    py = ["/usr/bin/python3", "-I", f"{GUEST_DIR}/bigwm-perf-collect.py"]
    try:
        if not vm.qga.wait(300):
            raise RuntimeError("qemu-ga não respondeu")
        prepare_guest(vm, args, label)
        for i in range(1, args.cycles + 1):
            if not wait_greeter(vm, 180):
                vm.screenshot(f"c{i}-sem-greeter")
                raise RuntimeError(f"ciclo {i}: greeter não voltou")
            vm.qga.exec(py + ["procs", args.user, f"{gd}/c{i}-0-greeter.json"])
            login(vm, password)
            time.sleep(90)
            vm.screenshot(f"c{i}-1-sessao")
            rc, out, err = vm.qga.exec(py + ["procs", args.user, f"{gd}/c{i}-1-sessao.json"])
            meta["events"].append({"cycle": i, "logout": logout(vm, args.user, args.session)})
            time.sleep(20)
            vm.qga.exec(py + ["procs", args.user, f"{gd}/c{i}-2-pos-logout.json"])
            # também o estado do systemd --user (pode já ter saído)
            vm.qga.sh(f"loginctl list-sessions --no-legend >{gd}/c{i}-2-loginctl.txt 2>&1; "
                      f"systemctl --user -M {args.user}@ list-units --all --plain --no-legend --state=active,failed "
                      f">{gd}/c{i}-2-user-units.txt 2>&1; true")
        fetch_results(vm, label, os.path.join(args.out, "guest"))
    finally:
        with open(os.path.join(args.out, "meta.json"), "w") as f:
            json.dump(meta, f, indent=1)
        vm.poweroff()
    return 0


def cmd_shell(args):
    """VM com o agente; lê comandos (uma linha = um script bash) da entrada."""
    vm = VMRun(args.image, args.out, writable=args.writable, console=args.console)
    print(f"QMP {vm.qmp_path}\nQGA {vm.qga_path}\nCONSOLE {vm.con_path}", flush=True)
    try:
        if args.console:
            vm.proc.wait()
            return 0
        if not vm.qga.wait(300):
            raise RuntimeError("qemu-ga não respondeu")
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            if line.startswith("@push "):
                _, src, dst = line.split(" ", 2)
                vm.qga.write_file(dst, open(src, "rb").read())
                print(f"pushed {src} -> {dst}", flush=True)
                continue
            if line.startswith("@pull "):
                _, src, dst = line.split(" ", 2)
                open(dst, "wb").write(vm.qga.read_file(src))
                print(f"pulled {src} -> {dst}", flush=True)
                continue
            rc, out, err = vm.qga.sh(line, 3600)
            print(f"### rc={rc}\n{out}{err}", flush=True)
    finally:
        vm.poweroff()
    return 0


class Console:
    """hvc0 (agetty) por um socket: só para instalar o qemu-ga na preparação."""

    def __init__(self, path):
        deadline = time.time() + 30
        while True:
            try:
                self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.s.connect(path)
                break
            except OSError:
                if time.time() > deadline:
                    raise
                time.sleep(0.5)
        self.buf = b""

    def expect(self, pattern, timeout):
        import re as _re
        deadline = time.time() + timeout
        self.s.settimeout(1)
        while time.time() < deadline:
            m = _re.search(pattern.encode(), self.buf)
            if m:
                self.buf = self.buf[m.end():]
                return True
            try:
                chunk = self.s.recv(65536)
                if chunk:
                    self.buf += chunk
            except socket.timeout:
                pass
        raise TimeoutError(f"console: {pattern!r} não apareceu; fim: {self.buf[-300:]!r}")

    def send(self, text):
        self.s.sendall(text.encode())


def cmd_prepare(args):
    """Cadeia de imagens: 01 = 00 + qemu-ga + pacotes do main + logins de aquecimento."""
    password = open(args.password_file).read().strip()
    base = os.path.abspath(args.base)
    img = os.path.abspath(args.image)
    if os.path.exists(img):
        raise SystemExit(f"{img} já existe")
    subprocess.run(["qemu-img", "create", "-q", "-f", "qcow2", "-F", "qcow2", "-b", base, img], check=True)
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)

    # 1. qemu-guest-agent pelo console serial (hvc0), com a conta instalada.
    vm = VMRun(img, os.path.join(out, "1-qga"), writable=True, console=True)
    try:
        con = Console(vm.con_path)
        for _ in range(3):
            con.send("\r")
            try:
                con.expect(r"login: ?$|login:", 60)
                break
            except TimeoutError:
                continue
        con.send(args.user + "\r")
        con.expect(r"(Senha|Password)\s*:", 30)
        con.send(password + "\r")
        con.expect(r"[$#](\x1b\[[0-9;]*m)? ?$", 60)
        mark = "__PREP_OK__"
        con.send(f"printf '%s\\n' '{password}' | sudo -S -p '' sh -c 'pacman -S --noconfirm --needed qemu-guest-agent "
                 f"|| pacman -Sy --noconfirm --needed qemu-guest-agent; systemctl start qemu-guest-agent; "
                 f"systemctl is-active qemu-guest-agent' && echo {mark}\r")
        con.expect(mark + r"\r?\n", 900)
        log("qemu-guest-agent instalado e ativo")
        con.send("exit\r")
        if not vm.qga.wait(120):
            raise RuntimeError("qemu-ga instalado mas não responde")
        rc, o, e = vm.qga.sh("pacman -Q qemu-guest-agent; systemctl is-enabled qemu-guest-agent 2>&1; "
                             "cat /usr/lib/udev/rules.d/*qemu-guest* 2>/dev/null")
        log(o + e)
        # 2. Pacotes do main (pacman -U dos artefatos de CI).
        if args.packages:
            vm.qga.sh("rm -rf /var/cache/bigwm-perf-pkgs; mkdir -p /var/cache/bigwm-perf-pkgs")
            files = []
            for pkg in args.packages:
                dst = f"/var/cache/bigwm-perf-pkgs/{os.path.basename(pkg)}"
                vm.qga.write_file(dst, open(pkg, "rb").read())
                files.append(dst)
            rc, o, e = vm.qga.sh("pacman -U --noconfirm " + " ".join(files) + " 2>&1", 1800)
            log(f"pacman -U rc={rc}\n{o[-3000:]}")
            if rc != 0:
                raise RuntimeError("pacman -U falhou")
            vm.qga.sh("rm -rf /var/cache/bigwm-perf-pkgs")
        rc, o, e = vm.qga.sh("pacman -Q")
        open(os.path.join(out, "pacman-Q.txt"), "w").write(o)
        for cmd in args.setup or []:
            rc, o, e = vm.qga.sh(cmd, 600)
            log(f"setup {cmd!r}: rc {rc} {o[-500:]}{e[-500:]}")
    finally:
        vm.poweroff()

    # 3. Logins de aquecimento (primeiro login depois da atualização: firstrun,
    # caches de ícones/fontes), X11 e Wayland, com logout gracioso.
    for sess in ("x11", "wayland"):
        vm = VMRun(img, os.path.join(out, f"2-aquecimento-{sess}"), writable=True)
        try:
            if not vm.qga.wait(300):
                raise RuntimeError("qemu-ga não respondeu")
            vm.qga.sh(f"mkdir -p /var/lib/wdm/sessions && printf '%s\\n' '{SESSIONS[sess]}' "
                      f">/var/lib/wdm/sessions/{args.user}")
            if not wait_greeter(vm):
                raise RuntimeError("sem greeter")
            login(vm, password)
            time.sleep(150)
            vm.screenshot(f"aquecimento-{sess}")
            rc, o, e = vm.qga.sh(f"loginctl list-sessions --no-legend; ps -u {args.user} -o pid=,rss=,args= | head -80")
            open(os.path.join(out, f"aquecimento-{sess}.txt"), "w").write(o + e)
            log("logout: " + logout(vm, args.user, sess))
            time.sleep(20)
            vm.qga.sh("sync")
        finally:
            vm.poweroff()
    os.chmod(img, 0o444)
    log(f"imagem pronta: {img}")
    return 0


def http_server(files, updir):
    """HTTP só em 127.0.0.1 (o convidado chega por 10.0.2.2, slirp): GET de
    arquivos conhecidos e PUT de resultados."""
    import http.server
    import threading

    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            name = self.path.lstrip("/")
            if name not in files:
                self.send_error(404)
                return
            data = open(files[name], "rb").read()
            self.send_response(200)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def do_PUT(self):
            name = os.path.basename(self.path)
            n = int(self.headers.get("Content-Length", "0"))
            with open(os.path.join(updir, name), "wb") as f:
                f.write(self.rfile.read(n))
            self.send_response(201)
            self.end_headers()

    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv, srv.server_address[1]


def console_login(con, user, password, timeout=300):
    con.send("\r")
    con.expect(r"login:", timeout)
    con.send(user + "\r")
    con.expect(r"(Senha|Password)\s*:", 30)
    con.send(password + "\r")
    con.expect(r"[$#](\x1b\[[0-9;]*m)? ?$", 60)


def console_run(con, cmd, timeout=120):
    mark = f"__FIM{int(time.time())}__"
    con.send(f"{cmd}; echo {mark}$?\r")
    con.expect(mark + r"(\d+)", timeout)


def cmd_live(args):
    """Sessão live da ISO (sem instalar), mesma VM dos boots instalados."""
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    label = args.label or os.path.basename(out)
    srv, port = http_server({"c.py": COLLECTOR}, out)
    meta = {"label": label, "session": "x11", "live": True, "iso": os.path.abspath(args.iso), "vm": VM,
            "samples": args.samples, "cpu_window": args.cpu_window, "wizard_keys": args.wizard_keys,
            "host_start": time.strftime("%Y-%m-%dT%H:%M:%S%z")}
    vm = VMRun(args.iso, out, iso=True, console=True)
    gd = "/var/tmp/bigwm-perf-live"
    try:
        con = Console(vm.con_path)
        console_login(con, args.user, args.password)
        meta["t_console"] = time.time() - vm.t_start
        console_run(con, f"curl -sf http://10.0.2.2:{port}/c.py -o /tmp/c.py && sudo -n systemd-run "
                    f"--unit=bigwm-perf-collect --collect /usr/bin/python3 -I /tmp/c.py run {args.user} {gd} "
                    f"--existing --t0 compositor --samples {args.samples} --cpu-window {args.cpu_window}")
        con.send("exit\r")
        vm.screenshot("00-assistente")
        for i, k in enumerate(args.wizard_keys.split(",") if args.wizard_keys else []):
            time.sleep(args.wizard_delay)
            if k == "wait":
                continue
            vm.qmp.key(*k.split("+"))
            vm.screenshot(f"01-assistente-{i}-{k}")
        meta["t_wizard_done"] = time.time() - vm.t_start
        time.sleep(90)
        vm.screenshot("02-sessao")
        last = max(int(x) for x in args.samples.split(","))
        time.sleep(max(0, last + args.cpu_window + 60 - 90))
        con = Console(vm.con_path)
        console_login(con, args.user, args.password)
        for _ in range(60):
            try:
                console_run(con, f"test -e {gd}/done", 30)
                break
            except TimeoutError:
                pass
            time.sleep(10)
        console_run(con, f"sudo -n tar -C {gd} -czf /tmp/r.tgz . && curl -sf -T /tmp/r.tgz "
                    f"http://10.0.2.2:{port}/guest.tar.gz", 120)
        vm.screenshot("03-fim")
        dest = os.path.join(out, "guest")
        os.makedirs(dest, exist_ok=True)
        subprocess.run(["tar", "-C", dest, "-xzf", os.path.join(out, "guest.tar.gz")], check=True)
        os.unlink(os.path.join(out, "guest.tar.gz"))
        meta["done"] = open(os.path.join(dest, "done")).read().strip() if os.path.exists(os.path.join(dest, "done")) else None
        con.send("sudo -n poweroff\r")
    finally:
        meta["host_end"] = time.strftime("%Y-%m-%dT%H:%M:%S%z")
        with open(os.path.join(out, "meta.json"), "w") as f:
            json.dump(meta, f, indent=1)
        try:
            vm.proc.wait(120)
        except subprocess.TimeoutExpired:
            vm.poweroff()
        srv.shutdown()
    return 0 if meta.get("done") == "ok" else 1


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="mode", required=True)
    p = sub.add_parser("prepare")
    p.add_argument("--base", required=True)
    p.add_argument("--image", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--user", default="openqa")
    p.add_argument("--password-file", required=True)
    p.add_argument("--packages", nargs="*")
    p.add_argument("--setup", action="append")
    for name in ("boot", "cycles"):
        p = sub.add_parser(name)
        p.add_argument("--image", required=True)
        p.add_argument("--session", choices=SESSIONS, required=True)
        p.add_argument("--out", required=True)
        p.add_argument("--user", default="openqa")
        p.add_argument("--password-file", required=True)
        p.add_argument("--label")
        p.add_argument("--samples", default="30,90,300,900")
        p.add_argument("--cpu-window", type=int, default=30)
        p.add_argument("--setup", action="append", help="comando bash (root) antes do login")
        p.add_argument("--keep-overlay", action="store_true")
        if name == "cycles":
            p.add_argument("--cycles", type=int, default=3)
    p = sub.add_parser("live")
    p.add_argument("--iso", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--label")
    p.add_argument("--user", default="community")
    p.add_argument("--password", default="big", help="senha pública do usuário live da BigCommunity")
    p.add_argument("--samples", default="30,90,300,900")
    p.add_argument("--cpu-window", type=int, default=30)
    p.add_argument("--wizard-keys", default="ret,ret,ret",
                   help="teclas (qcode, '+' para combinar, 'wait' para só esperar) que concluem o assistente")
    p.add_argument("--wizard-delay", type=float, default=8)
    p = sub.add_parser("shell")
    p.add_argument("--image", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--writable", action="store_true")
    p.add_argument("--console", action="store_true")
    args = ap.parse_args()
    return {"boot": cmd_boot, "cycles": cmd_cycles, "shell": cmd_shell, "prepare": cmd_prepare, "live": cmd_live}[args.mode](args)


if __name__ == "__main__":
    sys.exit(main())
