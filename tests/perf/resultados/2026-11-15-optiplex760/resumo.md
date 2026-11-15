# Resumo das séries (OptiPlex 760, 2026-11-15)

Gerado com `resumo.py` e `processos.py` (neste diretório) a partir dos sample-*.json do bigwm-perf-collect.py, sem os processos do user-1000.slice (sessão SSH do medidor). "depois2-x11" é a série "depois" válida do X11 (docs/performance/10-metodo.md).

## antes-x11 (N=5)
| t (s) | RAM usada (MemTotal−MemAvailable) | PSS todos | PSS sessão | Xorg | CPU % (2 núcleos) | ctx/s | processos sessão |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 30 | 1114.4 [1102.2–1130.5] | 444.7 [439.1–446.5] | 300.4 [295.0–300.6] | 41.8 [41.3–42.3] | 7.3 [7.1–7.5] | 2962.4 [2929.7–2992.6] | 86.0 [86.0–86.0] |
| 90 | 1113.5 [1095.3–1127.2] | 423.9 [418.5–425.5] | 282.3 [277.2–282.6] | 40.0 [39.6–40.7] | 7.4 [7.3–8.8] | 2942.1 [2894.6–3021.9] | 65.0 [65.0–65.0] |
| 300 | 1103.8 [1094.2–1124.1] | 424.1 [418.8–425.5] | 282.3 [277.2–282.7] | 40.0 [39.6–40.7] | 7.2 [7.1–7.5] | 2892.4 [2847.6–2940.2] | 65.0 [65.0–65.0] |
## depois2-x11 (N=5)
| t (s) | RAM usada (MemTotal−MemAvailable) | PSS todos | PSS sessão | Xorg | CPU % (2 núcleos) | ctx/s | processos sessão |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 30 | 1169.4 [1163.5–1179.4] | 436.5 [424.4–443.5] | 300.8 [289.7–300.8] | 35.0 [34.5–42.3] | 3.1 [3.0–3.6] | 1350.5 [1334.8–1391.8] | 86.0 [83.0–86.0] |
| 90 | 1158.7 [1149.0–1165.4] | 414.3 [404.7–421.9] | 282.6 [274.3–282.8] | 32.8 [32.5–39.8] | 3.0 [2.9–3.1] | 1308.4 [1289.8–1335.9] | 65.0 [65.0–65.0] |
| 300 | 1157.6 [1149.1–1162.3] | 414.4 [404.8–421.9] | 282.6 [274.3–282.8] | 32.8 [32.5–39.8] | 3.0 [2.9–3.2] | 1278.0 [1264.8–1317.9] | 65.0 [65.0–65.0] |
## antes-wl (N=5)
| t (s) | RAM usada (MemTotal−MemAvailable) | PSS todos | PSS sessão | Xorg | CPU % (2 núcleos) | ctx/s | processos sessão |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 30 | 1064.0 [1045.0–1083.2] | 458.4 [457.7–461.4] | 353.2 [352.7–357.3] | 0.0 [0.0–0.0] | 2.6 [2.6–2.6] | 878.7 [863.5–908.6] | 100.0 [100.0–100.0] |
| 90 | 1069.7 [1048.1–1096.2] | 442.6 [441.8–446.1] | 337.2 [336.8–341.8] | 0.0 [0.0–0.0] | 2.6 [2.5–2.6] | 823.9 [795.9–839.0] | 82.0 [82.0–82.0] |
| 300 | 1078.3 [1067.8–1110.4] | 443.0 [442.0–446.4] | 337.4 [336.9–342.0] | 0.0 [0.0–0.0] | 2.6 [2.5–2.7] | 849.4 [797.5–867.3] | 82.0 [82.0–82.0] |
## depois-wl (N=5)
| t (s) | RAM usada (MemTotal−MemAvailable) | PSS todos | PSS sessão | Xorg | CPU % (2 núcleos) | ctx/s | processos sessão |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 30 | 1065.2 [1057.4–1091.0] | 459.6 [459.3–460.9] | 353.1 [352.5–353.3] | 0.0 [0.0–0.0] | 2.0 [2.0–2.1] | 607.5 [581.0–623.9] | 100.0 [100.0–100.0] |
| 90 | 1070.0 [1065.4–1094.5] | 443.7 [442.8–445.0] | 337.1 [336.6–337.3] | 0.0 [0.0–0.0] | 2.0 [2.0–2.2] | 562.7 [549.9–578.9] | 82.0 [82.0–82.0] |
| 300 | 1068.0 [1063.4–1096.5] | 443.9 [442.9–445.2] | 337.2 [336.7–337.4] | 0.0 [0.0–0.0] | 2.0 [2.0–2.1] | 562.6 [541.3–584.8] | 82.0 [82.0–82.0] |

## CPU e PSS por processo aos 300 s (ticks de 10 ms na janela de 30 s; n = em quantos dos 5 logins o processo apareceu)

```
antes-x11 t=300, 5 logins: ticks/janela | ctx/s | PSS (MiB) | n | processo   [ticks em cada login onde apareceu]
 183.0   450.1    2.6  5  picom   [182, 182, 183, 183, 184]
  66.0   511.2   40.0  5  Xorg   [65, 65, 66, 66, 66]
  22.0    39.5    1.6  5  wmusic   [22, 21, 22, 21, 22]
   6.0   160.1    0.0  4  kworker/u9:2+i915_flip   [6, 6, 7, 6]
   6.0     0.4    9.2  5  python3   [6, 6, 5, 6, 6]
   6.0   163.4    0.0  1  kworker/u9:2-i915_flip   [6]
   5.0    17.1   10.9  5  systemd   [6, 5, 5, 6, 5]
   4.0    70.1    2.7  5  dbus-broker   [4, 4, 5, 4, 5]
   3.0     3.7   10.4  5  xdg-desktop-por   [3, 2, 1, 3, 3]
   3.0    10.4    0.4  5  wmnet   [4, 3, 3, 3, 3]
   2.0     4.4    1.1  5  xdg-document-po   [2, 2, 1, 2, 2]
   2.0     3.5    4.8  5  polkitd   [2, 1, 2, 2, 2]
   2.0     4.2    2.1  5  systemd-logind   [2, 1, 1, 2, 2]
   1.0     8.0    0.5  5  wmcpuload   [2, 1, 1, 0, 0]
   1.0    16.1    0.5  5  wmeyes   [1, 1, 1, 2, 1]
```

```
depois2-x11 t=300, 5 logins: ticks/janela | ctx/s | PSS (MiB) | n | processo   [ticks em cada login onde apareceu]
  46.0   208.5    2.7  5  picom   [47, 45, 45, 47, 46]
  28.0   228.8   32.8  5  Xorg   [28, 28, 28, 29, 29]
   5.0     0.1    9.2  5  python3   [5, 6, 5, 5, 6]
   3.0    10.2    0.4  5  wmnet   [3, 3, 3, 4, 3]
   2.0    34.9    0.0  3  kworker/u9:2-i915_flip   [2, 2, 2]
   2.0    12.2   11.0  5  systemd   [2, 2, 3, 2, 2]
   1.5    37.4    0.0  2  kworker/u9:2+i915_flip   [2, 1]
   1.0     0.6    8.5  5  systemd-journal   [1, 1, 0, 1, 1]
   1.0    16.0    0.4  5  wmeyes   [1, 1, 0, 1, 2]
   1.0     4.2    2.7  5  dbus-broker   [1, 1, 0, 0, 1]
   1.0     1.0    1.0  5  systemd-swap   [1, 0, 1, 1, 0]
   1.0     0.7   16.6  5  wireplumber   [0, 0, 1, 1, 1]
   1.0     9.8    0.0  1  kworker/u8:2-events_unbound   [1]
   1.0    11.7    0.0  1  kworker/1:1H-events_highpri   [1]
   1.0     7.3    0.0  1  kworker/0:1-events   [1]
```

```
antes-wl t=300, 5 logins: ticks/janela | ctx/s | PSS (MiB) | n | processo   [ticks em cada login onde apareceu]
  22.0    36.7    1.6  5  wmusic   [21, 24, 22, 22, 21]
  12.0    30.4   25.0  5  wlmaker   [12, 12, 12, 11, 12]
   6.0     0.3    8.9  5  python3   [7, 7, 6, 6, 6]
   5.0    37.7   21.1  5  Xwayland   [6, 5, 5, 5, 5]
   3.0    10.1    0.4  5  wmnet   [3, 3, 3, 2, 3]
   3.0    62.5    2.9  5  dbus-broker   [3, 5, 4, 3, 3]
   3.0     3.5   12.8  5  xdg-desktop-por   [1, 3, 3, 2, 3]
   2.0    30.2    0.0  4  kworker/u9:2-i915_flip   [2, 1, 2, 2]
   2.0     3.3   29.9  5  big-wm-tray   [2, 2, 2, 2, 2]
   2.0     4.1    1.1  5  xdg-document-po   [1, 2, 2, 2, 1]
   2.0    12.3   11.2  5  systemd   [2, 2, 2, 3, 3]
   1.0    19.6    0.0  5  psimon   [0, 1, 1, 1, 1]
   1.0     7.5    0.5  5  wmcpuload   [1, 1, 1, 1, 0]
   1.0     0.9   16.4  5  big-wm-mic-tray   [0, 0, 1, 1, 1]
   1.0     2.3   11.5  5  smbd   [0, 1, 0, 1, 1]
```

```
depois-wl t=300, 5 logins: ticks/janela | ctx/s | PSS (MiB) | n | processo   [ticks em cada login onde apareceu]
  12.0    30.7   25.0  5  wlmaker   [12, 12, 12, 11, 11]
   6.0     0.1    8.9  5  python3   [6, 6, 6, 6, 7]
   5.0    36.6   21.0  5  Xwayland   [4, 5, 5, 4, 5]
   3.0    12.3   11.2  5  systemd   [2, 3, 3, 3, 3]
   3.0    10.1    0.4  5  wmnet   [3, 3, 3, 2, 2]
   2.0    28.9    0.0  5  kworker/u9:2-i915_flip   [2, 2, 2, 1, 1]
   1.0    15.1    0.0  1  kworker/u8:5-events_power_efficient   [1]
   1.0     7.5    0.5  5  wmcpuload   [1, 1, 0, 1, 1]
   1.0     0.7   17.0  5  wireplumber   [1, 0, 0, 1, 1]
   1.0    40.2    0.0  5  rcu_preempt   [1, 1, 1, 1, 1]
   1.0    19.8    0.0  5  psimon   [1, 1, 1, 1, 1]
   1.0     1.0    1.0  5  systemd-swap   [2, 1, 1, 1, 0]
   1.0     0.6   17.7  5  systemd-journal   [0, 1, 1, 0, 1]
   1.0     8.6    0.0  1  kworker/0:2-mm_percpu_wq   [1]
   0.5     4.6    0.0  2  kworker/u8:0-btrfs-endio-write   [0, 1]
```
