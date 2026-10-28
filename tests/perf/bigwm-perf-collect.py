#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-collect.py - coletor de memória/CPU do Big WindowMaker, roda
# DENTRO da VM de benchmark, como root (iniciado pelo qemu-guest-agent a partir
# do host, ver tests/perf/bigwm-perf-vm.py). Só a biblioteca padrão do Python;
# só lê /proc, /sys e /run (nada de drop_caches, nada é alterado no sistema).
#
#   bigwm-perf-collect.py run USUÁRIO DIR [--samples 30,90,300,900] [--cpu-window 30]
#       [--existing] [--t0 session|compositor]
#       Espera a próxima sessão gráfica do USUÁRIO no logind (--existing: usa
#       a sessão já aberta, caso do login automático da live), mede o tempo até
#       a área de trabalho ficar pronta e tira as amostras nos tempos pedidos
#       (segundos depois da abertura da sessão, ou do início do wmaker/wlmaker
#       com --t0 compositor). Cada amostra vai para
#       DIR/sample-<t>.json; no fim, DIR/inventory.json e DIR/done.
#   bigwm-perf-collect.py sample USUÁRIO ARQUIVO [--cpu-window S]
#       Uma amostra avulsa.
#   bigwm-perf-collect.py procs USUÁRIO ARQUIVO
#       Só a lista de processos (para os ciclos de logout/login).
#
# Atribuição à sessão gráfica (critério registrado em cada amostra):
#   "sessao"  = processos cujo cgroup v2 está em /user.slice/user-<UID>.slice/
#               (user@UID.service inteiro -- systemd --user, D-Bus, autostart,
#               portais, pipewire -- e as session-N.scope do logind, com todos
#               os filhos);
#   "servidor_grafico" = Xorg (no X11 o wdm o inicia como root, fora da
#               user.slice); no Wayland o compositor e o Xwayland já estão na
#               sessão;
#   "ferramenta" = o próprio coletor e o qemu-ga, isto é, o cgroup
#               qemu-guest-agent.service ou uma unidade bigwm-perf* do
#               systemd-run (VM live por SSH) (excluídos de todos os
#               totais de PSS e mostrados à parte; MemTotal - MemAvailable os
#               inclui, porque o kernel não separa).
import json
import os
import re
import sys
import time

PAGE = os.sysconf("SC_PAGE_SIZE")
HZ = os.sysconf("SC_CLK_TCK")
SELF = os.getpid()


def read(path, default=None):
    try:
        with open(path, "rb") as f:
            return f.read().decode("utf-8", "replace")
    except OSError:
        return default


def mono():
    return time.clock_gettime(time.CLOCK_MONOTONIC)


def meminfo():
    out = {}
    for line in (read("/proc/meminfo") or "").splitlines():
        k, _, v = line.partition(":")
        out[k.strip()] = int(v.split()[0])
    return out


def pressure():
    out = {}
    for res in ("cpu", "memory", "io"):
        txt = read(f"/proc/pressure/{res}")
        if txt is None:
            continue
        out[res] = {}
        for line in txt.splitlines():
            kind, *fields = line.split()
            out[res][kind] = {k: float(v) for k, v in (f.split("=") for f in fields)}
    return out


def zram():
    out = {}
    for dev in sorted(os.listdir("/sys/block")):
        if not dev.startswith("zram"):
            continue
        mm = read(f"/sys/block/{dev}/mm_stat")
        size = read(f"/sys/block/{dev}/disksize")
        if mm:
            f = [int(x) for x in mm.split()]
            out[dev] = {"disksize": int(size or 0), "orig_data_size": f[0],
                        "compr_data_size": f[1], "mem_used_total": f[2]}
    return out


def ancestors_of_self():
    """O próprio coletor (os filhos do qemu-ga saem pelo cgroup)."""
    return {SELF}


def proc_stat(pid):
    st = read(f"/proc/{pid}/stat")
    if not st:
        return None
    comm = st.split("(", 1)[1].rsplit(")", 1)[0]
    f = st.rsplit(")", 1)[1].split()
    # f[0]=state f[1]=ppid ... f[11]=utime f[12]=stime f[19]=starttime
    return {"comm": comm, "state": f[0], "ppid": int(f[1]),
            "cpu_ticks": int(f[11]) + int(f[12]), "start_ticks": int(f[19])}


def ctxt(pid):
    txt = read(f"/proc/{pid}/status") or ""
    v = re.search(r"^voluntary_ctxt_switches:\s+(\d+)", txt, re.M)
    n = re.search(r"^nonvoluntary_ctxt_switches:\s+(\d+)", txt, re.M)
    u = re.search(r"^Uid:\s+(\d+)", txt, re.M)
    return (int(v[1]) if v else 0) + (int(n[1]) if n else 0), (int(u[1]) if u else -1)


ROLLUP_KEYS = ("Rss", "Pss", "Pss_Anon", "Pss_File", "Pss_Shmem", "Shared_Clean",
               "Shared_Dirty", "Private_Clean", "Private_Dirty", "Private_Hugetlb",
               "Swap", "SwapPss")


def processes(uid, tool):
    procs = []
    for name in os.listdir("/proc"):
        if not name.isdigit():
            continue
        pid = int(name)
        st = proc_stat(pid)
        if st is None:
            continue
        cg = read(f"/proc/{pid}/cgroup") or ""
        cgroup = ""
        for line in cg.splitlines():
            if line.startswith("0::"):
                cgroup = line[3:]
        cmd = (read(f"/proc/{pid}/cmdline") or "").replace("\0", " ").strip()
        sw, puid = ctxt(pid)
        p = {"pid": pid, "ppid": st["ppid"], "uid": puid, "comm": st["comm"],
             "state": st["state"], "cmd": cmd[:300], "cgroup": cgroup,
             "cpu_ticks": st["cpu_ticks"], "start_ticks": st["start_ticks"], "ctxt": sw}
        roll = read(f"/proc/{pid}/smaps_rollup")
        if roll:
            mem = {}
            for line in roll.splitlines()[1:]:
                k, _, v = line.partition(":")
                if k in ROLLUP_KEYS:
                    mem[k] = int(v.split()[0])
            if mem:
                mem["Uss"] = mem.get("Private_Clean", 0) + mem.get("Private_Dirty", 0) + mem.get("Private_Hugetlb", 0)
                p["mem"] = mem
        if not cmd and "mem" not in p:
            p["kthread"] = True
        if pid in tool or cgroup.endswith("/qemu-guest-agent.service") or "bigwm-perf" in cgroup:
            p["group"] = "ferramenta"
        elif cgroup.startswith(f"/user.slice/user-{uid}.slice/"):
            p["group"] = "sessao"
        elif st["comm"] == "Xorg":
            p["group"] = "servidor_grafico"
        elif p.get("kthread"):
            p["group"] = "kernel"
        else:
            p["group"] = "sistema"
        procs.append(p)
    return procs


def cpu_totals():
    line = (read("/proc/stat") or "").splitlines()[0].split()
    vals = [int(x) for x in line[1:]]
    names = ["user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal", "guest", "guest_nice"]
    d = dict(zip(names, vals))
    txt = read("/proc/stat") or ""
    m = re.search(r"^ctxt (\d+)", txt, re.M)
    d["ctxt"] = int(m[1]) if m else 0
    m = re.search(r"^intr (\d+)", txt, re.M)
    d["intr"] = int(m[1]) if m else 0
    return d


def interrupts():
    out = {}
    lines = (read("/proc/interrupts") or "").splitlines()
    if not lines:
        return out
    ncpu = len(lines[0].split())
    for line in lines[1:]:
        parts = line.split()
        if not parts:
            continue
        name = parts[0].rstrip(":")
        counts = []
        for x in parts[1:1 + ncpu]:
            if x.isdigit():
                counts.append(int(x))
        desc = " ".join(parts[1 + len(counts):])
        out[name] = {"n": sum(counts), "desc": desc[:60]}
    return out


def totals(procs):
    t = {}
    for p in procs:
        g = p["group"]
        e = t.setdefault(g, {"n": 0, "pss": 0, "rss": 0, "uss": 0, "pss_anon": 0, "swap_pss": 0})
        e["n"] += 1
        m = p.get("mem")
        if m:
            e["pss"] += m.get("Pss", 0)
            e["rss"] += m.get("Rss", 0)
            e["uss"] += m.get("Uss", 0)
            e["pss_anon"] += m.get("Pss_Anon", 0)
            e["swap_pss"] += m.get("SwapPss", 0)
    return t


def snapshot(user, cpu_window=0):
    uid = int(re.search(r"^[^:]*:[^:]*:(\d+):", next(
        l for l in open("/etc/passwd") if l.split(":")[0] == user))[1])
    t0 = mono()
    # meminfo primeiro: antes de o coletor ler /proc inteiro.
    mi = meminfo()
    snap = {"schema": 1, "user": user, "uid": uid, "mono": t0, "wall": time.time(),
            "uptime": float(read("/proc/uptime").split()[0]), "meminfo": mi,
            "used_kib": mi["MemTotal"] - mi["MemAvailable"], "pressure": pressure(),
            "swaps": read("/proc/swaps"), "zram": zram()}
    tool = ancestors_of_self()
    procs = processes(uid, tool)
    snap["collect_seconds"] = mono() - t0
    snap["xwayland"] = any(p["comm"] == "Xwayland" for p in procs)
    snap["session_type"] = session_type(user)
    if cpu_window:
        a_cpu, a_irq = cpu_totals(), interrupts()
        a = {p["pid"]: (p["start_ticks"], p["cpu_ticks"], p["ctxt"]) for p in procs}
        time.sleep(cpu_window)
        b_cpu, b_irq = cpu_totals(), interrupts()
        snap["cpu_window"] = {"seconds": cpu_window,
                              "delta": {k: b_cpu[k] - a_cpu[k] for k in a_cpu},
                              "hz": HZ, "ncpu": os.cpu_count()}
        snap["irq_delta"] = {k: b_irq[k]["n"] - v["n"] for k, v in a_irq.items()
                             if k in b_irq and b_irq[k]["n"] - v["n"] > 0}
        snap["irq_desc"] = {k: v["desc"] for k, v in b_irq.items() if k in snap["irq_delta"]}
        for p in procs:
            st = proc_stat(p["pid"])
            prev = a.get(p["pid"])
            if st and prev and st["start_ticks"] == prev[0]:
                p["cpu_delta_ticks"] = st["cpu_ticks"] - prev[1]
                p["ctxt_delta"] = ctxt(p["pid"])[0] - prev[2]
        # processos que nasceram durante a janela (sinal de fork periódico)
        b_pids = set(int(x) for x in os.listdir("/proc") if x.isdigit())
        snap["spawned_during_window"] = []
        for pid in sorted(b_pids - set(a)):
            st = proc_stat(pid)
            if st and pid not in tool and pid != os.getpid():
                snap["spawned_during_window"].append({"pid": pid, "comm": st["comm"], "ppid": st["ppid"]})
    snap["processes"] = procs
    snap["totals"] = totals(procs)
    return snap


def sessions_of(user):
    """Sessões do logind lidas de /run/systemd/sessions (sem criar processos)."""
    out = []
    try:
        names = os.listdir("/run/systemd/sessions")
    except OSError:
        return out
    for n in names:
        if n.endswith(".ref"):
            continue
        txt = read(f"/run/systemd/sessions/{n}") or ""
        kv = dict(l.split("=", 1) for l in txt.splitlines() if "=" in l)
        if kv.get("USER") == user or kv.get("NAME") == user:
            kv["ID"] = n
            out.append(kv)
    return out


def session_type(user):
    for s in sessions_of(user):
        if s.get("CLASS") == "user" and s.get("TYPE") in ("x11", "wayland") and s.get("ACTIVE", "1") == "1":
            return {"id": s["ID"], "type": s.get("TYPE"), "desktop": s.get("DESKTOP"),
                    "service": s.get("SERVICE"), "monotonic_usec": int(s.get("MONOTONIC", "0"))}
    return None


def user_systemctl(user, *args):
    import subprocess
    r = subprocess.run(["systemctl", "--user", "-M", f"{user}@", *args],
                       capture_output=True, text=True, timeout=20)
    return r.returncode, r.stdout


def run_cmd(*args, timeout=60):
    import subprocess
    try:
        r = subprocess.run(list(args), capture_output=True, text=True, timeout=timeout)
        return {"rc": r.returncode, "out": r.stdout[-200000:], "err": r.stderr[-4000:]}
    except Exception as e:  # noqa: BLE001 - registra e segue
        return {"rc": None, "err": repr(e)}


def inventory(user):
    inv = {}
    inv["systemd_analyze"] = run_cmd("systemd-analyze")
    inv["systemd_analyze_blame"] = run_cmd("systemd-analyze", "blame", "--no-pager")
    inv["system_units"] = run_cmd("systemctl", "list-units", "--all", "--no-pager", "--plain", "--no-legend")
    inv["user_units"] = run_cmd("systemctl", "--user", "-M", f"{user}@", "list-units", "--all",
                                "--no-pager", "--plain", "--no-legend")
    inv["user_unit_files"] = run_cmd("systemctl", "--user", "-M", f"{user}@", "list-unit-files",
                                     "--no-pager", "--plain", "--no-legend", "--state=enabled,static,generated")
    inv["user_failed"] = run_cmd("systemctl", "--user", "-M", f"{user}@", "--failed", "--no-pager", "--plain")
    inv["system_failed"] = run_cmd("systemctl", "--failed", "--no-pager", "--plain")
    inv["user_env"] = run_cmd("systemctl", "--user", "-M", f"{user}@", "show-environment")
    inv["busctl_user"] = run_cmd("busctl", "--user", "-M", f"{user}@", "list", "--no-pager", "--acquired")
    inv["busctl_user_activatable"] = run_cmd("busctl", "--user", "-M", f"{user}@", "list", "--no-pager", "--activatable")
    # Reservas do kernel que entram em MemTotal - MemAvailable sem serem uso
    # de ninguém (marcas d'água e lowmem_reserve das zonas).
    inv["sysctl_vm"] = run_cmd("sysctl", "vm")
    inv["zoneinfo"] = read("/proc/zoneinfo")
    inv["sysctl_files"] = run_cmd("sh", "-c", "grep -rH . /etc/sysctl.d /usr/lib/sysctl.d /run/sysctl.d /etc/sysctl.conf 2>/dev/null | grep -v ':#'")
    inv["timers"] = run_cmd("systemctl", "list-timers", "--all", "--no-pager")
    inv["journal_boot_warnings"] = run_cmd("journalctl", "-b", "-p", "warning", "--no-pager", "-o", "short-monotonic")
    inv["loginctl"] = run_cmd("loginctl", "list-sessions", "--no-legend")
    inv["uname"] = run_cmd("uname", "-a")
    inv["pacman_Q"] = run_cmd("pacman", "-Q")
    inv["locale"] = run_cmd("localectl", "status")
    inv["cmdline"] = read("/proc/cmdline")
    inv["xrandr_modes"] = read("/sys/class/drm/card0-Virtual-1/modes") or read("/sys/class/drm/card1-Virtual-1/modes")
    inv["perf_available"] = any(os.access(os.path.join(d, "perf"), os.X_OK) for d in ("/usr/bin", "/usr/sbin"))
    auto = {}
    for d in ("/etc/xdg/autostart", f"/home/{user}/.config/autostart"):
        try:
            for f in sorted(os.listdir(d)):
                if f.endswith(".desktop"):
                    txt = read(os.path.join(d, f)) or ""
                    keys = {}
                    for line in txt.splitlines():
                        if line.startswith("[") and line.strip() != "[Desktop Entry]":
                            break
                        k, _, v = line.partition("=")
                        if k in ("Exec", "OnlyShowIn", "NotShowIn", "Hidden", "TryExec",
                                 "X-GNOME-Autostart-enabled", "X-KDE-autostart-condition",
                                 "X-GNOME-AutoRestart", "X-systemd-skip", "AutostartCondition"):
                            keys[k] = v
                    auto[os.path.join(d, f)] = keys
        except OSError:
            pass
    inv["xdg_autostart"] = auto
    return inv


def wait_session(user, timeout, log):
    """Espera uma sessão gráfica NOVA (aberta depois do início deste comando)."""
    start = mono()
    known = {s["ID"] for s in sessions_of(user)}
    while mono() - start < timeout:
        for s in sessions_of(user):
            if s["ID"] in known:
                continue
            if s.get("TYPE") in ("x11", "wayland") and s.get("CLASS") == "user":
                return s
        time.sleep(0.2)
    return None


def wait_ready(user, uid, session_mono, timeout, log):
    """Área de trabalho pronta: compositor rodando, big-wm-session.target ativo
    e a fila de jobs do systemd --user vazia."""
    comp = ("wmaker", "wlmaker")
    deadline = mono() + timeout
    marks = {}
    while mono() < deadline:
        have = False
        for name in os.listdir("/proc"):
            if name.isdigit():
                st = proc_stat(int(name))
                if st and st["comm"] in comp and ctxt(int(name))[1] == uid:
                    have = True
                    break
        if have and "compositor" not in marks:
            marks["compositor"] = mono() - session_mono
        if have:
            rc, out = user_systemctl(user, "is-active", "big-wm-session.target")
            if out.strip() == "active":
                marks.setdefault("target", mono() - session_mono)
                rc, jobs = user_systemctl(user, "list-jobs", "--no-legend", "--plain")
                if rc == 0 and not jobs.strip():
                    marks["ready"] = mono() - session_mono
                    return marks
                # o que ainda segura a fila (para achar a unidade lenta)
                marks["jobs_pendentes"] = jobs.strip()[:2000]
                marks["jobs_pendentes_t"] = mono() - session_mono
        time.sleep(0.5)
    marks["timeout"] = True
    return marks


def compositor_start(uid, timeout):
    """Instante (CLOCK_MONOTONIC) em que o wmaker/wlmaker do usuário nasceu."""
    deadline = mono() + timeout
    while mono() < deadline:
        found = []
        for name in os.listdir("/proc"):
            if name.isdigit():
                st = proc_stat(int(name))
                if st and st["comm"] in ("wmaker", "wlmaker") and ctxt(int(name))[1] == uid:
                    found.append(st["start_ticks"] / HZ)
        if found:
            # starttime conta desde o boot (CLOCK_BOOTTIME); numa VM sem
            # suspensão é igual ao CLOCK_MONOTONIC.
            return min(found)
        time.sleep(0.5)
    return None


def cmd_run(user, outdir, samples, cpu_window, existing=False, t0="session"):
    os.makedirs(outdir, exist_ok=True)
    logf = open(os.path.join(outdir, "collect.log"), "a")

    def log(msg):
        logf.write(f"{mono():.2f} {msg}\n")
        logf.flush()

    uid = int(next(l for l in open("/etc/passwd") if l.split(":")[0] == user).split(":")[2])
    log(f"esperando sessão de {user} (uid {uid}), existente={existing}, t0={t0}")
    s = None
    if existing:
        st = session_type(user)
        if st:
            s = next(x for x in sessions_of(user) if x["ID"] == st["id"])
    if not s:
        s = wait_session(user, 600, log)
    if not s:
        log("nenhuma sessão nova em 600 s")
        open(os.path.join(outdir, "done"), "w").write("no-session\n")
        return 1
    sess_mono = int(s.get("MONOTONIC", "0")) / 1e6 or mono()
    log(f"sessão {s['ID']} tipo {s.get('TYPE')} desktop {s.get('DESKTOP')} mono {sess_mono:.3f}")
    if t0 == "compositor":
        # Live: o autologin abre a sessão antes do assistente; o relógio das
        # amostras começa quando o Window Maker nasce.
        c = compositor_start(uid, 1800)
        if c is None:
            log("compositor não apareceu")
            open(os.path.join(outdir, "done"), "w").write("no-compositor\n")
            return 1
        log(f"t0 = início do compositor, mono {c:.3f} (sessão +{c - sess_mono:.1f} s)")
        s["SESSION_MONOTONIC_ORIG"] = str(int(sess_mono * 1e6))
        sess_mono = c
    marks = wait_ready(user, uid, sess_mono, 300, log)
    log(f"marcas {marks}")
    with open(os.path.join(outdir, "login.json"), "w") as f:
        json.dump({"session": s, "session_mono": sess_mono, "marks": marks}, f, indent=1)
    for t in samples:
        delay = sess_mono + t - mono()
        if delay > 0:
            time.sleep(delay)
        log(f"amostra {t}s (atraso {mono() - sess_mono - t:.2f} s)")
        snap = snapshot(user, cpu_window)
        snap["sample_t"] = t
        snap["sample_actual_t"] = snap["mono"] - sess_mono
        with open(os.path.join(outdir, f"sample-{t}.json"), "w") as f:
            json.dump(snap, f)
    log("inventário")
    with open(os.path.join(outdir, "inventory.json"), "w") as f:
        json.dump(inventory(user), f)
    open(os.path.join(outdir, "done"), "w").write("ok\n")
    log("fim")
    return 0


def main(argv):
    if len(argv) < 4:
        print(__doc__ or "uso: ver o cabeçalho", file=sys.stderr)
        return 2
    mode, user, target = argv[1], argv[2], argv[3]
    rest = [a for a in argv[4:] if a != "--existing"]
    opts = dict(zip(rest[0::2], rest[1::2]))
    cpu_window = int(opts.get("--cpu-window", "30"))
    if mode == "run":
        samples = [int(x) for x in opts.get("--samples", "30,90,300,900").split(",")]
        return cmd_run(user, target, samples, cpu_window, existing="--existing" in argv,
                       t0=opts.get("--t0", "session"))
    if mode == "sample":
        snap = snapshot(user, cpu_window)
        with open(target, "w") as f:
            json.dump(snap, f)
        return 0
    if mode == "procs":
        snap = snapshot(user, 0)
        with open(target, "w") as f:
            json.dump(snap, f)
        return 0
    print(f"modo desconhecido: {mode}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
