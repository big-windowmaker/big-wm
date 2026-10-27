# Referências históricas

Separação estrita entre **fato documentado** (com fonte) e **interpretação**
(decisão de projeto deste dock). O vídeo do OPENSTEP 4.0 beta
(<https://www.youtube.com/watch?v=WyxByfhT1F0>, título "OPENSTEP 4.0 Demo,
Part 1") não pôde ser assistido por quem implementou; só textos e o código do
GNUstep foram consultados. Nenhuma imagem de terceiros foi copiada.

## OPENSTEP 4.0 pré-lançamento — a prateleira com abas

**Fatos documentados**

- Leon von Stauber, notas de uso de um "early prerelease of OpenStep 4.0 for
  Mach", atualizadas em 14/12/1995 — <https://www.occam.com/ocr/computer/os4m.html>:
  - a prateleira com abas "replaces the Workspace shelf and the Dock";
  - fica escondida na borda inferior da tela e aparece/some "just by flicking
    the mouse toward/away from it, even if it's covered by windows";
  - "The NeXT icon, the Documents icon, and the Recycler appear on every tabbed
    shelf"; o ícone NeXT mostra os aplicativos em execução e o Documents mostra
    os documentos abertos por tipo; juntos eliminam as fileiras de ícones de
    aplicativo e de miniaturas na base da tela;
  - minimizar uma janela de documento "now just makes it disappear".
- BetaWiki (resumo de busca; página atrás de Cloudflare) —
  <https://betawiki.net/wiki/OPENSTEP_4.0_Pre-release_1>: build de setembro de
  1995, "a dock with tabs that sort programs based on their type";
  removida "alegadamente" por frustrar usuários do Dock original
  (**motivo não confirmado**).
- OSNews, "openstep 4 beta video available" —
  <https://www.osnews.com/story/8962/openstep-4-beta-video-available/>:
  "legendary GUI design study which has never been officially released"; um
  comentário descreve "the shelf system at the bottom, as the Dock is in OS X".
- Wikipedia (<https://en.wikipedia.org/wiki/OPENSTEP>): o 4.0 lançado em julho
  de 1996 "retained the classic NeXTSTEP user interface" (Dock e Shelf). Nada
  sobre a interface do pré-lançamento.

**Interpretação adotada**

| Elemento | Referência | Big Tabbed Dock |
|---|---|---|
| Lugar | faixa horizontal na base | base por padrão; topo opcional |
| Esconder | sempre escondida, aparece ao "tocar" a borda | opcional (auto-ocultar); padrão visível com reserva de espaço, porque o Window Maker já usa a base para os ícones de aplicativo |
| Abas | agrupam programas por tipo | Geral, Desenvolvimento, Documentos, Temporário + abas do usuário |
| Ícone NeXT (em execução) em todas as abas | fato | seção "em execução" à direita de cada aba, com os abertos que não estão nela |
| Documents / Recycler | fato | não recriados: a aba Documentos e o gerenciador de arquivos cobrem o uso; Recycler não existe no Window Maker |
| Largura | provavelmente a tela toda (não documentado em texto) | ajustada ao conteúdo; "largura total" opcional |

A data "OPENSTEP 4.0 beta de 1996" que circula não bate com as fontes: o build
documentado é de fim de 1995 e o 4.0 final (1996) já não tinha a prateleira.

## GNUstep GWorkspace — TabbedShelf

**Fatos** (<https://github.com/gnustep/apps-gworkspace>, diretório
`GWorkspace/TShelf/`): `TShelfWin`, `TShelfView`, `TShelfViewItem`,
`TShelfIconsView` etc. Janela sem borda da largura da tela, 106 px, em y=0,
com auto-ocultar animado; abas de arquivos e de dados (área de transferência);
células de 64×52; faixa de abas de 24 px; abas com laterais em curva (Bézier)
de 14 px, lateral esquerda clara (branca), direita escura (preta), linha branca
no topo; aba selecionada desenhada por último, com a cor do corpo, "à frente";
rótulos Helvetica-Bold 12.

**Licença**: GPL v2 ou posterior (`COPYING` e cabeçalhos, © FSF, Enrico
Sersale, Riccardo Mottola). O big-wm é GPL-2.0-or-later, então seria
compatível, mas o código é Objective-C/AppKit e **nada foi copiado**: o
desenho das abas deste dock é implementação própria em cairo
(`tabbed-dock/src/ui/btd-draw.c`) a partir da descrição acima.

## Convenções visuais NeXT/OpenStep usadas

- Quatro cinzas: preto, #555555, #AAAAAA, branco (`NSDarkGray` = 1/3,
  `NSLightGray` = 2/3 — libs-gui `Source/externs.m`).
- Relevo de botão (`NSDrawButton`): branco em cima/esquerda, preto
  embaixo/direita mais uma linha cinza-escuro interna; afundado
  (`NSDrawGrayBezel`) é o inverso.
- Window Maker: ladrilhos de 64 px (`IconSize`), "..." no ladrilho de um
  aplicativo do Dock que não está rodando.
- Tema Big WindowMaker: gradiente do título #505a5e→#202a2e com texto branco
  negrito; menu/ícone #c2c0c5→#828085; IconBack dgradient #a6a6b6→#515561.

**Mapeamento no estilo Clássico**: prateleira em gradiente do menu
(#c2c0c5→#aaa8ad) com relevo NSDrawButton; aba ativa com a cor da prateleira e
sem borda inferior (funde-se com ela), abas inativas #939196 e 2 px mais
baixas; ladrilhos com o IconBack do tema; aplicativo ativo com moldura preta
interna; fonte Helvetica/Nimbus Sans negrito 12 px. Diferença consciente: o
indicador de execução é um "pino" em relevo por janela (o "..." do Window
Maker marca o contrário, o que não está rodando, e confundiria quem vem de
outros docks).
