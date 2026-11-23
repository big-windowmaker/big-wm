# 11 — Resultados finais

Antes: lote 1, perfil glibc (r57 como publicada). Depois: mesma imagem,
mesma carga, perfil glibc, com os pacotes `big-wm-session` e
`big-wm-wayland` desta rodada (big-wm #55 e #56) instalados no setup.
VM do harness, 4 GiB. Mediana [mín–máx], N=5 + 5 por sessão.

## X11

| MiB | 90 s antes | 90 s depois | Δ | 600 s antes | 600 s depois | Δ |
|---|---:|---:|---:|---:|---:|---:|
| residentes reescritos (mic-tray) | 17,6 | 1,1 | **−16,5** | 17,6 | 1,1 | **−16,5** |
| PSS da sessão | 296,8 [295,6–301,3] | 286,4 [280,8–291,4] | −10,3 | 298,0 [297,6–303,5] | 288,6 [283,0–294,7] | −9,4 |
| PSS da sessão + Xorg | 392,6 [391,3–396,2] | 381,6 [376,5–386,2] | −11,0 | 395,3 [379,7–400,9] | 393,2 [384,5–400,0] | −2,1 |
| RAM usada | 962,9 | 966,7 | +3,7 | 1030,7 | 998,1 | −32,6 |
| wmaker (compositor) | 10,0 | 10,0 | 0 | 10,1 | 10,1 | 0 |

O total da sessão X11 cai menos que o mic-tray porque outros processos
variam entre boots sem relação com a mudança: a 600 s, o nm-applet (+7,5
MiB de mediana; 15–23 MiB entre boots) e o protetor do xsecurelock
(`saver_big_wm`, +5,8: a tela bloqueou antes da amostra em parte dos boots).
O Xorg e a "RAM usada" oscilam ±20–30 MiB entre boots; não servem para
medir 16 MiB.

## Wayland

| MiB | 90 s antes | 90 s depois | Δ | 600 s antes | 600 s depois | Δ |
|---|---:|---:|---:|---:|---:|---:|
| residentes reescritos (mic-tray + kwin-screenshot) | 33,1 | 1,7 | **−31,4** | 33,1 | 1,7 | **−31,4** |
| PSS da sessão | 363,0 [358,4–363,5] | 329,7 [325,3–334,1] | **−33,3** | 367,6 [362,9–368,2] | 334,2 [330,2–338,7] | **−33,3** |
| PSS de todos os processos | 467,6 | 437,8 | −29,8 | 471,9 | 442,1 | −29,8 |
| RAM usada | 942,8 | 924,1 | −18,7 | 1012,7 | 1003,6 | −9,2 |
| wlmaker (compositor) | 24,5 | 24,4 | −0,1 | 27,2 | 27,2 | 0 |

No Wayland as faixas antes/depois do PSS da sessão não se sobrepõem.

## Por frente da missão

| Frente | Resultado | Situação |
|---|---|---|
| Alocadores (7 perfis, 68 boots) | glibc é o menor em todos; nenhum entra | investigado, **rejeitado com dados** ([01](01-comparativo-alocadores.md), [05](05-integracao-alocadores.md)) |
| Residentes Python → C | −16,5 MiB (X11), −31,4 MiB (Wayland); partida 167 → 6 ms no Core 2 Duo | **implementado e testado** (PRs #55, #56) |
| `big-wm-tray` em C | ~11 MiB possíveis no Wayland | proposto (A11), não feito |
| zram / swap / marcas d'água | sem OOM sob pressão no HDD; gargalo de I/O | medido, **sem mudança** ([08](08-zram-swap-memory-pressure.md)) |
| Virt-Manager 2–16 GiB, 4 resoluções | 14/14 células, 21/21 verificações | **testado** ([04](04-validacao-virt-manager.md)) |
| Estabilidade (1 h, 68 boots) | nenhuma queda; wlmaker em patamar | **testado** ([10](10-estabilidade-e-stress-tests.md)) |
| OptiPlex 760 | 21/21 nas duas sessões; −16 MiB residente | **testado** ([09](09-hardware-legado.md)) |
