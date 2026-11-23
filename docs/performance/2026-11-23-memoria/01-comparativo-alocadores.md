# 01 — Comparativo de alocadores

Versões dos repositórios do Manjaro em 2026-11-23. Teoria resumida, depois
o que foi medido nos compositores ([02](02-metodologia-benchmark.md),
[03](03-baseline-x11-wayland.md)).

## Candidatos

| Alocador | Versão | Como funciona (resumo) | Ponto forte esperado | Custo esperado |
|---|---|---|---|---|
| glibc ptmalloc2 | 2.44 | arenas por thread, bins, tcache por thread; devolve memória ao kernel pelo topo do heap e por `madvise` em chunks grandes | sem dependência; ferramentas (ASan, Valgrind, `malloc_info`); hardening | fragmentação em programas multithread de vida longa |
| glibc + `arena_max=2` | 2.44 | limita as arenas | menos memória em programas com muitas threads | nada num programa de uma thread |
| mimalloc | 3.5.4 | segmentos e páginas por tamanho, listas livres por página, purga atrasada | velocidade, pouca fragmentação | reserva segmentos grandes de saída |
| jemalloc | 5.4.0 | arenas, slabs por classe de tamanho, decay de páginas sujas | fragmentação baixa em servidores multithread | metadados e arenas fixos |
| TCMalloc (gperftools) | 2.18.1 | cache por thread + heap central, `libtcmalloc_minimal` | velocidade | caches por thread; não é o google/tcmalloc atual (esse não é empacotado) |
| snmalloc | 0.7.6 | message passing entre threads, slabs | multithread com produtor/consumidor | reservas de slab |
| Scudo (compiler-rt) | 23 | alocador endurecido do LLVM (Android): checagens de cabeçalho, quarentena opcional | segurança | mais metadados por bloco |

Outros (Hoard, rpmalloc, PartitionAlloc) não estão empacotados para a
distribuição: ficaram de fora para não depender de binários de terceiros.

## Medido (PSS do compositor a 600 s, depois da carga; mediana [mín–máx])

| Perfil | wmaker (X11) | Δ glibc | wlmaker (Wayland) | Δ glibc | N |
|---|---:|---:|---:|---:|---:|
| glibc (lote 1) | 10,1 [10,1–10,3] | — | 27,2 [27,2–27,4] | — | 5 |
| glibc (controle do lote 2) | 10,2 [10,0–10,2] | — | 27,3 [27,2–27,3] | — | 3 |
| glibc-a2 | 10,2 [10,0–10,2] | +0,1 | 27,3 [27,3–27,4] | +0,1 | 5 |
| jemalloc | 11,0 [10,9–11,1] | +0,9 | 28,2 [28,1–28,4] | +1,0 | 5 |
| snmalloc | 11,4 [11,4–11,5] | +1,2 | 29,3 [29,3–29,3] | +2,0 | 3 |
| scudo | 12,9 [12,9–12,9] | +2,7 | 29,7 [29,6–29,8] | +2,4 | 3 |
| tcmalloc | 14,4 [14,4–14,5] | +4,3 | 31,1 [30,9–31,2] | +3,9 | 5 |
| mimalloc | 23,4 [21,7–25,7] | +13,3 | 39,5 [39,5–39,7] | +12,3 | 5 |

CPU total do compositor até 600 s: nenhuma diferença fora da dispersão em
nenhum perfil (tabelas em [03](03-baseline-x11-wayland.md); lote 2, X11 /
Wayland: glibc 660 / 4260 ms, snmalloc 690 / 4420, scudo 730 / 4540, faixas
sobrepostas).

Estabilidade: 68 boots, nenhum compositor caiu ou reiniciou; todos
carregaram a biblioteca do seu perfil (conferido em `/proc/PID/maps`).

## Conclusão

Com heaps de 6–14 MiB e uma thread, o ptmalloc2 da glibc é o mais
econômico; cada alternativa adiciona a sua reserva fixa (de 1 a 13 MiB) e
não compra CPU mensurável. O controle glibc repetiu os mesmos números nos
dois lotes. **Recomendação: glibc**, sem tunables
([05](05-integracao-alocadores.md)). Scudo só faria sentido por
hardening (+2,4–2,7 MiB), o que não foi pedido nem avaliado.
