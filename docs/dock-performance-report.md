# Clip em grade — desempenho

Medido em 2026-11-23 no laboratório aninhado (`perf.sh`: Xvfb 1280×900,
wlmaker do X11 backend com pixman, XWayland, 4 dockapps), build de
depuração, mesma sequência para `-7` (antes) e `-8` (depois).

| Medida | Antes (-7) | Depois (-8) |
|---|---|---|
| wlmaker, CPU em 30 s ocioso | 0,19 s | 0,19 s |
| wlmaker RSS / PSS ocioso | 42,6 MB / 20,1 MB | 42,6 MB / 20,2 MB |
| `wlmdock --clip` CPU ocioso / RSS | 0 / 20,4 MB | 0 / 20,4 MB |
| wlmaker, CPU em 20 arrastes (51,6 s) | 0,48 s | 0,81 s |
| RSS após 20 arrastes | 42,9 MB | 42,9 MB |

Leitura: nada muda em repouso (sem polling nem timers novos; nenhum processo
novo). Cada soltura custa ~17 ms a mais de CPU no compositor: o
congelamento grava as células e o `State.plist` é relido ao rearranjar.
Isso acontece só ao soltar, nunca durante o movimento do ponteiro (o
encaixe é uma busca em memória nos ícones do Clip). Memória estável depois
dos ciclos.

Na máquina real (OptiPlex 760, Gen4 com pixman), mesma sessão Wayland com
12 dockapps, 90 s após o login, 60 s ocioso:

| | wlmaker CPU | Xwayland CPU | RSS | PSS |
|---|---|---|---|---|
| `-7` | 0,21 s | 0,09 s | 43,9 MB | 25,4 MB |
| `-8` (3 dockapps em faixas) | 0,23 s / 0,22 s (2 rodadas) | 0,10 s | 44,1 MB | 25,5 MB |

Escrita em disco: uma gravação atômica por soltura no Clip (duas quando um
estado antigo é congelado pela primeira vez); nenhuma em repouso.

Instrumentação: build ASan+UBSan; testes `clip_grid` e `dockapps` sem
erros nem vazamentos; cenário gráfico completo sem erros de memória no
wlmaker. Vazamentos encontrados apenas em caches do cairo/fontconfig
(conhecidos); o do `wlmdock --clip` na saída (lançadores da grade não
destruídos) foi corrigido em 0179 — resta o mesmo resíduo de 1,1 kB do Dock,
anterior a esta rodada.
