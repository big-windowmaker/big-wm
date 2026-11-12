# Menus e popups no Wayland — testes de regressão

## Automático

`tests/integration/wayland-popups.sh` (carregado pelo `wayland.sh`, com o
wlmaker no ar): janela GTK 3 de teste (`popup-menu.py`, fundo `#303030`,
itens magenta, submenu "Formatar para"), ponteiro virtual
(`wl-pointer.py`), menu achado na captura (`grim` + ImageMagick). Para os
backends `wayland` e `x11` (XWayland):

| Caso | Critério |
|---|---|
| janela nova, clique direito | o menu abre a 0–6 px do ponteiro |
| janela arrastada pela barra de título do Window Maker | idem |
| perto da borda de baixo da saída | o menu fica inteiro na saída (inverte) |

Resultado (laboratório, wlmaker aninhado 1280×900): wlmaker 33 **5/6
FALHA** (XWayland, janela arrastada: clique 485,264, menu 731,349);
wlmaker 34 **6/6 PASSA**. Sem `wlr-randr` o teste sai como SKIP.

## Manual (laboratório)

| Aplicativo / caso | Ponteiro | Menu (34) |
|---|---|---|
| Thunar X11 | 765,550 | 765,550 |
| Thunar Wayland | 765,550 | 762,548 |
| Kate (Qt Wayland) | 515,450 | 516,452 |
| GTK X11, janela movida | 601,450 | 603,452 |
| GTK, canto de baixo | 941,870 | inverte para cima, 943,719 |
| Submenu "Formatar para" | — | 85×75 em 509,327, colado ao menu |
| Janela encaixada numa zona | — | X informado (+71+30), menu no lugar |
| Dois monitores (1024×768 + 1280×800), perto da borda do primeiro | 833,300 | inverte e fica no primeiro, 627,302 (X11 e Wayland) |

## Não verificado

GParted de verdade (precisa de root), saídas com escala fracionária, menu
de Ctrl+clique do xterm (caminho sem pai), sessão real (o wlmaker 34 vale a
partir do próximo login). A sessão X11 (Window Maker) não usa nenhum código
alterado.
