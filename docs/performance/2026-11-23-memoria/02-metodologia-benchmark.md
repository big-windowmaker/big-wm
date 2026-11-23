# 02 — Metodologia

Mesmo harness das rodadas anteriores (`tests/perf/`, método geral em
[../10-metodo.md](../10-metodo.md)); aqui só o que é desta rodada.

## VM

`bigwm-perf-vm.py boot`: QEMU/KVM, `-cpu host`, 2 vCPUs, 4096 MiB,
virtio-vga 1920x1080 sem 3D, overlay descartável sobre uma imagem base
somente leitura (r57 instalada pelo Calamares + wlmaker -8, alocadores do
repositório instalados). Login real pelo wdm (teclado virtual do QEMU),
coletor como root (`bigwm-perf-collect.py`: `smaps_rollup` de todo
processo, `/proc/meminfo`, PSI, CPU).

Novidades no harness:
- `BIGWM_PERF_RAM_MIB` e `BIGWM_PERF_RES` mudam memória e resolução da VM
  do harness;
- `--setup '@push ORIGEM DESTINO'` envia arquivos (bibliotecas maiores que
  o limite de 128 KiB de um argumento);
- `bigwm-perf-alloc-batch.sh`: `BIGWM_PERF_PUSH`, `BIGWM_PERF_SAMPLES` e
  `BIGWM_PERF_LOAD_REPEAT` (sessões longas);
- `bigwm-perf-libvirt.sh` (VM do Virt-Manager): `BIGWM_PERF_SAMPLES` e
  `BIGWM_PERF_FUNCIONAL=1`; `bigwm-perf-virt-matrix.sh` monta a matriz
  ([04](04-validacao-virt-manager.md));
- `bigwm-perf-alloc-children.sh`: confere que o alocador não vazou para
  outros processos ([06](06-riscos-compatibilidade.md)).

## Alocador por processo

`bigwm-malloc-wrapper` (só na imagem de teste, em `/usr/local/bin/wlmaker`
e `/usr/local/bin/wmaker`) lê o perfil de `/etc/bigwm-malloc-profile`,
põe a biblioteca em `LD_PRELOAD` junto com `libbigwm-malloc-once.so` e
executa o binário de `/usr/bin`. O `libbigwm-malloc-once.so`
(`bigwm-malloc-once.c`) apaga do ambiente, no construtor, as variáveis de
`BIG_WM_MALLOC_UNSET`: os filhos do compositor (terminal, navegador,
Steam, dockapps) nascem com o malloc da glibc.

Armadilha do X11: `wmaker` é um lançador que inicia `wmaker --for-real`, o
gerenciador de janelas. Com `BIG_WM_MALLOC_UNSET_IF_ARG=--for-real` a
limpeza só acontece no processo cujo `/proc/self/cmdline` tem o argumento;
o lançador repassa o preload ao filho. Sem isso, o alocador nunca chegava
ao WM (visto no `/proc/PID/maps`). Cada boot grava as bibliotecas
carregadas em cada compositor (`alloc-load.log`).

## Perfis

| Perfil | O que é |
|---|---|
| glibc | padrão (glibc 2.44) |
| glibc-a2 | glibc com `GLIBC_TUNABLES=glibc.malloc.arena_max=2` |
| mimalloc | mimalloc 3.5.4 (extra) |
| jemalloc | jemalloc 5.4.0 (extra) |
| tcmalloc | gperftools 2.18.1, `libtcmalloc_minimal` (não é o google/tcmalloc atual) |
| snmalloc | snmalloc 0.7.6, `libsnmallocshim.so` (lote 2) |
| scudo | Scudo standalone do compiler-rt 23 (lote 2) |

## Lotes

`bigwm-perf-alloc-batch.sh IMAGEM SENHA SAÍDA RODADAS PARALELO`:

- lote 1: 5 perfis × 2 sessões × 5 rodadas = 50 boots, 4 em paralelo;
  lote 2: glibc + snmalloc + scudo × 2 sessões × 3 rodadas;
- ordem dos perfis girada a cada rodada (o perfil 1 da rodada 1 é o último
  da rodada 5), para o paralelismo e o aquecimento do host não favorecerem
  ninguém;
- carga 150 s após o boot (`bigwm-perf-alloc-load.sh`): 40 ciclos de abrir
  e fechar o gnome-calculator (2 s abertos); no Wayland, alternando Wayland
  nativo e XWayland. Exercita mapeamento, foco, decoração, buffers e
  destruição de janelas no compositor;
- amostras em 30, 90, 300 e 600 s (300 s = logo depois da carga; 600 s =
  repouso depois dela).

Estatística: mediana e [mín–máx] por perfil e sessão
(`bigwm-perf-summary.py`). Um perfil só "ganha" se a faixa dele não se
sobrepõe à do glibc no compositor e o ganho passa de 1 MiB.

## Sessões longas, matriz e máquina real

- [04](04-validacao-virt-manager.md): matriz de memória (2/4/8/16 GiB) e
  resolução na VM do Virt-Manager, com `bigwm-perf-funcional.sh` (21
  verificações por sessão);
- [10](10-estabilidade-e-stress-tests.md): sessões de 1 h;
- [09](09-hardware-legado.md): OptiPlex 760, sessão real pelo wdm, entrada
  por uinput (o mesmo caminho de um mouse e teclado).

## O que não entra

- Medições com o host ocupado por outra coisa que não o lote.
- Valores de um boot só apresentados como conclusão: os de N=1 estão
  marcados.
