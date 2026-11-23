# Clip em grade — arquitetura (wlmaker -8)

## Por que mudou

O Clip do wlmaker era uma coluna numa ordem só (lançadores e dockapps
misturados, `wlmaker_dock_column_t::dockapp_positions`). A coluna não
representa "ao lado de": `wlmaker_tile_drag_snap()` só aceitava a coluna
(`across == 0`), então soltar um dockapp ao lado de outro caía em "fora das
docas" e o encerrava (`Removed` em `Dockapps.plist`). O Window Maker trata o
Clip como grade (ver [inventário](dock-x11-behavior-inventory.md)).

## Modelo

Cada ícone do Clip tem uma **célula** `(lane, slot)`: `lane` é a distância da
borda do monitor em ladrilhos, `slot` a distância da âncora; o Clip é (0,0).
A célula não depende do canto: mover o Clip de lado ou de ponta espelha a
grade.

| Peça | Arquivo | Papel |
|---|---|---|
| Regras e geometria | `src/clip_grid.[ch]` (novo, lógica pura) | célula ↔ posição, limites do monitor, `wlmaker_clip_grid_snap` (= `wDockSnapIcon` `WM_CLIP`: livre, na tela, vizinho a ≤ 1 inclusive diagonal), resolução de estados |
| Estado do Clip | `src/clip.c` | grade atual (`wlmaker_clip_t::grid`), faixas, congelamento, layout publicado |
| Persistência dos dockapps | `src/dockapps.c` | `Dockapps.plist`: `Cells = { Order = { <nome> = "lane,slot"; }; }` |
| Persistência dos lançadores | `src/dock_launchers.c` | `State.plist`: chave `Cell = "lane,slot"` no dict de cada lançador do Clip |
| Arraste | `src/tile_drag.c` | encaixe na grade, célula ocupada recusa, aplicação transacional |
| Ponteiro sobre lançadores | `src/layer_panel.c` | célula → lançador; fora de um lançador o ponteiro atravessa |
| Desenho dos lançadores | `src/wlmdock.c`, `src/dock/tilebox.c` | `wlmdock --clip` lê `Cells`/`Origin` e monta faixas (caixas aninhadas), células vazias transparentes |
| Margens | `src/toolkit/box.c`, `element.h` | `placeholder`: sem linha entre dois ladrilhos vazios |

## Desenho

- **Faixa 0** é o próprio dock do Clip (com a zona exclusiva de sempre): o
  Clip, depois um ladrilho por slot até o último ícone; slots sem dockapp
  recebem um espaçador transparente que deixa o ponteiro passar.
- **Faixas ≥ 1** com dockapps ganham um `wlmtk_dock` próprio, `overlay` (sem
  zona exclusiva, como no X11 as janelas podem ficar por baixo) e
  `edge_offset = lane × passo`. Seguem o Clip em canto, nível
  (acima/abaixo/auto), monitor e tema.
- Os ladrilhos dos dockapps continuam do placement `dockapps.c`; um
  **hospedeiro** (`wlmaker_dockapps_set_host`) os distribui pelas faixas.
- Os **lançadores** continuam desenhados pelo `wlmdock --clip`: o compositor
  publica em `$XDG_RUNTIME_DIR/wlmaker-docks-<display>.plist` a seção `Clip`
  com `Cells` (uma por lançador mostrado) e `Origin` (0 se algum lançador
  está na linha do Clip, então a superfície começa no slot 0).

## Estados antigos e monitor menor

`_wlmaker_clip_update_grid()`: lançadores e dockapps com célula guardada,
dentro dos limites e sem conflito, ficam nela; os demais recebem a primeira
célula livre da coluna **na ordem linear antiga** (`AfterLaunchers`). Um
estado sem nenhuma célula aparece igual a antes. Uma célula guardada fora do
monitor atual é mostrada na coluna, mas **não é sobrescrita** no arquivo:
volta quando o monitor comporta.

## Arraste como transação

1. Início: a origem é a célula do ícone; um lançador é coberto por um
   ladrilho vazio; Esc cancela.
2. Movimento: `wlmaker_clip_cell_at` + `wlmaker_clip_grid_snap` →
   *ATTACH* (fantasma na célula), *OCCUPIED* (sem fantasma; soltar = voltar),
   *OUTSIDE* (desacopla como no X11, salvo travado/em execução).
3. Soltar: desliza; então **congela** (`wlmaker_clip_freeze_grid`: grava as
   células que ainda não estavam gravadas, para nada se deslocar), aplica a
   nova célula (um único `set_cells`/`move_cell`) e rearranja.

Gravações: atômicas (`bs_dynbuf_write_file`: temporário + `rename`, cópia
`.old`). Nenhuma gravação durante o movimento do ponteiro.

## Menus

Menus de contexto e o menu raiz são janelas do workspace (abaixo da camada
TOP onde o Clip fica): `wlmaker_clip_grid_avoid()` os afasta dos ícones da
grade, como já se fazia com gavetas abertas.
