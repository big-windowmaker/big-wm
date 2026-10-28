#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-pressure.py - teste de pressão de memória (no convidado, root),
# para comparar ajustes do kernel (vm.watermark_scale_factor,
# vm.min_free_kbytes) pela resposta do desktop, não pelo número do
# MemAvailable.
#
#   bigwm-perf-pressure.py USUÁRIO SAÍDA.json [--alvo-mib N] [--manter-s S]
#
# Com a sessão X11 do USUÁRIO aberta:
#   1. linha de base: 3 aberturas do gnome-calculator (tempo até a janela
#      aparecer na _NET_CLIENT_LIST do Window Maker) e a latência de um
#      comando curto (python3 -c pass);
#   2. pressão: um processo aloca memória anônima incompressível em blocos
#      de 64 MiB até o alvo (padrão: MemTotal - 600 MiB, igual para todas
#      as configurações) e continua tocando nela, enquanto outro lê um
#      arquivo de 1 GiB em laço (cache de páginas disputando a memória);
#   3. no pico, durante --manter-s (padrão 120 s): uma abertura do aplicativo
#      a cada 15 s e o comando curto a cada 2 s;
#   4. guarda deltas de /proc/vmstat (reclaim direto, kswapd, swap, refaults,
#      OOM), PSI (some/full, total em µs) e as latências.
# O processo de pressão é encerrado no fim; o arquivo é apagado.
import ctypes
import json
import os
import signal
import subprocess
import sys
import time

MiB = 1 << 20


def mono():
    return time.clock_gettime(time.CLOCK_MONOTONIC)


def read(p):
    with open(p) as f:
        return f.read()


def vmstat():
    return {k: int(v) for k, v in (l.split() for l in read("/proc/vmstat").splitlines())}


def psi():
    out = {}
    for r in ("memory", "cpu", "io"):
        for line in read(f"/proc/pressure/{r}").splitlines():
            kind, *f = line.split()
            out[f"{r}_{kind}"] = {k: float(v) for k, v in (x.split("=") for x in f)}
    return out


def meminfo():
    return {l.split(":")[0]: int(l.split()[1]) for l in read("/proc/meminfo").splitlines()}


def session_env(user):
    uid = int(next(l for l in open("/etc/passwd") if l.split(":")[0] == user).split(":")[2])
    for pid in os.listdir("/proc"):
        if not pid.isdigit():
            continue
        try:
            if read(f"/proc/{pid}/comm").strip() != "wmaker" or os.stat(f"/proc/{pid}").st_uid != uid:
                continue
            env = dict(x.split("=", 1) for x in open(f"/proc/{pid}/environ", "rb").read().decode(errors="replace").split("\0") if "=" in x)
            return uid, {k: env[k] for k in ("DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS", "XDG_RUNTIME_DIR",
                                              "HOME", "LANG", "GSK_RENDERER", "GTK_THEME", "LD_PRELOAD", "PATH") if k in env}
        except OSError:
            continue
    raise SystemExit("sessão X11 (wmaker) do usuário não encontrada")


class X:
    def __init__(self, display, xauth):
        os.environ["DISPLAY"], os.environ["XAUTHORITY"] = display, xauth
        self.x = ctypes.CDLL("libX11.so.6")
        self.x.XOpenDisplay.restype = ctypes.c_void_p
        self.x.XOpenDisplay.argtypes = [ctypes.c_char_p]
        self.x.XDefaultRootWindow.restype = ctypes.c_ulong
        self.x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        self.x.XInternAtom.restype = ctypes.c_ulong
        self.x.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        self.x.XGetWindowProperty.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_long,
                                              ctypes.c_long, ctypes.c_int, ctypes.c_ulong,
                                              ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_int),
                                              ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong),
                                              ctypes.POINTER(ctypes.c_void_p)]
        self.x.XFree.argtypes = [ctypes.c_void_p]
        self.d = self.x.XOpenDisplay(None)
        if not self.d:
            raise SystemExit("XOpenDisplay falhou")
        self.root = self.x.XDefaultRootWindow(self.d)
        self.atom = self.x.XInternAtom(self.d, b"_NET_CLIENT_LIST", 0)

    def clients(self):
        t, f, n, b = ctypes.c_ulong(), ctypes.c_int(), ctypes.c_ulong(), ctypes.c_ulong()
        data = ctypes.c_void_p()
        self.x.XGetWindowProperty(self.d, self.root, self.atom, 0, 4096, 0, 0, ctypes.byref(t), ctypes.byref(f),
                                  ctypes.byref(n), ctypes.byref(b), ctypes.byref(data))
        out = set()
        if data.value:
            arr = ctypes.cast(data, ctypes.POINTER(ctypes.c_ulong))
            out = {arr[i] for i in range(n.value)}
            self.x.XFree(data)
        return out


def gui_launch(xc, uid, env, timeout=60):
    """Tempo (s) do exec do gnome-calculator até a janela entrar na lista."""
    before = xc.clients()
    t0 = mono()
    p = subprocess.Popen(["setpriv", f"--reuid={uid}", f"--regid={uid}", "--init-groups", "env", "-i",
                          *[f"{k}={v}" for k, v in env.items()], "gnome-calculator"],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    dt = None
    while mono() - t0 < timeout:
        if xc.clients() - before:
            dt = mono() - t0
            break
        time.sleep(0.02)
    try:
        os.killpg(p.pid, signal.SIGTERM)
    except OSError:
        pass
    try:
        p.wait(10)
    except subprocess.TimeoutExpired:
        os.killpg(p.pid, signal.SIGKILL)
    # espera a janela sumir
    t1 = mono()
    while xc.clients() - before and mono() - t1 < 10:
        time.sleep(0.05)
    return dt


def short_cmd():
    t0 = mono()
    subprocess.run(["python3", "-c", "pass"], check=False)
    return mono() - t0


HOG = r'''
import mmap, os, sys, time
target = int(sys.argv[1]) << 20
chunk = 64 << 20
rnd = os.urandom(1 << 20)  # páginas incompressíveis: o zram não as absorve de graça
bufs = []
t0 = time.monotonic()
while sum(len(b) for b in bufs) < target:
    m = mmap.mmap(-1, chunk)
    for off in range(0, chunk, 1 << 20):
        m[off:off + (1 << 20)] = rnd
    bufs.append(m)
    print(f"alloc {len(bufs) * 64} {time.monotonic() - t0:.2f}", flush=True)
print("ALVO", flush=True)
while True:
    for m in bufs:
        for off in range(0, chunk, 4096 * 16):
            m[off] = (m[off] + 1) & 0xff
    time.sleep(0.5)
'''


def main(argv):
    user, out = argv[1], argv[2]
    opts = dict(zip(argv[3::2], argv[4::2]))
    hold = int(opts.get("--manter-s", "120"))
    mi0 = meminfo()
    # Alvo fixo em MiB (igual para todas as configurações comparadas): o
    # padrão deixa ~600 MiB para o resto do sistema, kernel e cache.
    alvo = int(opts.get("--alvo-mib", str(mi0["MemTotal"] // 1024 - 600)))
    uid, env = session_env(user)
    # wdm não exporta XAUTHORITY: vale o ~/.Xauthority do usuário.
    env.setdefault("XAUTHORITY", os.path.join(env.get("HOME", f"/home/{user}"), ".Xauthority"))
    xc = X(env["DISPLAY"], env["XAUTHORITY"])
    res = {"sysctl": {k: read(f"/proc/sys/vm/{k}").strip() for k in
                      ("watermark_scale_factor", "watermark_boost_factor", "min_free_kbytes", "swappiness")},
           "alvo_mib": alvo, "meminfo_antes": mi0}
    # 1. linha de base
    res["base_gui_s"] = [gui_launch(xc, uid, env) for _ in range(3)]
    res["base_cmd_s"] = [short_cmd() for _ in range(5)]
    # arquivo para o laço de leitura (cache de páginas)
    big = "/var/tmp/bigwm-perf-pressure.bin"
    subprocess.run(["dd", "if=/dev/urandom", f"of={big}", "bs=1M", "count=1024", "status=none"], check=True)
    subprocess.run(["sync"])
    v0, p0, t0 = vmstat(), psi(), mono()
    reader = subprocess.Popen(["sh", "-c", f"while :; do cat {big} >/dev/null; done"], start_new_session=True)
    hog = subprocess.Popen(["python3", "-I", "-c", HOG, str(alvo)], stdout=subprocess.PIPE, text=True,
                           start_new_session=True)
    alloc = []
    reached = None
    oom = False
    deadline = mono() + 600
    while mono() < deadline:
        line = hog.stdout.readline()
        if not line:
            oom = True  # o hog morreu (OOM)
            break
        if line.startswith("ALVO"):
            reached = mono() - t0
            break
        alloc.append(line.split()[1:])
    res["alvo_atingido_s"] = reached
    res["hog_morreu"] = oom
    res["alocacao"] = alloc[::4]
    # 3. pico
    gui, cmd = [], []
    end = mono() + hold
    next_gui = mono()
    samples = []
    while mono() < end:
        if mono() >= next_gui:
            gui.append(gui_launch(xc, uid, env))
            next_gui = mono() + 15
        cmd.append(short_cmd())
        m = meminfo()
        samples.append({"t": mono() - t0, "MemAvailable": m["MemAvailable"], "MemFree": m["MemFree"],
                        "SwapUsed": m["SwapTotal"] - m["SwapFree"], "Cached": m["Cached"]})
        time.sleep(2)
    v1, p1, t1 = vmstat(), psi(), mono()
    for p in (hog, reader):
        try:
            os.killpg(p.pid, signal.SIGKILL)
        except OSError:
            pass
    os.unlink(big)
    keys = [k for k in v1 if k.startswith(("allocstall", "pgscan", "pgsteal", "pswp", "workingset_refault",
                                           "oom_kill", "compact_stall", "kswapd", "pageoutrun", "pgmajfault",
                                           "drop_slab", "zone_reclaim"))]
    res["vmstat_delta"] = {k: v1[k] - v0.get(k, 0) for k in keys}
    res["psi_total_us_delta"] = {k: p1[k]["total"] - p0[k]["total"] for k in p1}
    res["psi_fim"] = p1
    res["duracao_s"] = t1 - t0
    res["pico_gui_s"] = gui
    res["pico_cmd_s"] = cmd
    res["amostras"] = samples
    res["dmesg_oom"] = subprocess.run(["sh", "-c", "dmesg | grep -iE 'out of memory|oom-kill|killed process' | tail -20"],
                                      capture_output=True, text=True).stdout
    with open(out, "w") as f:
        json.dump(res, f)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
