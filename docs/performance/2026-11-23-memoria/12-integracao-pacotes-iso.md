# 12 — Pacotes e ISO

## O que entra

| Mudança | Onde | Pacote | Estado |
|---|---|---|---|
| `big-wm-kwin-screenshot` em C | big-wm PR #55 | `big-wm-wayland` passa a x86_64 | aberto, aguarda revisão |
| `big-wm-mic-tray` em C | big-wm PR #56 (sobre o #55) | `big-wm-session` passa a x86_64; `json-glib` nas dependências (`libdbusmenu-glib` já estava) | aberto, aguarda revisão |
| Ferramentas de medição (lotes, wrapper experimental, matriz do Virt-Manager, 21 verificações) e estes documentos | big-wm, branch `perf/alocadores` | nenhum (`tests/perf/` não é instalado) | PR desta rodada |

Nenhum alocador, tunable da glibc, ajuste de zram ou sysctl entra nos
pacotes ou na ISO ([05](05-integracao-alocadores.md),
[08](08-zram-swap-memory-pressure.md)).

## Verificado antes do PR

- `makepkg` da branch do #56 (todos os 11 pacotes) e `tests/run.sh` inteiro
  passando.
- Pacotes instalados por `pacman -U` sobre a r57 no OptiPlex 760 e na VM do
  Virt-Manager: as duas sessões sobem com os binários em C (21/21
  verificações, [04](04-validacao-virt-manager.md)).
- A troca de `any` para `x86_64` é aceita pelo pacman como atualização
  normal do mesmo nome.

## Ordem para publicar

1. Revisar e mesclar #55, depois #56 (o #56 está empilhado).
2. O repositório `packages` gera os pacotes do big-wm a partir da `main`.
3. Próxima ISO pelo fluxo de sempre (build-iso → publish-release); a
   lista de pacotes da ISO não muda (mesmos nomes; `json-glib` já está na
   imagem como dependência de outros pacotes).
