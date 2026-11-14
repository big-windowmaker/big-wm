# Validação da missão de estabilidade (2026-11-13)

Legenda: PASSOU · FALHOU · BLOQUEADO · NÃO EXECUTADO · NÃO APLICÁVEL.
"Lab" é um wlmaker aninhado (backend X11) num Xvfb privado, com XWayland,
sem tocar na sessão real. "VM" é uma VM QEMU descartável com a ISO de
2026-11-12 em que trocamos peças em RAM, não uma ISO nova.

## Matriz

| Funcionalidade | X11 | Wayland | Live ISO | VM |
|---|---|---|---|---|
| Ctrl+C/Ctrl+V (último copiado) | PASSOU (lab Xvfb, 203 verificações) | PASSOU (lab, 34 verificações) | NÃO EXECUTADO | NÃO EXECUTADO |
| Super+V não muda a área de transferência ao abrir | PASSOU (código: só escolher copia) | PASSOU (idem) | NÃO EXECUTADO | NÃO EXECUTADO |
| Favoritos não tomam o lugar da última cópia | PASSOU | PASSOU | NÃO EXECUTADO | NÃO EXECUTADO |
| wdm | NÃO APLICÁVEL | NÃO APLICÁVEL | NÃO EXECUTADO (ISO não construída) | PASSOU (X11, Wayland, sair, desligar) |
| Usuário bigwindowmaker / big | NÃO APLICÁVEL | NÃO APLICÁVEL | NÃO EXECUTADO | PASSOU (criado como o manjaro-live-setup faz) |
| Cliques sem derrubar o virt-manager | NÃO APLICÁVEL | PASSOU (spicy no lab) | NÃO EXECUTADO | NÃO EXECUTADO na sessão real |
| wmcube troca o objeto no clique | NÃO EXECUTADO | PASSOU (lab, 105 ms) | NÃO EXECUTADO | NÃO EXECUTADO |
| Gavetas (arrastar, retirar) | NÃO EXECUTADO | NÃO EXECUTADO | NÃO EXECUTADO | NÃO EXECUTADO |
| Drag-and-drop de dockapps | NÃO EXECUTADO | NÃO EXECUTADO | NÃO EXECUTADO | NÃO EXECUTADO |
| Dockapps lado a lado (Clip 2D) | já existe no Window Maker | NÃO IMPLEMENTADO | — | — |
| Persistência | NÃO EXECUTADO | NÃO EXECUTADO | NÃO EXECUTADO | NÃO EXECUTADO |

## Números

| Teste | Antes | Depois |
|---|---|---|
| Área de transferência no Wayland (lab: favorito, 3 cópias, origem fechando, imagem, 30 cópias) | 32 falhas | 0 |
| Área de transferência no X11 (Xvfb: recarga, origem fechando, 100 cópias) | 11 falhas | 0 |
| `spicy` clica com o ponteiro capturado (lab) | caiu, status 139 | vivo |
| wmcube: clique depois de 2 s de mouse a 125, 500 e 1000 Hz sobre ele | 522-679 ms, ou nunca em 5 s | 105 ms (piso da medição) |

## Desempenho

- **Área de transferência:** nenhum processo novo. O `big-wm-clipboard
  --store` já rodava a cada cópia no Wayland. Agora ele grava um arquivo de
  64 bytes (o hash) e, só quando a área de transferência esvazia, roda um
  `wl-paste -l` e um `wl-copy`. Não foi medido em CPU ou memória.
- **wlmaker 36:** um global a mais e uma comparação por movimento do
  ponteiro. Não foi medido.
- **wmcube:** lê a fila inteira a cada quadro em vez de um evento. Menos
  trabalho acumulado, mesmo intervalo de desenho. Não foi medido em CPU.

## Fora do escopo desta entrega (pendente)

- **Clip em 2D (dockapps ao lado de outros) no Wayland.** O wlmaker guarda
  o Dock, o Clip e as Gavetas como colunas (índice por posição:
  `wlmaker_dock_column_t`, `tile_drag.c`). No Window Maker, o Clip aceita
  ícones em qualquer célula vizinha. Corrigir de verdade é trocar o modelo:
  posição (coluna, linha) por ícone no `Dockapps.plist`, hit-test e encaixe
  em grade, `wlmdock --clip` desenhando lacunas 2D. É uma mudança grande,
  não um deslocamento de pixels.
- **Retirar um dockapp de uma Gaveta.** Não foi reproduzido nesta entrega.
  Pelo código, um dockapp é arrastado pela borda do quadrado ou com o
  modificador de mover (o clique no meio vai para o próprio dockapp, como no
  X11), e a Gaveta aberta é procurada antes do Dock. Falta reproduzir com
  arraste real.
- **ISO nova** com tudo isto, construída pelo CI do build-iso, e o
  virt-manager real com ela.
