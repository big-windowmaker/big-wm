# 03 — Linha de base e alocadores, X11 e Wayland

Lote 1: 50 boots (5 perfis × 2 sessões × 5 rodadas), método em
[02](02-metodologia-benchmark.md). Lote 2 (snmalloc, Scudo) em
[01](01-comparativo-alocadores.md). Mediana [mín–máx], N=5 por célula.

Conferido em todos os 50 boots (`/proc/PID/maps` gravado pela carga): nos
perfis com biblioteca, o compositor carregou a do seu perfil; no perfil
glibc, nenhuma biblioteca de alocador aparecia no mapa. O compositor
manteve o mesmo PID de 30 a 600 s (nenhuma queda); as 50 cargas terminaram
(2000 ciclos de janela).

Carga no X11: 1,4–2,2 % dos ciclos dos lotes (22 de 1000 no lote 1) e
até 7 % nas sessões de 1 h terminaram em menos de 1,5 s, ou seja, sem a
janela ficar 2 s aberta (provável instância anterior do gnome-calculator
ainda no D-Bus; o código de saída não era gravado). Espalhados entre todos
os perfis, inclusive o glibc; no Wayland, nenhum. O script grava o código
de saída desde então.

## Linha de base (glibc)

| MiB | X11 30 s | X11 600 s | Wayland 30 s | Wayland 600 s |
|---|---:|---:|---:|---:|
| RAM usada (MemTotal − MemAvailable) | 961 [949–969] | 1031 [1022–1058] | 945 [940–953] | 1013 [989–1025] |
| PSS de todos os processos | 507 [506–514] | 489 [474–495] | 473 [469–474] | 462 [457–462] |
| PSS da sessão (usuário) | 315 [314–319] | 298 [298–303] | 379 [374–380] | 368 [363–368] |
| PSS da sessão + Xorg | 411 [410–414] | 395 [380–401] | 379 | 368 |

(VM de 4 GiB; a RAM usada inclui ~465 MiB de reserva do kernel, A1. As
imagens já têm o mic-tray em Python: os números de depois dos portes em C
estão em [11](11-resultados-finais.md).)

## Compositor por alocador

PSS / memória anônima / USS (MiB) e CPU total do compositor até 600 s.

### X11 — `wmaker --for-real`

| Perfil | PSS 90 s | PSS 600 s | anônima 600 s | USS 600 s | CPU até 600 s (ms) |
|---|---:|---:|---:|---:|---:|
| glibc | 10,0 [10,0–10,2] | 10,1 [10,1–10,3] | 6,3 [6,3–6,3] | 7,6 [7,6–7,8] | 710 [630–800] |
| glibc-a2 | 10,1 [9,9–10,2] | 10,2 [10,0–10,2] | 6,3 [6,3–6,3] | 7,7 [7,5–7,8] | 720 [580–760] |
| jemalloc | 10,9 [10,7–10,9] | 11,0 [10,9–11,1] | 6,9 [6,8–6,9] | 8,4 [8,2–8,4] | 770 [570–780] |
| tcmalloc | 14,4 [14,3–14,4] | 14,4 [14,4–14,5] | 10,7 [10,7–10,7] | 12,0 [11,9–12,1] | 770 [710–800] |
| mimalloc | 25,8 [23,8–25,8] | 23,4 [21,7–25,7] | 19,5 [17,8–21,9] | 20,9 [19,1–23,3] | 790 [690–890] |

### Wayland — `wlmaker`

| Perfil | PSS 90 s | PSS 600 s | anônima 600 s | USS 600 s | CPU até 600 s (ms) |
|---|---:|---:|---:|---:|---:|
| glibc | 24,5 [24,4–24,6] | 27,2 [27,2–27,4] | 14,3 [14,2–14,3] | 24,4 [24,3–24,5] | 4600 [4010–5300] |
| glibc-a2 | 24,5 [24,4–24,6] | 27,3 [27,3–27,4] | 14,4 [14,3–14,4] | 24,4 [24,4–24,5] | 4680 [4110–5190] |
| jemalloc | 25,5 [25,4–25,5] | 28,2 [28,1–28,4] | 14,8 [14,5–14,8] | 25,4 [25,3–25,5] | 4620 [4090–4810] |
| tcmalloc | 28,2 [28,2–28,3] | 31,1 [30,9–31,2] | 18,1 [17,8–18,1] | 28,2 [28,0–28,4] | 4270 [3720–4640] |
| mimalloc | 35,6 [35,5–35,7] | 39,5 [39,5–39,7] | 26,4 [26,4–26,4] | 36,7 [36,7–36,8] | 4830 [3940–5110] |

## Leitura

- **glibc (com ou sem `arena_max=2`, que empata) é o menor em memória nos
  dois compositores**, em todos os instantes; as faixas dos alocadores
  alternativos não se sobrepõem às dela. Diferença ao
  glibc no PSS a 600 s: jemalloc +0,9/+1,0 MiB, tcmalloc +4,3/+3,9,
  mimalloc +13,3/+12,3 (X11/Wayland).
- `arena_max=2`: igual ao glibc (compositores de uma thread).
- CPU: nenhuma diferença fora da dispersão (as faixas se sobrepõem em
  todos os perfis). O tcmalloc tem a menor mediana no Wayland (4270 contra
  4600 ms) e o menor mínimo (3720 contra 4010 ms), mas as faixas se
  sobrepõem e a diferença está dentro da dispersão de 5 boots; não passa no
  critério de [02](02-metodologia-benchmark.md) e ele custa +3,9 MiB.
- Depois da carga (300 s → 600 s) nenhum perfil cresce: o heap volta ao
  mesmo patamar; não há sinal de vazamento no compositor com nenhum deles.
- O total da sessão X11 varia ±20 MiB entre boots pelo Xorg (pixmaps das
  janelas da carga), mais do que qualquer diferença de alocador no wmaker;
  por isso a decisão usa o compositor, não o total.

Observação: no X11, os perfis com wrapper substituíram o `LD_PRELOAD` da
sessão (gtk-nocsd) no wmaker ([06](06-riscos-compatibilidade.md)); só o
perfil glibc manteve essa biblioteca pequena mapeada. O viés favorece as
alternativas, e mesmo assim nenhuma ganhou.

Observação: durante as rodadas 3–5 a VM do Virt-Manager rodava no mesmo
host ([04](04-validacao-virt-manager.md)); como a ordem dos perfis gira,
todos os perfis dividiram esse período. Memória por VM não é afetada.
