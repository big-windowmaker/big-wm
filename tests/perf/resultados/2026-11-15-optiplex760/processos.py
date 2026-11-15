#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""processos.py DIR SÉRIE T [N] -> CPU, trocas de contexto e PSS por processo
(mediana dos logins onde o processo apareceu; n = em quantos), sem o
user-1000.slice."""
import json,glob,statistics as st,collections,sys
base,tag,t=sys.argv[1],sys.argv[2],sys.argv[3]
acc=collections.defaultdict(list); ctx=collections.defaultdict(list); pss=collections.defaultdict(list)
fs=sorted(glob.glob(f'{base}/{tag}-*/sample-{t}.json'))
for f in fs:
    d=json.load(open(f)); w=d['cpu_window']['seconds']
    c=collections.Counter(); x=collections.Counter(); m=collections.Counter()
    for p in d['processes']:
        if 'user-1000.slice' in p['cgroup']: continue
        c[p['comm']]+=p.get('cpu_delta_ticks') or 0; x[p['comm']]+=p.get('ctxt_delta') or 0; m[p['comm']]+=(p.get('mem') or {}).get('Pss',0)/1024
    for k in set(c)|set(x)|set(m): acc[k].append(c[k]); ctx[k].append(x[k]/w); pss[k].append(m[k])
print(f"{tag} t={t}, {len(fs)} logins: ticks/janela | ctx/s | PSS (MiB) | n | processo   [ticks em cada login onde apareceu]")
for k in sorted(acc,key=lambda k:-st.median(acc[k]))[:int(sys.argv[4]) if len(sys.argv)>4 else 14]:
    print(f"{st.median(acc[k]):6.1f} {st.median(ctx[k]):7.1f} {st.median(pss[k]):6.1f} {len(acc[k]):2d}  {k}   {acc[k]}")
