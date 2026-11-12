# 09 — Vários monitores

- O compositor já põe janelas novas na saída do ponteiro (verificado: a
  janela do Spectacle abriu na X11-2 com o ponteiro lá, e na X11-1 depois).
- "Tela atual" precisa do ponteiro no momento do pedido: o wlmaker 33 tem
  `wmake-zones pointer` (patch 0146: posição e saída do cursor); com o
  wlmaker anterior, a saída da janela ativa; senão, a primeira.
- Escala: a resposta leva a escala da saída (`status` do wmake-zones).

Laboratório (X11-1 1024×768 em 0,0; X11-2 1280×800 em 1024,0):

| | Antes | Depois |
|---|---|---|
| `-f` | 1280×800 (só a X11-2) | 2304×800; cada parte idêntica à sua saída (RMSE 0) |
| `-m`, ponteiro na X11-2 | 2304×800 (tudo) | 1280×800, idêntica à X11-2 |
| `-m`, ponteiro na X11-1 | 2304×800 | 1024×768, idêntica à X11-1 |

Sessão real (três monitores): `-f` 9440×1440 com a disposição real;
`-m` 3440×1440 (o DP-1, da janela ativa: o critério até o próximo login).
