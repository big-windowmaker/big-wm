#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""resumo.py DIR SÉRIE... -> mediana [mín–máx] por tempo de amostra dos
sample-*.json do bigwm-perf-collect.py (DIR/SÉRIE-N/), sem os processos do
user-1000.slice (sessão SSH do medidor), que o coletor põe em "sistema"."""
import json, os, sys, statistics as st, glob
base = sys.argv[1]
def med(v):
    v = [x for x in v if x is not None]
    if not v: return "—"
    return f"{st.median(v):.1f} [{min(v):.1f}–{max(v):.1f}]" if len(v) > 1 else f"{v[0]:.1f}"
def load(tag):
    runs = sorted(glob.glob(f"{base}/{tag}-*"))
    out = {}
    for r in runs:
        for f in glob.glob(f"{r}/sample-*.json"):
            t = int(f.rsplit("-", 1)[1].split(".")[0])
            out.setdefault(t, []).append(json.load(open(f)))
    return out, len(runs)
for tag in sys.argv[2:]:
    data, n = load(tag)
    print(f"## {tag} (N={n})")
    print("| t (s) | RAM usada (MemTotal−MemAvailable) | PSS todos | PSS sessão | Xorg | CPU % (2 núcleos) | ctx/s | processos sessão |")
    print("|---:|---:|---:|---:|---:|---:|---:|---:|")
    for t in sorted(data):
        S = data[t]
        used = [s["used_kib"]/1024 for s in S]
        def tot(s, g):
            # sem a sessão SSH do medidor (user-1000.slice: ruscher), que o coletor põe em "sistema"
            return sum((p.get("mem") or {}).get("Pss", 0) for p in s["processes"]
                       if p["group"] == g and "user-1000.slice" not in p["cgroup"])/1024
        allp = [sum(tot(s, g) for g in ("sistema","servidor_grafico","sessao")) for s in S]
        sess = [tot(s, "sessao") for s in S]
        xorg = [tot(s, "servidor_grafico") for s in S]
        cpu, ctx = [], []
        for s in S:
            d = s["cpu_window"]["delta"]; busy = sum(d[k] for k in ("user","nice","system","irq","softirq","steal"))
            totl = busy + d["idle"] + d["iowait"]
            cpu.append(100*busy/totl if totl else None); ctx.append(d["ctxt"]/s["cpu_window"]["seconds"])
        np_ = [s["totals"]["sessao"]["n"] for s in S]
        print(f"| {t} | {med(used)} | {med(allp)} | {med(sess)} | {med(xorg)} | {med(cpu)} | {med(ctx)} | {med(np_)} |")
