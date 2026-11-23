# 00 — Auditoria inicial (2026-11-23)

Ponto de partida da rodada de memória, alocadores e estabilidade. Medições
da imagem r57 instalada (VM do harness, 4 GiB, 2 vCPUs, 1920x1080; método em
[02](02-metodologia-benchmark.md)) e do OptiPlex 760 (Core 2 Duo E8400,
3,7 GiB, G41 Gen4).

## Onde a memória está

Sessão aberta, 600 s após o login, perfil glibc (boot 1 de 5 de cada sessão;
as medianas dos 5 estão em [03](03-baseline-x11-wayland.md)).

| MiB | X11 | Wayland |
|---|---:|---:|
| RAM usada (MemTotal − MemAvailable) | 1058 | 1025 |
| PSS dos processos da sessão (usuário) | 298 | 364 |
| Xorg (X11) / wlmaker + Xwayland (Wayland) | 96,8 | 27,4 + 18,9 |
| PSS do sistema (root, serviços) | 94 | 94 |

A RAM usada inclui ~465 MiB de reserva do kernel (marcas d'água do
`auto-rules-io`, decisão A1 em
[../../performance-decisions.md](../../performance-decisions.md): medida e
mantida). Os processos do usuário são a maior parte do resto.

### Maiores residentes da sessão (PSS, MiB)

| Processo | X11 | Wayland | De quem |
|---|---:|---:|---|
| indicator-sound-switcher (Python) | 31,2 | 30,3 | terceiros; mantido (A14) |
| big-wm-tray (Python + GTK) | — | 29,7 | big-wm (A11) |
| wlmaker | — | 27,4 | big-wm (fork) |
| nm-applet | 22,8 | 14,0 | terceiros |
| wireplumber | 20,4 | 21,2 | sistema de áudio |
| big-wm-tabbed-dock (C) | 19,8 | 11,6 | big-wm |
| big-wm-notifications (C) | 18,2 | 7,3 | big-wm |
| wmcliphist | 9,7 | 18,1 | dockapp |
| **big-wm-mic-tray (Python)** | **17,6** | **16,5** | big-wm → C neste ciclo |
| snixembed | 17,4 | — | terceiros |
| **big-wm-kwin-screenshot (Python)** | — | **16,5** | big-wm → C neste ciclo |
| big-keyboard (C + GTK) | 15,9 | 8,6 | big-wm |
| wmaker --for-real | 10,3 | — | Window Maker |

## Quanto o alocador pode mudar

Só a memória anônima (heap) depende do malloc. Composição do PSS dos
compositores no mesmo boot:

| MiB | PSS | anônima | arquivos | shmem (buffers) |
|---|---:|---:|---:|---:|
| wmaker --for-real | 10,3 | 6,3 | 3,9 | 0,0 |
| wlmaker | 27,4 | 14,3 | 4,4 | 8,6 |
| Xorg (fora do escopo: servidor) | 96,8 | 54,6 | 41,0 | 1,1 |

Teto de ganho de um alocador no compositor: parte de 6,3 MiB (X11) e de
14,3 MiB (Wayland), numa sessão de 300–370 MiB. Já medido antes:
`malloc_trim(0)` no wlmaker devolve 1,1 MiB (rejeitado).

## Sistema de memória do OptiPlex (como vem da r57)

| Item | Valor |
|---|---|
| zram | zstd, 1,7 GiB, prioridade 32767 |
| swap em arquivo | 512 MiB (prioridade menor) |
| zswap | desligado |
| `vm.swappiness` | 30 |
| `vm.min_free_kbytes` | 115101 |
| `vm.watermark_scale_factor` / `watermark_boost_factor` | 150 / 15000 |
| THP | madvise |
| systemd-oomd | inativo |

## Conclusões da auditoria

1. Os maiores ganhos estão em residentes Python que cabem em C sem perder
   função: `big-wm-kwin-screenshot` e `big-wm-mic-tray` (feitos:
   [07](07-otimizacoes-estruturais.md)); `big-wm-tray` fica proposto (A11).
2. O alocador do compositor tem pouco para ganhar (heap pequeno); medir
   mesmo assim, por processo, sem preload global
   ([01](01-comparativo-alocadores.md), [05](05-integracao-alocadores.md)).
3. Reserva do kernel e zram: já decididos (A1); só remedir
   ([08](08-zram-swap-memory-pressure.md)).
