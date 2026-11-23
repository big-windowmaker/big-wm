# Clip em grade — regressão

| Área | Como foi conferido | Resultado |
|---|---|---|
| Estado antigo do Clip (sem células) | laboratório, `rm -rf` do HOME + firstrun | aparece igual ao `-7` (**verificado**) |
| Dock (coluna, inserção, dockapps, cheio) | arrastes Dock↔Clip no laboratório e na VM | inalterado (**verificado**) |
| Gavetas | código inalterado; `dnd`/`drawer` só tocam o Clip | **não verificado** em tela |
| Menus do lançador, do dockapp, do Clip, menu raiz | laboratório | abrem, itens corretos; não ficam sob a grade (**verificado**) |
| Clique em buraco / célula de dockapp na superfície do `wlmdock` | laboratório | atravessa para o que está embaixo (**verificado**) |
| Balão do lançador fora da coluna | laboratório | nome e comando (**verificado**) |
| Mover o Clip de canto | laboratório | grade espelhada (**verificado**) |
| Sessão X11 | nenhum arquivo do wmaker alterado | sem mudança; referência X11 conferida no live (**verificado**) |
| Testes do wlmaker | `ctest` | 30/30 (**verificado**) |

## Defeitos achados e corrigidos nesta rodada

1. Soltar um dockapp ao lado de outro no Clip **o encerrava** (`Removed`).
   → grade (0174).
2. Menu do Clip abria **por baixo** dos ícones das faixas novas. → 0177.
3. **Queda do wlmaker** (`surface.c:584 ASSERT`): clique entregue ao painel
   do Dock com o foco do seat num dockapp XWayland, logo após mover
   lançador e dockapp entre Dock e Clip (2 de 3 execuções do cenário). Não
   reproduzido no `-7` com os gestos possíveis nele (6/6). → 0178: aviso no
   log e o clique não é repassado (0 de 5 quedas). O foco preso em si fica
   como pendência (o clique daquele instante se perde).
4. Vazamento do `wlmdock --clip` na saída (lançadores da grade). Pendente.
