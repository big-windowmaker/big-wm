# Clip em grade — relatório final (2026-11-23)

## Objetivo e resultado

Dockapps e lançadores do Clip no Wayland posicionados como no Window Maker
X11: lado a lado, acima/abaixo, na diagonal e com buracos, cada um na sua
célula, persistindo. **Feito** no wlmaker `1:0.8.r96.g5576886-8` (patches
0173–0180; 0180 veio depois da validação: o foco preso do painel depois de um arraste).

## Causa raiz

O Clip do wlmaker era uma coluna numa ordem só; `wlmaker_tile_drag_snap()`
recusava qualquer posição fora da coluna, e soltar ali era "fora das
docas": o dockapp era encerrado e marcado `Removed`. O X11 usa uma grade
(`wDockSnapIcon`, `WM_CLIP`, `CLIP_ATTACH_VICINITY` = 1). A documentação da
"Rodada 2" (`appicons-dock-clip.md`) estava correta e integrada (os arquivos
`tile_drag.c`, `dockapps.c`, `dock_launchers.c` existem na série de patches
que gera o pacote; não no `main` do fork), mas escolheu de propósito uma
coluna sem buracos.

## Alterações

| Repositório | Branch | Conteúdo |
|---|---|---|
| wlmaker (fork) | `feat/clip-grade-2d` (6 commits sobre `4a46af5`, o do `-7`) | `clip_grid.[ch]` novo; `clip.c`, `dockapps.c`, `dock_launchers.c`, `tile_drag.c`, `layer_panel.c`, `dnd.c`, `context_menu.c`, `root_menu.c`, `icon_manager.c`, `wlmdock.c`, `dock/tilebox.c`, `toolkit/box.c`, `toolkit/surface.c`, `element.h`; testes |
| packages | `feat/wlmaker-clip-grade-2d` | wlmaker `-8`: patches 0173 (do `-7`) e 0174–0180 |
| big-wm | `docs/clip-grade-2d` | esta documentação e capturas |

Formatos novos (aditivos, sem migração destrutiva): `Cell = "lane,slot"` nos
lançadores do Clip em `State.plist`; `Cells = { Order = { … }; }` em
`Dockapps.plist`; seção `Clip` do layout publicado com `Cells`, `Origin`,
`DockappCells`. Riscos: versões antigas do wlmaker ignoram as chaves e voltam
à coluna (sem perder nada).

## Validação

- Unitários 30/30 (novos: `clip_grid.*`, `dockapps.cells`,
  `box.placeholder_margin`, `tilebox.grid`); ASan/UBSan sem erros.
- Laboratório aninhado: todos os casos de
  [dock-clip-placement-tests.md](dock-clip-placement-tests.md) **verificados**.
- VM libvirt (ISO r57, live e **instalada** pelo Calamares): X11 de
  referência e Wayland com os mesmos resultados; reinício de sessão e de
  máquina preservam o layout (**verificado**,
  [dock-virt-manager-validation.md](dock-virt-manager-validation.md)).
- Desempenho: igual em repouso; +17 ms de CPU por soltura
  ([dock-performance-report.md](dock-performance-report.md)).
- **Máquina real (OptiPlex 760): verificado.** Reinstalada pela ISO r57 +
  `-8`; grade, recusa, Esc, menu, reinício da máquina, 60 movimentos e
  sessão X11 ([validação](dock-virt-manager-validation.md)), com o `-8`
  de 0174–0179; sessão padrão Wayland.

## Limitações e pendências

- Fora desta rodada (paridade ainda **falta**): Clip por área de trabalho e
  onipresente, atrair ícones, seleção múltipla, Clip recolhido, buracos no
  Dock ([matriz](dock-wayland-parity-matrix.md)).
- Diferenças deliberadas: célula ocupada recusa (X11 desacopla); ícone volta
  à própria célula mesmo sem vizinho; ícone novo vai à coluna.
- Foco do ponteiro preso no painel do Dock por um instante após mover itens
  Dock↔Clip: o clique daquele instante é descartado (antes derrubava a
  sessão). Causa no toolkit não isolada.
- Instalador (pré-existente): o Calamares tenta habilitar `sddm`, `gdm`,
  `cups-browsed`, `pkgfile-update.timer` (inexistentes): só avisos.
- Nada foi enviado ao GitHub: branches locais aguardando sua autorização
  para push/PR.
