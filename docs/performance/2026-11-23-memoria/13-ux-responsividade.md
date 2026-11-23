# 13 — UX e responsividade

Só o que foi medido. Itens sem medida estão no fim.

## Partida dos residentes reescritos

`big-wm-mic-tray`: do `exec` até o nome `org.bigwindowmaker.MicTray` estar no
barramento (o item só aparece na bandeja depois disso), e CPU gasta nos
3 primeiros segundos. dbus-daemon próprio, 10 partidas de cada, ordem
alternada.

| Mediana [mín–máx] | Python | C |
|---|---:|---:|
| Host: nome no barramento (ms) | 81 [80–86] | 5 [5–5] |
| Host: CPU nos 3 s iniciais (ms) | 80 [70–90] | 0 [0–0] |
| OptiPlex 760: nome no barramento (ms) | 167 [164–171] | 6 [6–10] |
| OptiPlex 760: CPU nos 3 s iniciais (ms) | 170 [170–180] | 0 [0–10] |

No login, esses 170 ms de CPU disputavam o Core 2 Duo com o resto da
sessão subindo; a versão em C sai da disputa.

## Na máquina real (OptiPlex 760, verificado à mão por uinput)

- Wayland: ícone do microfone na bandeja (página 2/2), balão e menu em
  pt_BR, Silenciar pelo menu (ícone muda na hora), clique esquerdo abre as
  configurações.
- X11: ícone no wmsystemtray (snixembed), mesmo menu.
- 21/21 verificações funcionais nas duas sessões
  (`bigwm-perf-funcional.sh`, [04](04-validacao-virt-manager.md)).

## Alocadores e resposta

A carga do lote ([02](02-metodologia-benchmark.md)) tem tempo fixo por
janela (2 s), então não mede latência de abertura; o efeito dos alocadores
na resposta está como CPU do compositor em [03](03-baseline-x11-wayland.md).

## Sem medida nesta rodada

- Latência de entrada (tecla → quadro) e de arrastar janelas: exige câmera
  ou ferramenta de captura de quadros; não feito.
- Tempo de abertura do menu da bandeja Wayland (`big-wm-tray`): não mudou
  nesta rodada.
