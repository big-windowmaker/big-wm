# 06 — Riscos de compatibilidade

Riscos de trocar o malloc de um processo da sessão e o que o mecanismo desta
rodada ([05](05-integracao-alocadores.md)) faz com cada um.

| Risco | Onde aparece | Situação |
|---|---|---|
| Preload herdado pelos filhos | todo programa aberto pelo Dock, menu, atalhos: navegador, Wine/Proton, Steam, jogos (anti-cheat que recusa `LD_PRELOAD`), apps com malloc próprio | **evitado**: o `libbigwm-malloc-once.so` apaga `LD_PRELOAD`/`GLIBC_TUNABLES` no construtor do processo-alvo; os filhos nascem com a glibc. **Verificado** com jemalloc no compositor (`bigwm-perf-alloc-children.sh`, 1 boot por sessão): nenhum dos 217 (Wayland) e 203 (X11) outros processos tinha alocador no ambiente ou no `/proc/PID/maps`; no X11, os 11 dockapps iniciados pelo `wmaker --for-real` nasceram sem ele |
| Preload que a sessão já usa | a sessão X11 exporta `LD_PRELOAD=/usr/lib/libgtk-nocsd.so.0` (gtk-nocsd) para todos os processos | **achado**: o wrapper experimental sobrescreve esse valor e o `libbigwm-malloc-once.so` apaga a variável inteira; o WM e os filhos diretos dele (dockapps) ficaram sem o gtk-nocsd nos boots de teste. Se um dia for adotado, o wrapper tem de acrescentar ao valor herdado e o construtor tem de devolver o valor original, não apagar |
| Lançador × processo real | `wmaker` inicia `wmaker --for-real` | **tratado**: a limpeza só acontece no processo com `--for-real` (`BIG_WM_MALLOC_UNSET_IF_ARG`); sem isso o WM nunca recebia o alocador |
| Mistura de alocadores | memória alocada por um malloc e liberada por outro (biblioteca carregada antes do preload, `dlopen` com `RTLD_DEEPBIND`) | o preload entra antes de tudo no processo inteiro; nos 68 boots dos lotes o compositor manteve o mesmo PID do início ao fim (nenhuma queda) |
| `/etc/ld.so.preload` ou `LD_PRELOAD` global | todos os processos do sistema, incluindo os setuid e os de terceiros | **não usado**, por regra |
| Biblioteca removida ou atualizada | pacote do alocador desinstalado, ABI nova | com preload por wrapper, o `ld.so` só avisa e segue com a glibc ("cannot be preloaded: ignored"); o compositor sobe do mesmo jeito |
| glibc atualizada | tunables renomeados | `GLIBC_TUNABLES` desconhecido é ignorado pela glibc |
| Diagnóstico | ASan, Valgrind, `malloc_info`, `MALLOC_CHECK_` | com outro alocador, as ferramentas da glibc não enxergam o heap; depurar sempre com o perfil glibc |
| Segurança | hardening da glibc (tcache double free, checagens de chunk) | jemalloc, mimalloc e tcmalloc têm outras checagens; Scudo é o mais endurecido. O compositor lida com dados de clientes (buffers, títulos, protocolos): trocar o malloc dele muda a superfície |
| Memória sob pressão | alocadores com cache por thread devolvem memória ao kernel por política própria (decay, purge) | medido em [03](03-baseline-x11-wayland.md) e [10](10-estabilidade-e-stress-tests.md) |
| Fork do Window Maker / wlmaker | atualização do pacote | o wrapper fica fora do pacote; mudança em `Exec` das sessões exigiria testar os dois pacotes juntos |

## Fora de questão

- Pré-carregar no Firefox, Chromium, Wine, Steam ou qualquer programa de
  terceiros.
- Mudar a glibc do sistema ou o malloc padrão da distribuição.
- Ligar um alocador "por precaução" sem ganho medido.
