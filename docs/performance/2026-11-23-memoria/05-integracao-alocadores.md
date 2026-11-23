# 05 — Integração de alocadores

## Decisão

**Nenhum alocador alternativo entra nos pacotes ou na ISO.** wlmaker e
wmaker continuam com o malloc da glibc, sem tunables. Motivo: em todos os
boots do lote, o PSS do compositor com glibc foi o menor ou igual ao menor
([03](03-baseline-x11-wayland.md)); nenhum perfil passou no critério de
[02](02-metodologia-benchmark.md) (faixa sem sobreposição e ganho > 1 MiB).
O heap dos compositores é pequeno (6–14 MiB) e as alternativas trazem
reservas próprias por arena/segmento que custam mais do que economizam
nesse tamanho.

`glibc.malloc.arena_max=2` não muda nada mensurável: os compositores
alocam quase tudo numa thread.

## O que fica no repositório

Só a ferramenta de medição, em `tests/perf/` (nada instalado pelos pacotes):

| Arquivo | Papel |
|---|---|
| `bigwm-malloc-wrapper` | wrapper experimental da imagem de teste; lê o perfil de `/etc/bigwm-malloc-profile` |
| `bigwm-malloc-once.c` | `libbigwm-malloc-once.so`: apaga `LD_PRELOAD`/`GLIBC_TUNABLES` do ambiente no processo-alvo, para os filhos nascerem com a glibc; `BIG_WM_MALLOC_UNSET_IF_ARG` escolhe o processo pelo argumento (`wmaker --for-real`) |
| `bigwm-perf-alloc-batch.sh` | lotes com ordem girada |
| `bigwm-perf-alloc-load.sh` | carga de janelas e registro do alocador carregado em cada compositor |

## Se um dia valer a pena

Caminho por processo, nunca global: um `Exec` da sessão (`startbigwm`,
`startbigwm-wayland`) apontando para um wrapper como o acima, com o
`libbigwm-malloc-once.so` para não vazar para os filhos, empacotado e
testado com os dois compositores. Pré-requisitos: ganho medido com o
mesmo método e os riscos de [06](06-riscos-compatibilidade.md) revistos.
Nunca `/etc/ld.so.preload`, nunca `LD_PRELOAD` no ambiente da sessão.
