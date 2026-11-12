# Relatório final — missão de novembro de 2026

Integração das correções e recursos novos do Big WindowMaker (X11 e Wayland).
Instalado na máquina de desenvolvimento em 09/11/2026; **nada foi publicado**
(sem push, sem PR, sem ISO).

## Pacotes instalados

| Pacote | Versão | Origem |
|---|---|---|
| wlmaker | 1:0.8.r80.g7481afe-26 | fork, branch `fix/xwl-toplevel-geometry` (patch 0144 com toda a integração) |
| windowmaker | 0.96.0.r130.g558e67f-17 | fork, branch `fix/x11-borderless-games` (patches 0020–0023) |
| big-wm-* (11 pacotes, novo `big-wm-zoom`) | 26.11.09-0249 | branch `feat/missao-integracao` |
| big-wm-display-resolution (novo) | 26.11.09-0245 | repositório local `big-wm-display-resolution` |
| wmcliphist (big-dockapps) | 20260412-10 | packages, branch `fix/dockapps-wayland-2026-11` |

## Resultado por área

Estados: **Implementado e testado**; **Implementado, pendente de teste real**;
**Parcialmente funcional**; **Bloqueado por limitação comprovada**.

| Funcionalidade | X11 | Wayland | Testes | Resultado |
|---|---|---|---|---|
| Steam (moldura, arrastar) | Implementado, pendente de teste real | Implementado e testado | Steam real com GPU (Radeon RX 9060 XT) num wlmaker aninhado: 16 arrastes, encaixe, maximizar, menus; A/B com o wlmaker 24 (defeito reproduzido) e com os binários dos pacotes 25 e 26 | Conteúdo fica na moldura ([wayland-window-decorations.md](wayland-window-decorations.md)) |
| Decoração em todas as janelas | Implementado e testado | Implementado e testado | Laboratórios X11/Wayland: GTK 3/4, libadwaita, Qt, Chrome, Telegram, AshyTerm; jogos sem bordas (janela do tamanho da tela) | Moldura do Window Maker; jogos em tela cheia sem bordas ficam sem moldura |
| Appicons | Implementado e testado | Implementado e testado | Laboratórios: GTK 3/4, Qt 6, Chrome/Electron, Firefox, xterm, app sem .desktop; menu, duplo clique | [appicons-dock-clip.md](appicons-dock-clip.md) |
| Dock/Clip (arrastar, lançadores) | Implementado e testado | Implementado e testado | Arrastar para o Dock/Clip/gaveta, Esc, persistência após reiniciar; comandos quebrados do WMState migrados | Lançadores com `gtk-launch` do .desktop |
| wmcliphist | Implementado e testado (sem mudança) | Implementado e testado | 8 rodadas, 88 verificações em 2 saídas; escala 1,5; área de transferência X11↔Wayland | [wmcliphist-wayland.md](wmcliphist-wayland.md) |
| DockApps | Implementado e testado | Implementado e testado | Auditoria ladrilho por ladrilho (30 pelo XWayland, 3 nativos); wmpager fora (limitação comprovada) | [wayland-dockapps.md](wayland-dockapps.md) |
| Tema escuro | Implementado e testado | Implementado e testado | Alternâncias claro/escuro com GTK 3/4, libadwaita, Thunar pelo D-Bus, Qt, dunst, rofi | [tema-claro-escuro.md](tema-claro-escuro.md) |
| Tabbed Dock (menu, ícone, início) | Implementado e testado | Implementado e testado | Login simulado com o gerador de autostart real; meson test 7/7 | [tabbed-dock-preferencias.md](tabbed-dock-preferencias.md) |
| Zoom (Ctrl+Alt+rolagem, Ctrl+Alt+0) | Implementado, pendente de teste real | Implementado, pendente de teste real | Laboratórios: GTK, Qt, cliques coerentes, 2 saídas, gles2; CPU 0 em repouso | [zoom-acessibilidade.md](zoom-acessibilidade.md); X11 sem cursor ampliado |
| Display Resolution | Implementado e testado (Xorg dummy) | Implementado e testado (aninhado) | 62 testes; reversão em 15–18 s com a janela travada/morta; perfis; hotplug por eventos; detecção dos 3 monitores reais conferida (só leitura) | [display-resolution.md](display-resolution.md); HDR/VRR sem controle falso |

## Validação na máquina real (antes de pedir o novo login)

- `big-wm-display-resolution --show` na sessão real: os três monitores com
  identidade EDID e posições corretas; nenhum perfil (nada é aplicado no login).
- Simulação do login numa cópia da conta: `big-wm-firstrun` (Wayland e X11)
  acrescentou os atalhos do zoom e as entradas de menu (Preferências do Dock
  com Abas, Monitores); o wlmaker 26 instalado iniciou com essa configuração
  sem erro.
- Testes automáticos: big-wm `tests/run.sh` (todas as verificações), wlmaker
  `ctest` 28/28 (o `wlmaker_metainfo_test` depende da rede: URLs
  inalcançáveis no momento), wmaker `make -C test check`, Display Resolution
  `make check` 62 testes.

## Limitações e pendências reais

- Testes na sessão real depois do novo login: Steam com mouse físico, zoom
  em monitores reais, arrastar appicons, wmcliphist.
- X11 (Window Maker real) não foi usado nesta rodada pelo usuário: as
  mudanças do wmaker foram testadas no Xvfb.
- Zoom no X11 depende do picom compondo; o cursor não é ampliado.
- HDR/VRR: só detecção (EDID) e o que o compositor aceitar; validar em
  hardware.
- Mapear 6 janelas X no mesmo instante perde às vezes uma (o wlroots/XWayland
  não emite o `map`; igual na versão anterior).
- O host reiniciou em 09/11 às 02:17 por um `systemctl reboot` da sessão do
  usuário, de origem não identificada (sessão, VSCodium e laboratórios
  compartilham a `session-1.scope`).

## Branches (locais, não publicadas)

- wlmaker: `fix/xwl-toplevel-geometry` (inclui `big-wm/missao-2026-11`,
  zoom, appicons, monitores desligados, wmcliphist, `2ac7086`, `de57e2e`).
- wmaker: `fix/x11-borderless-games` (inclui appicons).
- big-wm: `feat/missao-integracao` (inclui `feat/big-wm-welcome`, tema,
  zoom, appicons, monitores, dockapps).
- big-wm-display-resolution: `main` (repositório local, sem remoto).
- packages: `fix/dockapps-wayland-2026-11` (big-dockapps 10); os PKGBUILDs
  do wlmaker 26 e windowmaker 17 estão em `~/.cache/bwm-pkg`.
- iso-profiles: `feat/wmaker-apps` (lista de aplicativos).
