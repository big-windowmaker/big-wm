# Cenários de uso: OptiPlex 760, 2026-11-15

Sessão X11 depois das correções, com picom, dockapps e o tema padrão.
Uma série por cenário (não é antes/depois: serve para saber se a máquina
aguenta o uso cotidiano e onde está o limite). Scripts e método em
[10-metodo.md](10-metodo.md).

## B: abrir programas (s até a janela entrar na lista do Window Maker)

| Programa | Frio (sem cache) | Com cache | Com cache |
|---|---:|---:|---:|
| Thunar | 8,8 | 0,6 | 0,6 |
| AshyTerm | 11,0 | 1,2 | 1,2 |
| Calculadora (gnome-calculator) | 7,0 | 0,9 | 0,9 |
| Kate | 7,9 | 0,7 | 0,7 |
| Firefox (perfil novo de teste) | 16,2 | 2,0 | 2,1 |

O frio é o disco rígido (cache esvaziado com `drop_caches`); com cache,
tudo abre em até 2 s. Armadilhas da medição: o Kate se separa do terminal
(numa unidade do systemd o processo principal sai e leva o resto junto:
`kate -n -b`), e Kate e Firefox abertos antes só recebem o pedido e saem
com 0, sem janela nova.

## C: tudo aberto

Thunar, AshyTerm, Calculadora e Firefox abertos juntos: +551 MiB usados
(1044 → 1595 MiB de 3746), swap sem mudança (149 MiB), PSI de memória
(some, avg10) 9,6 % logo depois de abrir, PSI de E/S 29,6 % (o Firefox
lendo do disco). Um comando curto (`python3 -c pass`) continua em 19–22 ms
(52 ms na primeira vez).

## D: pressão de memória

`tests/perf/bigwm-perf-pressure.py`: aloca até MemTotal − 600 MiB de
memória incompressível e lê 1 GiB do disco em laço, abrindo a calculadora
a cada 15 s. Com os ajustes atuais (zram zstd 1,7 GiB + swapfile 512 MiB,
`swappiness=30`, `watermark_scale_factor=150`):

- antes da pressão: calculadora em 0,6–1,7 s, comando curto em 19–21 ms;
- no pico: a primeira abertura levou 56 s (PSI de E/S ~80 %: o disco
  rígido relendo as bibliotecas que saíram do cache); as seguintes,
  0,7–1,1 s; o comando curto, ~20 ms;
- o OOM killer escolheu `indicator-sound-switcher` e `nm-applet`
  (`oom_score_adj` 200 dos autostarts), não o processo de pressão: efeito
  de um teste que segura a memória de propósito, não de uso real.

A decisão A1 ([../performance-decisions.md](../performance-decisions.md))
continua valendo: o desktop volta a responder, mas o primeiro programa
depois de um pico espera o disco.

## E: mover janelas

200 movimentos da janela do Thunar com `xdotool windowmove` em 0,7 s:
Xorg 22 ticks, wmaker 0, picom 1. Mover janelas não é gargalo.

## F: sair da sessão

Dois logouts reais (wdm), um com o compositor desligado e outro ligado:
10 s depois, nenhum processo do usuário (nem o `systemd --user`). Nada
fica para trás.

## Efeitos visuais (compositor) desligados

Menu Configurações > Efeitos visuais (`big-wm-compositor`), na máquina
real. CPU da máquina inteira em repouso, ticks em 3 janelas de 30 s, com
os dockapps animados:

| Estado | Ticks/30 s |
|---|---|
| Ligado (antes) | 74, 26, 60 |
| Desligado (`--disable`, na hora) | 26, 16, 16 |
| Desligado, depois de sair e entrar | 20, 20, 20 |
| Religado (`--toggle`) | 95, 87, 88 |

O picom e o `big-wm-x11-repaint` param na hora e não voltam no login
seguinte; ao religar, voltam os dois, sem sair da sessão.
