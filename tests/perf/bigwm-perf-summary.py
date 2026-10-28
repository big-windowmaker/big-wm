#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-summary.py - resume os boots medidos por bigwm-perf-vm.py.
#
#   bigwm-perf-summary.py RESULTADOS/ [--out DIR]
#
# RESULTADOS/<perfil>/<sessão>-<n>/ é um boot (meta.json + guest/*.json).
# Gera em DIR (padrão RESULTADOS/):
#   amostras.tsv     uma linha por boot e tempo de amostra (todas as métricas)
#   boots.tsv        uma linha por boot (tempo de boot, login, versão)
#   resumo.tsv       mediana, mínimo e máximo por perfil, sessão e tempo
#   componentes.tsv  PSS/RSS/USS, CPU e trocas de contexto por componente
#                    (mediana entre os boots) por perfil, sessão e tempo
#   resumo.md        as tabelas acima em Markdown
#
# Definições (MiB = 1024 KiB):
#   ram_usada     MemTotal - MemAvailable (/proc/meminfo, todo o sistema)
#   pss_todos     soma do PSS de todos os processos, menos a ferramenta
#                 (qemu-guest-agent.service e o coletor)
#   pss_sessao    soma do PSS dos processos em /user.slice/user-<UID>.slice/
#   pss_sessao_x  pss_sessao + Xorg (no X11 o Xorg é do wdm, fora da sessão)
#   usada_free    MemTotal - MemFree - Buffers - Cached - SReclaimable (a
#                 coluna "usada" do free(1)); a diferença para ram_usada é
#                 sobretudo a reserva de memória livre do kernel (marcas
#                 d'água), que o MemAvailable desconta
#   cpu_ocupada   100 - ocioso% de todas as CPUs na janela de 30 s seguinte
#                 à amostra (/proc/stat); ctxt_s e intr_s por segundo
import gzip
import json
import os
import re
import statistics
import sys

INTERP = {"python3", "python", "python3.14", "python3.13", "bash", "sh", "perl", "env", "dash"}


def mib(kib):
    return kib / 1024.0


def component(p):
    """Nome do componente: o comm, ou o script quando é um interpretador."""
    comm = p["comm"]
    args = p["cmd"].split()
    if comm in INTERP or comm.startswith("python"):
        for a in args[1:]:
            if a.startswith("-") or "=" in a:
                continue
            return os.path.basename(a)
    if comm == "wmaker" and "--for-real" in p["cmd"]:
        return "wmaker --for-real"
    if comm.startswith("dbus-") and "--session" in p["cmd"]:
        return comm + " (sessão)"
    if comm in ("systemd",) and p["uid"] != 0:
        return "systemd --user"
    if comm == "(sd-pam)":
        return "(sd-pam)"
    # comm é cortado em 15 caracteres: o nome completo vem do argv (o
    # programa ou, num interpretador que renomeia o processo, o script).
    if len(comm) == 15 and args:
        for a in args[:3]:
            base = os.path.basename(a)
            if base.startswith(comm):
                return base
    # gtk-nocsd reexecuta alguns programas pelo carregador dinâmico.
    if "ld-linux" in comm or (args and "ld-linux" in args[0]):
        prog = [a for a in args[1:] if not a.endswith(".so.0") and not a.startswith("-")]
        return "ld.so " + (os.path.basename(prog[0]) if prog else "?")
    return comm


def jload(path):
    """JSON puro ou .json.gz (resultados versionados no repositório)."""
    for p in (path, path + ".gz"):
        if os.path.exists(p):
            with (gzip.open(p, "rt") if p.endswith(".gz") else open(p)) as f:
                return json.load(f)
    return None


def load_boot(d):
    meta = jload(os.path.join(d, "meta.json"))
    g = os.path.join(d, "guest")
    samples = {}
    for f in os.listdir(g) if os.path.isdir(g) else []:
        m = re.match(r"sample-(\d+)\.json(\.gz)?$", f)
        if m:
            samples[int(m[1])] = jload(os.path.join(g, f"sample-{m[1]}.json"))
    login = jload(os.path.join(g, "login.json")) or {}
    inv = jload(os.path.join(g, "inventory.json")) or {}
    return meta, samples, login, inv


def boot_times(inv):
    out = {}
    txt = (inv.get("systemd_analyze") or {}).get("out", "")
    m = re.search(r"Startup finished in (.*?)= ([\d.]+)(min|s|ms)", txt)
    if m:
        v = float(m[2]) * {"min": 60, "s": 1, "ms": 0.001}[m[3]]
        # "1min 2.3s" aparece como "1min 2.3s": refaz pela soma das partes
        tot = re.search(r"= ((?:[\d.]+(?:min|ms|s)\s*)+)", txt)
        if tot:
            v = sum(float(x) * {"min": 60, "s": 1, "ms": 0.001}[u] for x, u in re.findall(r"([\d.]+)(min|ms|s)", tot[1]))
        out["boot_total_s"] = round(v, 2)
    m = re.search(r"graphical\.target reached after ((?:[\d.]+(?:min|ms|s)\s*)+)", txt)
    if m:
        out["graphical_target_s"] = round(sum(float(x) * {"min": 60, "s": 1, "ms": 0.001}[u]
                                               for x, u in re.findall(r"([\d.]+)(min|ms|s)", m[1])), 2)
    return out


def sample_row(s):
    mi = s["meminfo"]
    t = s["totals"]
    g = lambda k, f="pss": t.get(k, {}).get(f, 0)  # noqa: E731
    cw = s.get("cpu_window", {})
    d = cw.get("delta", {})
    busy = None
    if d:
        tot = sum(d.get(k, 0) for k in ("user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal"))
        busy = 100.0 * (1 - (d.get("idle", 0) + d.get("iowait", 0)) / tot) if tot else None
    sec = cw.get("seconds") or 1
    procs = s["processes"]
    nproc = sum(1 for p in procs if not p.get("kthread"))
    swap_used = mi.get("SwapTotal", 0) - mi.get("SwapFree", 0)
    psi = s.get("pressure", {})
    return {
        "ram_usada": mib(s["used_kib"]),
        # "usada" do free(1)/htop: sem o cache nem as reservas do kernel
        "usada_free": mib(mi["MemTotal"] - mi["MemFree"] - mi.get("Buffers", 0) - mi.get("Cached", 0)
                          - mi.get("SReclaimable", 0)),
        "pss_todos": mib(g("sessao") + g("servidor_grafico") + g("sistema")),
        "pss_sessao": mib(g("sessao")),
        "pss_sessao_x": mib(g("sessao") + g("servidor_grafico")),
        "pss_sistema": mib(g("sistema")),
        "pss_xorg": mib(g("servidor_grafico")),
        "pss_ferramenta": mib(g("ferramenta")),
        "rss_sessao": mib(g("sessao", "rss")),
        "uss_sessao": mib(g("sessao", "uss")),
        "AnonPages": mib(mi.get("AnonPages", 0)),
        "Shmem": mib(mi.get("Shmem", 0)),
        "Cached": mib(mi.get("Cached", 0)),
        "Buffers": mib(mi.get("Buffers", 0)),
        "SReclaimable": mib(mi.get("SReclaimable", 0)),
        "SUnreclaim": mib(mi.get("SUnreclaim", 0)),
        "Slab": mib(mi.get("Slab", 0)),
        "KernelStack": mib(mi.get("KernelStack", 0)),
        "PageTables": mib(mi.get("PageTables", 0)),
        "MemFree": mib(mi.get("MemFree", 0)),
        "MemAvailable": mib(mi.get("MemAvailable", 0)),
        "MemTotal": mib(mi.get("MemTotal", 0)),
        "swap_usada": mib(swap_used),
        "SwapTotal": mib(mi.get("SwapTotal", 0)),
        "zram_mem": mib(sum(z["mem_used_total"] for z in s.get("zram", {}).values()) / 1024),
        "processos": nproc,
        "processos_sessao": g("sessao", "n"),
        "kthreads": sum(1 for p in procs if p.get("kthread")),
        "xwayland": int(bool(s.get("xwayland"))),
        "cpu_ocupada_pct": busy,
        "ctxt_s": d.get("ctxt", 0) / sec if d else None,
        "intr_s": d.get("intr", 0) / sec if d else None,
        "psi_mem_some_avg60": psi.get("memory", {}).get("some", {}).get("avg60"),
        "psi_cpu_some_avg60": psi.get("cpu", {}).get("some", {}).get("avg60"),
        "psi_io_some_avg60": psi.get("io", {}).get("some", {}).get("avg60"),
        "atraso_amostra_s": s.get("sample_actual_t", 0) - s.get("sample_t", 0),
    }


def components(s):
    out = {}
    sec = s.get("cpu_window", {}).get("seconds") or 1
    hz = s.get("cpu_window", {}).get("hz", 100)
    for p in s["processes"]:
        if p.get("kthread") or p["group"] == "ferramenta":
            continue
        k = (p["group"], component(p))
        e = out.setdefault(k, {"n": 0, "pss": 0, "rss": 0, "uss": 0, "cpu_ms_s": 0.0, "ctxt_s": 0.0})
        m = p.get("mem", {})
        e["n"] += 1
        e["pss"] += m.get("Pss", 0)
        e["rss"] += m.get("Rss", 0)
        e["uss"] += m.get("Uss", 0)
        e["cpu_ms_s"] += 1000.0 * p.get("cpu_delta_ticks", 0) / hz / sec
        e["ctxt_s"] += p.get("ctxt_delta", 0) / sec
    return out


def stats(vals):
    vals = [v for v in vals if v is not None]
    if not vals:
        return None
    return statistics.median(vals), min(vals), max(vals), len(vals)


def fmt(v, nd=1):
    if v is None:
        return "N/A"
    if isinstance(v, float):
        return f"{v:.{nd}f}"
    return str(v)


def main(argv):
    root = os.path.abspath(argv[1])
    out = root
    if "--out" in argv:
        out = os.path.abspath(argv[argv.index("--out") + 1])
    os.makedirs(out, exist_ok=True)
    rows, boots, comp = [], [], {}
    for prof in sorted(os.listdir(root)):
        pdir = os.path.join(root, prof)
        if not os.path.isdir(pdir):
            continue
        for b in sorted(os.listdir(pdir)):
            bdir = os.path.join(pdir, b)
            if jload(os.path.join(bdir, "meta.json")) is None or not os.path.isdir(os.path.join(bdir, "guest")):
                continue
            meta, samples, login, inv = load_boot(bdir)
            if meta.get("done") != "ok" or not samples:
                print(f"ignorado (incompleto): {bdir}", file=sys.stderr)
                continue
            sess = meta["session"]
            st = next(iter(samples.values())).get("session_type") or {}
            if st.get("type") != ("x11" if sess == "x11" else "wayland"):
                print(f"ignorado (sessão {st.get('type')} != {sess}): {bdir}", file=sys.stderr)
                continue
            marks = login.get("marks", {})
            br = {"perfil": prof, "sessao": sess, "boot": b, "login_pronto_s": marks.get("ready"),
                  "login_compositor_s": marks.get("compositor"), "login_target_s": marks.get("target"),
                  "t_greeter_host_s": meta.get("t_greeter"), "desktop": st.get("desktop")}
            br.update(boot_times(inv))
            boots.append(br)
            for t, s in sorted(samples.items()):
                # Amostra de outra sessão (alguém entrou/saiu na VM durante a
                # medição): fora do resumo.
                cur = s.get("session_type") or {}
                if cur.get("id") != st.get("id") or cur.get("type") != st.get("type"):
                    print(f"amostra descartada (sessão {cur} != {st}): {bdir} t={t}", file=sys.stderr)
                    continue
                r = {"perfil": prof, "sessao": sess, "boot": b, "t": t}
                r.update(sample_row(s))
                rows.append(r)
                for k, v in components(s).items():
                    comp.setdefault((prof, sess, t, k), []).append(v)
    if not rows:
        print("nenhum boot completo", file=sys.stderr)
        return 1

    cols = list(rows[0].keys())
    with open(os.path.join(out, "amostras.tsv"), "w") as f:
        f.write("\t".join(cols) + "\n")
        for r in rows:
            f.write("\t".join(fmt(r[c], 2) for c in cols) + "\n")
    bcols = ["perfil", "sessao", "boot", "boot_total_s", "graphical_target_s", "t_greeter_host_s",
             "login_compositor_s", "login_target_s", "login_pronto_s", "desktop"]
    with open(os.path.join(out, "boots.tsv"), "w") as f:
        f.write("\t".join(bcols) + "\n")
        for r in boots:
            f.write("\t".join(fmt(r.get(c), 2) for c in bcols) + "\n")

    metrics = [c for c in cols if c not in ("perfil", "sessao", "boot", "t")]
    groups = {}
    for r in rows:
        groups.setdefault((r["perfil"], r["sessao"], r["t"]), []).append(r)
    with open(os.path.join(out, "resumo.tsv"), "w") as f:
        f.write("perfil\tsessao\tt\tmetrica\tmediana\tmin\tmax\tn\n")
        for k in sorted(groups):
            for m in metrics:
                s = stats([r[m] for r in groups[k]])
                if s:
                    f.write(f"{k[0]}\t{k[1]}\t{k[2]}\t{m}\t{s[0]:.2f}\t{s[1]:.2f}\t{s[2]:.2f}\t{s[3]}\n")

    with open(os.path.join(out, "componentes.tsv"), "w") as f:
        f.write("perfil\tsessao\tt\tgrupo\tcomponente\tboots\tprocessos\tpss_mib\tpss_min\tpss_max\trss_mib\tuss_mib\tcpu_ms_por_s\tctxt_por_s\n")
        for k in sorted(comp, key=lambda k: (k[0], k[1], k[2], -statistics.median(v["pss"] for v in comp[k]))):
            v = comp[k]
            pss = [x["pss"] for x in v]
            f.write("\t".join([k[0], k[1], str(k[2]), k[3][0], k[3][1], str(len(v)),
                               fmt(statistics.median(x["n"] for x in v)),
                               fmt(mib(statistics.median(pss))), fmt(mib(min(pss))), fmt(mib(max(pss))),
                               fmt(mib(statistics.median(x["rss"] for x in v))),
                               fmt(mib(statistics.median(x["uss"] for x in v))),
                               fmt(statistics.median(x["cpu_ms_s"] for x in v), 2),
                               fmt(statistics.median(x["ctxt_s"] for x in v), 2)]) + "\n")

    # Markdown
    md = []
    md.append("## Três métricas (MiB; mediana [mín–máx], N boots)\n")
    md.append("| Perfil | Sessão | t | RAM usada (MemTotal−MemAvailable) | PSS todos | PSS sessão | PSS sessão + Xorg | N |")
    md.append("|---|---|---:|---:|---:|---:|---:|---:|")
    for k in sorted(groups, key=lambda k: (k[0], k[1], k[2])):
        g = groups[k]
        cells = []
        for m in ("ram_usada", "pss_todos", "pss_sessao", "pss_sessao_x"):
            s = stats([r[m] for r in g])
            cells.append(f"{s[0]:.0f} [{s[1]:.0f}–{s[2]:.0f}]")
        md.append(f"| {k[0]} | {k[1]} | {k[2]} s | " + " | ".join(cells) + f" | {len(g)} |")
    md.append("\n## Memória do kernel e outras (MiB, mediana [mín–máx])\n")
    extra = ("usada_free", "AnonPages", "Shmem", "Cached", "SReclaimable", "SUnreclaim", "KernelStack", "PageTables",
             "swap_usada", "processos", "processos_sessao", "xwayland", "cpu_ocupada_pct", "ctxt_s", "intr_s",
             "psi_mem_some_avg60", "pss_ferramenta")
    md.append("| Perfil | Sessão | t | " + " | ".join(extra) + " |")
    md.append("|---|---|---:|" + "---:|" * len(extra))
    for k in sorted(groups, key=lambda k: (k[0], k[1], k[2])):
        g = groups[k]
        cells = []
        for m in extra:
            s = stats([r[m] for r in g])
            cells.append("N/A" if not s else f"{s[0]:.1f} [{s[1]:.1f}–{s[2]:.1f}]")
        md.append(f"| {k[0]} | {k[1]} | {k[2]} s | " + " | ".join(cells) + " |")
    md.append("\n## Boot e login (s, mediana [mín–máx])\n")
    md.append("| Perfil | Sessão | boot total (systemd-analyze) | graphical.target | login → compositor | login → big-wm-session.target | login → área de trabalho pronta | N |")
    md.append("|---|---|---:|---:|---:|---:|---:|---:|")
    bg = {}
    for r in boots:
        bg.setdefault((r["perfil"], r["sessao"]), []).append(r)
    for k in sorted(bg):
        cells = []
        for m in ("boot_total_s", "graphical_target_s", "login_compositor_s", "login_target_s", "login_pronto_s"):
            s = stats([r.get(m) for r in bg[k]])
            cells.append("N/A" if not s else f"{s[0]:.1f} [{s[1]:.1f}–{s[2]:.1f}]")
        md.append(f"| {k[0]} | {k[1]} | " + " | ".join(cells) + f" | {len(bg[k])} |")
    with open(os.path.join(out, "resumo.md"), "w") as f:
        f.write("\n".join(md) + "\n")
    print("\n".join(md))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
