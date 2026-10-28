#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-cycles.py - analisa os ciclos de login/logout de
# "bigwm-perf-vm.py cycles" (DIR/guest/c<N>-{0-greeter,1-sessao,2-pos-logout}.json).
#
#   bigwm-perf-cycles.py DIR...
#
# Para cada ciclo mostra:
#   - processos do usuário na sessão (90 s depois do login), PSS da sessão;
#   - duplicados: componente com mais instâncias que no 1º ciclo;
#   - órfãos: processos do usuário (UID) ou na user.slice dele que ainda
#     existem 20 s depois do logout;
#   - processos do sistema que surgiram depois do 1º login e continuam vivos.
import collections
import gzip
import json
import os
import sys


def comp(p):
    args = p["cmd"].split()
    c = p["comm"]
    if c.startswith("python") or c in ("bash", "sh", "perl"):
        for a in args[1:]:
            if not a.startswith("-"):
                return os.path.basename(a)
    return c


def load(path):
    for p in (path, path + ".gz"):
        if os.path.exists(p):
            with (gzip.open(p, "rt") if p.endswith(".gz") else open(p)) as f:
                return json.load(f)
    return None


def main(dirs):
    for d in dirs:
        g = os.path.join(d, "guest")
        print(f"## {d}")
        first = None
        base_sys = None
        n = 1
        while load(os.path.join(g, f"c{n}-1-sessao.json")) is not None:
            s = load(os.path.join(g, f"c{n}-1-sessao.json"))
            post = load(os.path.join(g, f"c{n}-2-pos-logout.json"))
            uid = s["uid"]
            sess = [p for p in s["processes"] if p["group"] == "sessao"]
            cnt = collections.Counter(comp(p) for p in sess)
            pss = sum(p.get("mem", {}).get("Pss", 0) for p in sess) / 1024
            line = f"ciclo {n}: {len(sess)} processos na sessão, PSS sessão {pss:.1f} MiB"
            sysp = {comp(p) for p in s["processes"] if p["group"] == "sistema"}
            if first is None:
                first, base_sys = cnt, sysp
            else:
                dup = {k: v for k, v in cnt.items() if v > first.get(k, 0)}
                gone = {k: v for k, v in first.items() if cnt.get(k, 0) < v}
                line += f"; a mais que no ciclo 1: {dup or 'nada'}; a menos: {gone or 'nada'}"
                newsys = sorted(sysp - base_sys)
                if newsys:
                    line += f"; sistema novo: {newsys}"
            print(line)
            if post:
                orf = [p for p in post["processes"]
                       if (p["uid"] == uid or f"/user-{uid}.slice/" in p["cgroup"]) and p["group"] != "ferramenta"]
                desc = [f"{comp(p)} ({p['cgroup'].rsplit('/', 1)[-1]})" for p in orf]
                print(f"  20 s após o logout: {len(orf)} processo(s) do usuário: {desc or 'nenhum'}")
            n += 1
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
