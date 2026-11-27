# Documentação do big-wm

Índice. Nada aqui é usado pelo código.

## Para usuários

- [Guia do usuário](guia-do-usuario.md): comece por aqui.
- Aparência: [tema claro e escuro](tema-claro-escuro.md) ·
  [aparência retrô dos aplicativos](aparencia.md) ·
  [diálogos de encerramento](dialogos-encerramento.md)
- Acessibilidade: [ampliação da tela](zoom-acessibilidade.md) ·
  [localizar o ponteiro](localizar-ponteiro.md) ·
  [Onboard no Dock](onboard-dock.md)
- Aplicativos: [aplicativos padrão](aplicativos-padrao.md) ·
  [HP, scrcpy e UxPlay](aplicativos-hp-scrcpy-uxplay.md) ·
  [chaveiro (senhas)](chaveiro.md) ·
  [Ctrl+Alt+Esc, a caveira](cursor-caveira.md) ·
  [Boas-vindas](welcome.md)
- Menu, Dock e monitores: [menu Configurações](menu-configuracoes.md) ·
  [Dock com Abas](tabbed-dock-preferencias.md) ·
  [monitores](display-resolution.md) ·
  [microfone na bandeja](systray-microfone.md)

## Técnico (como funciona)

- Sessões e paridade: [paridade X11 × Wayland](wayland-parity.md) ·
  [Dock, Clip e gavetas: paridade](dock-wayland-parity-matrix.md) ·
  [inventário do Dock no X11](dock-x11-behavior-inventory.md) ·
  [appicons, Dock e Clip](appicons-dock-clip.md) ·
  [appicons no Wayland](wayland-appicons.md) ·
  [dockapps no Wayland](wayland-dockapps.md) ·
  [decorações no Wayland](wayland-window-decorations.md) ·
  [Clip em grade: arquitetura](dock-drag-drop-architecture.md) ·
  [wmcliphist no Wayland](wmcliphist-wayland.md) ·
  [Spectacle com vários monitores](spectacle-multimonitor.md)
- Instalação e live: [Calamares](bigwindowmaker-calamares.md) ·
  [live pelo wdm](live-wdm-integration.md)
- Desempenho: [decisões](performance-decisions.md) ·
  [orçamento de memória](performance-budget.md) ·
  [benchmarks](performance-benchmarks.md) ·
  [memória da sessão X11](memoria.md) · [performance/](performance/)
- Testes: [laboratório X11/Wayland](wayland-test-lab.md) ·
  [popups no Wayland](wayland-popup-regression-tests.md) ·
  [Clip em grade](dock-clip-placement-tests.md) ·
  [checklist de publicação](RELEASE_CHECKLIST.md)
- Por componente: [i18n/](i18n/) · [notifications/](notifications/) ·
  [virtualization/](virtualization/)

## Arquivo (relatórios e investigações datados)

Registros de uma rodada de trabalho: causa raiz, evidências e decisões
daquele momento. Úteis para entender o porquê de uma escolha; **não**
descrevem necessariamente o estado atual.

- Relatórios de novembro de 2026:
  [final](RELATORIO-FINAL-2026-11.md) ·
  [mestre](RELATORIO-MESTRE-2026-11.md) ·
  [rodada 2](RELATORIO-RODADA2-2026-11.md) ·
  [auditoria geral (11/11)](AUDITORIA_GERAL.md) ·
  [bloqueio, teclado e mosaico](AUDITORIA-2026-11-BLOQUEIO-TECLADO-MOSAICO.md) ·
  [correções, rodada 2](AUDITORIA-2026-11-RODADA-2.md) ·
  [correções](BUGFIX-AUDIT.md) ·
  [validação de estabilidade](qa-validation-report.md)
- Investigações: [Ctrl+V colava um favorito](clipboard-root-cause.md) ·
  [piscadas de tema](theme-flicker-investigation.md) ·
  [clique fechava o virt-manager](virt-manager-input-debugging.md) ·
  [popups fora do lugar](wayland-popup-positioning-audit.md)
- Clip em grade: [relatório final](dock-implementation-final-report.md) ·
  [desempenho](dock-performance-report.md) ·
  [regressão](dock-regression-report.md) ·
  [validação em VM](dock-virt-manager-validation.md)
- Missões: [auditoria-sistema/](auditoria-sistema/) ·
  [clipboard-spectacle/](clipboard-spectacle/) ·
  [dockapps-audio/](dockapps-audio/) ·
  [wayland-theme-dock-fixes/](wayland-theme-dock-fixes/)

O plano do projeto fica no repositório da organização:
[PLANEJAMENTO-ESTRATEGICO.md](https://github.com/big-windowmaker/.github/blob/main/docs/PLANEJAMENTO-ESTRATEGICO.md).
