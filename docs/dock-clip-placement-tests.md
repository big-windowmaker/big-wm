# Clip em grade — testes

## Unitários (wlmaker, `ctest`: 30/30)

| Teste | Cobre |
|---|---|
| `clip_grid.cell_parse` | "lane,slot", valores inválidos e absurdos recusados |
| `clip_grid.geometry` | célula ↔ posição nos 4 cantos, arredondamento de `wDockSnapIcon`, vizinho por direção |
| `clip_grid.snap` | lado a lado, abaixo, diagonal, coluna, longe (fora), ocupada, o Clip, fora do monitor, própria célula, ícone isolado |
| `clip_grid.resolve` | estado antigo = coluna; células guardadas com buraco; duplicadas, sobre o Clip e fora do monitor; mais ícones que cabem |
| `clip_grid.free_cell` | ícone novo: primeiro livre da coluna, depois a faixa seguinte |
| `dockapps.cells` | gravar, reler, dockapp parado mantém, desacoplado esquece; hospedeiro dos ladrilhos |
| `toolkit box.placeholder_margin` | sem margem entre dois vazios |
| `dock tilebox.grid` | lançadores nas células, borda direita, âncora inferior |

`build-asan` (ASan + UBSan): `clip_grid` e `dockapps` sem erros nem vazamentos.

## Laboratório gráfico (reproduzível)

`~/.cache/big-wm-lab-grade2d`: `start.sh BIN` sobe Xvfb + wlmaker aninhado
(XWayland) + `wlmdock` + `wlmdock --clip` + `wmclock wmcpuload wmcube
wmmemload`; `drag.sh X1 Y1 X2 Y2 [alt]` arrasta pelo XTEST do Xvfb;
`scen2.sh` roda a sequência abaixo anotando se o wlmaker continua vivo;
`perf.sh BIN TAG` mede CPU e memória. HOME do laboratório
`/tmp/wlh-grade2d`; `systemctl`, `loginctl` e afins bloqueados (safebin).

| Caso | Passos (1280×900, Clip à esquerda) | Esperado |
|---|---|---|
| D01/D03 | Alt+arrastar `wmcpuload` do Dock para (1,0), `wmmemload` para (2,0) | lado a lado; `Cells` gravado |
| D02/D05/D07 | Netscape (0,1)→(1,1); pasta (0,2)→(3,0) | lançadores fora da coluna; `Cell` no `State.plist`; `Origin = 0` |
| D06 | dockapp novo (`wmfsm`) | primeira célula livre da coluna |
| D08 | menu do dockapp: mover direita/baixo | uma célula; troca se ocupada |
| D09/D10 | dockapp Clip→Dock; lançador Dock→Clip | transferido; os outros não se deslocam |
| D13 | arrastar e Esc | `State.plist` idêntico (`cmp`) |
| D15/D14 | soltar sobre ícone | "Tile drag refused", nada removido |
| D20/D21 | menu do Clip, menu raiz perto das faixas | inteiros, fora dos ícones |
| D23 | reiniciar wlmaker com o mesmo HOME | `State.plist` e `Dockapps.plist` iguais; mesma disposição |
| — | Mover o Clip para a direita | grade espelhada |
| D30 | 10 idas e voltas coluna↔faixa; cenário com ASan | sem queda (após 0178) |
