# Dock, Clip e gavetas — paridade X11 × Wayland

Estado em 2026-11-23: wlmaker `1:0.8.r96.g5576886-8` (patches 0173–0180,
branch `feat/clip-grade-2d` do fork) contra o Window Maker X11
(`windowmaker 0.96.0.r130.g558e67f-26`). Legenda: **igual**, **equivalente**
(mesmo resultado, outro caminho), **diferente** (decisão documentada),
**falta**.

| Comportamento | X11 | Wayland antes (-7) | Wayland agora (-8) |
|---|---|---|---|
| Dockapp ao lado de outro no Clip | grade 2D | **soltar o encerrava** (kaboom, `Removed`) | **igual** (verificado) |
| Lançador ao lado / acima / abaixo / diagonal no Clip | grade 2D | só a coluna | **igual** (verificado) |
| Buracos no Clip | sim | não (coluna compacta) | **igual** (verificado) |
| Lançadores e dockapps misturados | sim | sim, só na coluna | **igual** |
| Soltar numa célula ocupada do Clip | desacopla (perde o ícone) | — | **diferente**: recusa e volta (D14) |
| Soltar de volta na própria célula sem vizinho | desacopla | — | **diferente**: fica |
| Arraste longe do Clip (sem vizinho) | desacopla | desacopla | **igual** |
| Esc durante o arraste | não existe no X11 | cancela | cancela (verificado) |
| Ícones não se deslocam quando outro sai do Clip | sim | não (coluna compacta) | **igual** (verificado) |
| Estado antigo (sem células) | — | coluna | lido igual à coluna, sem migração destrutiva (verificado) |
| Monitor menor: célula fora da tela | ícone fica fora | — | mostrado na coluna, célula guardada volta quando cabe (teste unitário) |
| Mover Clip de canto (lado/ponta) | mantém índices | coluna | grade espelhada (verificado) |
| Dock: buracos | sim | não | **diferente** (inalterado nesta rodada: inserção desloca) |
| Dock: soltar em slot ocupado vindo de fora | recusa | insere e desloca | **diferente** (inalterado) |
| Gaveta: linha compacta, desliza | sim | sim | **igual** (inalterado) |
| Menus de dockapp: mover | não existe | cima/baixo na coluna | cima/baixo/esquerda/direita por célula (extensão) |
| Menus abertos sobre o Clip | acima dos ícones | coluna fora da área útil | evitam os ícones da grade (verificado) |
| Clip por área de trabalho / onipresente | sim | **falta** | **falta** |
| Atrair ícones (Autoattract, Keep Icon) | sim | **falta** | **falta** |
| Seleção múltipla, *Move Icon To* | sim | **falta** | **falta** |
| Clip recolhido (*Collapsed*) | sim | **falta** | **falta** |
| Posição automática alternando coluna/linha no canto | sim | fim da coluna | **diferente**: primeiro livre da coluna (a coluna é a área reservada às janelas) |

Pendências (fora desta rodada, por tamanho e risco): Clip por área de
trabalho e onipresença, atração de ícones, seleção múltipla, Clip recolhido,
buracos no Dock. Cada uma muda o formato do `State.plist`/`Dockapps.plist`
e merece rodada própria com testes.
