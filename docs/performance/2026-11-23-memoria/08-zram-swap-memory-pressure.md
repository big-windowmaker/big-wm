# 08 — zram, swap e pressão de memória

## Configuração (como vem na r57; nada mudado nesta rodada)

zram zstd 1,7 GiB (prioridade 32767) + swapfile 512 MiB, zswap desligado,
`swappiness` 30, `min_free_kbytes` 115101, `watermark_scale_factor` 150,
`watermark_boost_factor` 15000, THP `madvise`, systemd-oomd inativo. As
marcas d'água vêm do `auto-rules-io` da BigLinux; a troca já foi medida
numa VM e rejeitada (A1 em
[../../performance-decisions.md](../../performance-decisions.md)). Pela
regra da missão, nenhum desses valores foi alterado sem medida.

## Pressão no hardware real (OptiPlex 760, disco rígido)

`bigwm-perf-pressure.py` (o mesmo teste do A1), sessão X11: um processo
aloca 3146 MiB de memória anônima **incompressível** (MemTotal − 600 MiB; o
zram não a comprime) e continua tocando nela; outro lê 1 GiB em laço; por
120 s o gnome-calculator abre a cada 15 s e `python3 -c pass` roda a cada
2 s. 3 rodadas, mediana [mín–máx]:

| | OptiPlex (N=3) | VM do A1, mesmo teste (N=6) |
|---|---:|---:|
| MemAvailable em repouso (MiB) | 2723 [2711–2866] | 2907 [2878–2926] |
| abertura do aplicativo em repouso (s) | 0,66 [0,64–0,69] | — |
| 1ª abertura sob pressão (s) | 16,9 [14,0–23,4] | 1,9 [1,7–2,2] |
| aberturas sob pressão, mediana (s) | 1,4 [1,0–4,3] | 0,5 [0,4–0,8] |
| `python3 -c pass`, p90 (ms) | 49 [41–71] | 47 [41–64] |
| PSI memory some / full (s em ~139 s) | 21,8 / 16,9 | 7,8 / 6,1 |
| PSI io some / full (s) | 68,7 / 42,6 | — |
| swap escrita (MiB) | 3097 [2770–3306] | — |
| refaults de arquivo (mil páginas) | 6944 [5357–7188] | 488 |
| processos mortos pelo OOM | 0 nas 3 | 0 nas 6 |

Leitura:
- **Sem OOM e sem travar**: a sessão, o Window Maker e o shell continuaram
  respondendo; o comando curto ficou no mesmo p90 da VM.
- **O gargalo é o disco rígido**: a primeira abertura sob pressão leva
  14–23 s porque o código do aplicativo foi despejado do cache e volta do
  HDD (milhões de refaults; I/O "some" em metade do teste). Na VM (disco em
  SSD) a mesma abertura leva 2 s.
- O zram não ajuda neste teste de propósito (dados incompressíveis); com
  memória de aplicativos reais (comprimível) ele absorve mais. O caso real
  "tudo aberto" do OptiPlex está em [../11-cenarios.md](../11-cenarios.md).

## Decisão

Nada muda no sistema de memória: nenhuma medida aponta para um ajuste que
resolva o gargalo (I/O de disco), e o ajuste já testado (A1) aumentava o
reclaim direto e causava OOM. O que reduz a pressão de verdade é a sessão
ocupar menos: os portes em C desta rodada tiram 31,4 MiB residentes no
Wayland e 16,5 MiB no X11 ([11](11-resultados-finais.md)).

Não medido: zswap no lugar do zram; `vm.page-cluster` 0 com zram (padrão
do kernel: 3). Ficam como hipóteses, sem mudança.
