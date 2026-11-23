# Memória, alocadores e estabilidade (2026-11-23)

Rodada sobre a imagem r57 (VM do harness, VM do Virt-Manager e OptiPlex
760). Resultado em uma linha: **a glibc fica**; o ganho veio de reescrever
em C dois residentes Python (−16,5 MiB residentes no X11; −31,4 MiB
residentes e −33,3 MiB de PSS da sessão no Wayland). Números em
[11](11-resultados-finais.md).

| Arquivo | Conteúdo |
|---|---|
| [00-auditoria-inicial.md](00-auditoria-inicial.md) | Onde a memória está; quanto o alocador pode mudar |
| [01-comparativo-alocadores.md](01-comparativo-alocadores.md) | glibc, mimalloc, jemalloc, TCMalloc, snmalloc, Scudo: teoria e medida |
| [02-metodologia-benchmark.md](02-metodologia-benchmark.md) | Harness, perfis, lotes, critério de decisão |
| [03-baseline-x11-wayland.md](03-baseline-x11-wayland.md) | Resultados dos lotes por sessão e perfil |
| [04-validacao-virt-manager.md](04-validacao-virt-manager.md) | Matriz de memória e resolução na VM do Virt-Manager, 21 verificações funcionais |
| [05-integracao-alocadores.md](05-integracao-alocadores.md) | Decisão: nenhum alocador entra; o que fica no repositório |
| [06-riscos-compatibilidade.md](06-riscos-compatibilidade.md) | Riscos de trocar o malloc e como o mecanismo por processo os trata |
| [07-otimizacoes-estruturais.md](07-otimizacoes-estruturais.md) | `big-wm-kwin-screenshot` e `big-wm-mic-tray` em C; propostas |
| [08-zram-swap-memory-pressure.md](08-zram-swap-memory-pressure.md) | Pressão no OptiPlex (HDD): sem OOM; gargalo de I/O |
| [09-hardware-legado.md](09-hardware-legado.md) | OptiPlex 760: o que foi medido na máquina real |
| [10-estabilidade-e-stress-tests.md](10-estabilidade-e-stress-tests.md) | Lotes, carga, sessões longas, falhas |
| [11-resultados-finais.md](11-resultados-finais.md) | Tabelas finais, X11 e Wayland separados |
| [12-integracao-pacotes-iso.md](12-integracao-pacotes-iso.md) | PRs, pacotes, ISO |
| [13-ux-responsividade.md](13-ux-responsividade.md) | Partida dos residentes, verificação na máquina real |

Decisões anteriores continuam valendo:
[../../performance-decisions.md](../../performance-decisions.md).
