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

## Moldura do Window Maker em todas as janelas (novembro de 2026)

O usuário quer a moldura do Window Maker em todo programa, também nos que
desenham a própria e pedem "sem moldura".

- **Wayland (wlmaker):** atributo de janela `ServerDecoration` no
  `WindowAttributes` do `Config.plist` (chave: app ID, ou instância/classe
  do WM_CLASS; `"*"` para todos). `Yes` emoldura mesmo um cliente que
  negociou decoração própria (xdg-decoration `client_side`), que não fala
  protocolo nenhum (GTK 4/libadwaita) ou que desligou as decorações no X11
  (`_MOTIF_WM_HINTS`, Steam); `No` nunca emoldura. O padrão do Big
  WindowMaker é `"*" = { ServerDecoration = Yes; }`. Um cliente emoldurado
  contra a vontade recebe os estados *tiled* do xdg-shell nas quatro
  bordas: GTK e Chrome tiram sombra, cantos arredondados e margens de
  redimensionar. Ficam sem moldura: janelas override-redirect, menus,
  dicas, avisos, telas de abertura, janelas utilitárias e de ferramentas,
  docks, desktops, ícones de arrastar e listas (tipos EWMH), e painéis
  layer-shell (Dock com abas, bandeja, dockapps).
- **X11 (Window Maker):** `IgnoreDecorationChanges = YES` no `"*"` do
  `WMWindowAttributes`. O wmaker foi corrigido para aplicar esse atributo só
  aos tipos decorados (normal, diálogo): antes ele dava barra de título
  também a docks, menus e telas de abertura.
- **Jogos em "tela cheia sem bordas"** (Wine/Proton, SDL: sem
  decorações, do tamanho do monitor, sem o estado de tela cheia) não
  ganham moldura nas duas sessões, e no Wayland ficam na origem do
  monitor (não ao lado do Clip): wlmaker `1cd5ee9` (dica de posicionamento
  `exact`, com teste unitário) e wmaker `9431c6d8`
  (`wNETWMCoversItsHead`). Testado no laboratório: janela sem decorações
  de 1280x900 numa tela de 1280x900 abre em 0,0 sem moldura; uma de 500x300
  ganha a moldura. Jogos que pedem tela cheia (`_NET_WM_STATE_FULLSCREEN`,
  xdg fullscreen) já ficavam sem moldura.
- **Botões duplicados:** o `big-wm-theme` esvazia uma vez por conta o
  `button-layout` (gsettings, lido pelo portal: GTK 4, libadwaita,
  Telegram, Chrome), o `gtk-decoration-layout` (settings.ini) e o
  `Gtk/DecorationLayout` (XSETTINGS, X11): os programas tiram os próprios
  botões de janela e ficam os da barra do Window Maker. A barra própria da
  Steam (CEF) não tem opção para sair e fica dentro da moldura.
- Contas existentes: `big-wm-firstrun` acrescenta os dois atributos uma vez
  (marcas em `~/.config/big-wm/migrations/`); apagado pelo usuário, não
  volta. Chrome, Chromium e Brave ganham uma vez por perfil "Usar barra de
  título e bordas do sistema" (com o navegador fechado e sem escolha
  anterior).

## Steam: conteúdo some ao arrastar (com a moldura)

**Sintoma.** Com a moldura do Window Maker, arrastar a Steam deixava a
moldura vazia ("transparente"); o conteúdo só voltava reabrindo a Steam.

**Investigação.** No laboratório sem GPU (Xvfb, pixman, CEF sem GPU) o
arraste funcionava: a falha dependia do caminho com GPU. Teste real com a
Steam do usuário e GPU (AMD Radeon RX 9060 XT, GLES2): wlmaker aninhado
com o backend `headless` e `WLR_RENDER_DRM_DEVICE=/dev/dri/renderD128`
(independe da tela do usuário e do bloqueio de tela), XWayland com glamor,
e um cliente de ponteiro virtual (`wlr-virtual-pointer-unstable-v1`, que o
wlmaker já suporta) para arrastar de verdade pela barra de título. Captura
com `grim` no compositor aninhado. Na sessão real o defeito também foi
visto: depois de um arraste a janela X estava em `+547+508` e a moldura em
`+415+325` — o conteúdo era desenhado onde o X achava que a janela estava,
fora da moldura.

**Causa.** `_xwl_surface_handle_set_geometry` (wlmaker, `src/xwl_surface.c`)
punha o elemento do conteúdo na posição da janela X. Para menus e popups
isso é certo (posição relativa ao pai); para uma janela numa moldura é um
deslocamento dentro da moldura. Desde que o X passou a saber a posição real
da janela (`aa91c3e`, para os menus abrirem no lugar), um ConfigureNotify
atrasado (o X responde a cada configure de um arraste com atraso; com a GPU
a Steam responde mais tarde) trazia uma posição antiga diferente da atual: o
wlroots emitia `set_geometry`, o conteúdo saía da moldura pela posição da
janela na tela, e o próximo movimento lia essa posição e mandava ao X:
o erro crescia. O upstream tem o mesmo código, mas lá o X nunca sabia a
posição (janelas em 0,0), e o defeito não aparecia.

**Correção (wlmaker `9d23d01`).** O conteúdo de uma janela fica sempre na
origem da moldura; se o X informar outra posição, recebe de novo a real.
Superfícies sem janela (menus, popups) continuam como antes.

**Menus (wlmaker 34, patch 0147).** O `aa91c3e` deixou incompleto o caso dos
menus: um menu com pai ficava na sua posição de raiz menos a do pai **e**
menos a sua posição anterior, e ia longe embaixo à direita (o menu do
GParted). Corrigido junto com a sombra e a borda da saída dos `xdg_popup`;
ver [wayland-popup-positioning-audit.md](wayland-popup-positioning-audit.md)
e [wayland-popup-regression-tests.md](wayland-popup-regression-tests.md).

**Testes (Steam real, GPU).**

| Teste | wlmaker 24 (instalado) | Com a correção |
|---|---|---|
| Encaixar numa zona (`wmake-zones snap 1`) | moldura vazia, conteúdo fora | conteúdo na moldura |
| 12 arrastes pela barra (ponteiro virtual) | — | X sempre em moldura + (1, 24): 0 divergências |
| Maximizar pelo botão da Steam | — | conteúdo preenche a moldura, X 1214x687 em 65,24 |
| Menu da conta (popup do CEF) | — | abre embaixo do botão |
| Chrome X11: arrastar e menu ⋮ | — | menu embaixo do botão (sem regressão) |
| GTK no X11 (Calculadora) | — | moldura, sem botões duplicados |

Pendente na máquina real: suspensão e retomada, mudança de resolução com a
Steam aberta, sobreposição (overlay) dentro de jogos.

## Limitações conhecidas

- A Steam foi testada no wlmaker aninhado com GPU (acima); suspensão,
  troca de resolução e overlay de jogos ficam para a máquina real.
- Aplicativos libadwaita (o assistente "Instalação do Big WindowMaker" é
  um) desenham a própria moldura e ficaram com o visual do libadwaita, não
  com o tema GTK; aplicativos GTK 4 sem libadwaita podem seguir o tema GTK
  (`gtk-theme-name`). Ver `docs/theme-flicker-investigation.md`.
