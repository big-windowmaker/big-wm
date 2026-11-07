# Decorações e gerenciamento de janelas no Wayland (Steam, Chrome)

Rodada de 2026-11-07. Referência: a sessão X11 (Window Maker). Tudo abaixo
foi reproduzido e conferido no laboratório (wlmaker aninhado num Xvfb,
XWayland, `docs/wayland-test-lab.md`) com o cliente real (Google Chrome
151, Ozone X11 e Wayland, perfis temporários); o A/B com o wlmaker instalado
(pacote 21) mostra o defeito antes da correção.

## Steam: não movia nem redimensionava

**Causa.** A janela principal da Steam (`steamwebhelper`, CEF/Chromium, no
XWayland) desliga as decorações (`_MOTIF_WM_HINTS` decorations = 0) e
desenha a própria barra de título. Arrastar e redimensionar por ela é um
pedido ao gerenciador de janelas, `_NET_WM_MOVERESIZE`, que o wlroots
anuncia em `_NET_SUPPORTED`. O wlmaker não ouvia os eventos
`request_move`/`request_resize` das superfícies XWayland: o pedido sumia. O
mesmo valia para o botão de maximizar do próprio programa
(`_NET_WM_STATE`), tela cheia e `_NET_ACTIVE_WINDOW`. Afeta tudo que
desenha a própria moldura no X11: Steam, Chromium/Electron com moldura
própria, GTK com decoração do cliente, SDL com hit-test.

**Correção (wlmaker, `src/xwl_surface.c`).** `request_move` e
`request_resize` começam o arraste e o redimensionamento do compositor (as
bordas vêm do pedido); `request_maximize`/`request_fullscreen` seguem o
estado pedido; `request_activate` traz e ativa a janela.

**Segundo defeito, junto.** O X achava que toda janela estava em 0,0 (ou
onde o programa pediu por último): menus e listas de uma janela movida,
posicionados em coordenadas da raiz, abriam no lugar antigo (o menu do
Chrome abria deslocado). A janela do toolkit agora avisa quando muda de
posição (`position_changed`: colocada, arrastada, redimensionada pela
esquerda ou pelo topo) e a janela X é configurada na posição real do
conteúdo, também nos pedidos de tamanho e de configuração.

**Testes (Chrome no X11 com moldura própria).** O Chrome não é o CEF da
Steam, mas os dois são Chromium e usam o mesmo caminho do X11 para mover e
redimensionar pela própria moldura (`_NET_WM_MOVERESIZE`); a Steam em si
continua a conferir (abaixo).

| Ação | wlmaker 21 (instalado) | Com a correção |
|---|---|---|
| Arrastar pela faixa das abas | não move (`680x804+64+0` antes e depois) | move (`+64+10` → `+199+100`) |
| Borda direita | — | `1050` → `900` de largura |
| Canto inferior direito | — | `800x1004` → `680x804` |
| Botão maximizar / restaurar | — | `1216x1024+64+0` / volta a `680x804+109+14` |
| Menu ⋮ depois de mover | abre deslocado | abre embaixo do botão (X em `334,135`) |

Calculadora (GTK, decoração do cliente no X11): arrasta pela barra e os
menus abrem no lugar. wmcliphist (dockapp): menu continua certo.

**A conferir na sua máquina:** a Steam de verdade (o laboratório usou o
Chrome): mover, redimensionar pelas bordas, maximizar pelo
botão dela, diálogos e janelas de jogos.

## Chrome: bordas estranhas

**Causa.** O Chrome no Wayland desenha a própria moldura (pede
`client_side` no xdg-decoration) com sombra e margem de redimensionamento:
`set_window_geometry(16, 10, 1018, 962)` num buffer de 1050x1004. O wlmaker
contava a janela pelo buffer: posicionamento, maximizar e as zonas usavam
uma caixa maior que a janela visível; com a moldura do servidor
(`Decoration Mode = EnforceServer`) o Chrome mantém as margens e a moldura
do wlmaker cercava as margens transparentes, com faixas e vãos entre a
moldura e a janela.

**Correção (wlmaker).**

- O tamanho da superfície e da janela é a geometria da janela (xdg), não o
  buffer (`src/toolkit/surface.c`, `src/xdg_toplevel.c`).
- Na moldura do servidor, o conteúdo é recortado na geometria
  (`wlr_scene_subsurface_tree_set_clip`): nenhum vão entre a moldura e a
  janela.
- Janelas nas zonas ou no mosaico recebem os estados *tiled* do xdg-shell:
  GTK e Chrome tiram a sombra e os cantos arredondados e a janela ocupa a
  zona inteira (Chrome: `configure` com 5 estados, geometria 599x1012 =
  a zona); restaurar devolve a sombra.
- `Decoration Mode` é relido com SIGHUP (janelas novas).

**Testes.** Chrome Wayland, `SuggestServer` (o padrão): janela
`1018x962` (antes `1050x1004`), move, redimensiona pela margem da sombra.
`EnforceServer`: barra de título do Window Maker, conteúdo e barra de
redimensionar encostados, sem vão.

**Barra de título clássica no Chrome.** Com `SuggestServer` o Chrome
desenha a moldura dele (escolha do aplicativo, respeitada pelo protocolo).
Para a do Window Maker: clique direito na faixa das abas → "Usar barra de
título e bordas do sistema"; o Chrome passa a pedir a decoração do servidor.
Não há política nem opção de linha de comando do Chrome para isso.

## Instalador centrado embaixo do Dock com abas (X11)

O Dock com abas reserva 112 px embaixo (`_NET_WM_STRUT`), mas o Window
Maker publicava `_NET_WORKAREA` com a tela inteira: só o republicava ao
criar ou apagar áreas de trabalho ou ao mudar os monitores. Programas que
se centram na área de trabalho (Calamares, diálogos Qt e GTK) ficavam com
os botões embaixo do Dock. Correção no wmaker (`src/screen.c`): a área de
trabalho é republicada sempre que as reservas mudam. A/B no Xvfb: `0, 0,
1280, 800` (instalado) → `0, 0, 1280, 688`.

## Limitações conhecidas

- A Steam real não foi aberta no laboratório (a instância do usuário está
  aberta na sessão dele; uma segunda só repassa o pedido).
- Aplicativos libadwaita (o assistente "Instalação do Big WindowMaker" é
  um) desenham a própria moldura e ficaram com o visual do libadwaita, não
  com o tema GTK; aplicativos GTK 4 sem libadwaita podem seguir o tema GTK
  (`gtk-theme-name`). Ver `docs/theme-flicker-investigation.md`.
