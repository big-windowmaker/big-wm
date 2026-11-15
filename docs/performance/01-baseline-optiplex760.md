# Linha de base: Dell OptiPlex 760

Medido em 2026-11-15, com o sistema como estava (big-wm `eab850e`,
windowmaker 26, wlmaker 40, big-dockapps 14, picom 13). Antes de qualquer
mudança, o maior desperdício era CPU em repouso no X11 (7,2 % dos dois
núcleos), não memória.

## Máquina (conferida por comando)

| Item | Valor |
|---|---|
| CPU | Intel Core 2 Duo E8400, 2 núcleos, 2,0–3,0 GHz; SSE4.1, sem `nonstop_tsc` |
| GPU | Intel 4 Series (G45) `8086:2e12`, driver i915; Mesa 26.2.4 (crocus) |
| X11 | xorg-server 21.1.24 com o driver `intel` (SNA), ajuste local da máquina |
| Wayland | wlmaker 40, wlroots 0.20.2, `WLR_RENDERER=pixman` (ajuste local) |
| Memória | 3,66 GiB; zram 1,7 GiB (zstd) + `/swapfile` 512 MiB; `vm.swappiness=30`, `vm.watermark_scale_factor=150` |
| Disco | **HDD** SAMSUNG HD161HJ (rotacional), btrfs `compress-force=zstd:3,autodefrag`, 6,4 de 149 GiB usados |
| Kernel | 6.18.55-1-MANJARO; relógio pelo HPET ("TSC halts in idle") |
| Monitor | 1920×1080 a 60 Hz, VGA |

## Repouso (5 logins por sessão, amostra aos 300 s, mediana [mín–máx])

| Sessão | CPU (% dos 2 núcleos) | Trocas de contexto/s | PSS da sessão (MiB) | PSS de todos (MiB) | Login → pronto (com cache) |
|---|---:|---:|---:|---:|---:|
| X11 | 7,2 [7,1–7,5] | 2892 [2848–2940] | 282,3 [277,2–282,7] (+40,0 do Xorg) | 424,1 [418,8–425,5] | 5,0 s |
| Wayland | 2,6 [2,5–2,7] | 849 [798–867] | 337,4 [336,9–342,0] | 443,0 [442,0–446,4] | 5,7 s |

Maiores consumidores de CPU no X11 (ticks de 10 ms em 30 s): picom 183,
Xorg 66, wmusic 22. Em memória, na sessão X11: Xorg 40,0, indicator-sound-
switcher 30,9, Dock com Abas 29,0, nm-applet 23,5, snixembed 18,1,
big-wm-mic-tray 17,3, big-keyboard 16,8 (MiB de PSS).

## Boot (3 reboots, mediana)

| Marco | s desde o boot |
|---|---:|
| `big-driver-manager-gpu` termina | 20,0 |
| wdm iniciado (tela de login a caminho) | 20,1 |
| Xorg da tela de login | 20,7 |
| `graphical.target` | 27,6 |
| Login a frio → área pronta | 8,7 s depois do Enter |

## Estado geral

Nenhuma unidade com falha (sistema e sessão). Em repouso, 2,7 GiB
disponíveis, zram vazia, sem pressão de memória (PSI ~0). Avisos do
journal: os do xkbcomp (FK23/FK24) a cada X iniciado. O Xorg com SNA
registrou 3 travamentos da GPU num boot de 12 h, recuperados por reset.
