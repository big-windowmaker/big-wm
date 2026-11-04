# Relatório — Prompt Mestre de novembro de 2026 (UX/UI, Calamares, X11/Wayland)

Ambiente de teste: esta máquina (Manjaro, kernel 6.18, UEFI, 3 monitores
3440x1440/2560x1080/3440x1440, AMD). Sem `sudo`: nada foi instalado no
sistema; os testes gráficos rodaram em Xvfb (X11) e em wlmaker headless
(wlroots + pixman, Wayland) num `XDG_RUNTIME_DIR` próprio, nunca na tela
real. A VM "Big WindowMaker" do libvirt existe, mas sem credenciais de SSH
nem a senha do `sudo` dela: nenhum teste em VM, ISO ou instalação.

## Resultados por funcionalidade

| Funcionalidade | X11 | Wayland | Teste realizado | Resultado |
|---|---|---|---|---|
| Ponteiro que some ao sacudir | servidor X desenha o cursor grande | não implementado (precisa do compositor) | Xvfb + picom: 24→72→24 px, SIGTERM, SIGKILL, `--show` sem daemon (tests/run.sh) | Concluído e testado (X11) |
| Atalhos Super+R, Alt+Espaço, Ctrl+Shift+Esc, Ctrl+Alt+Esc, Print | menu raiz + teclas Print livres no WM | Config.plist do wlmaker | migrações e plists (tests/run.sh); teclas reais não pressionadas | Implementado, aguardando teste real das teclas |
| Spectacle no Print | `--fullscreen/--region/--activewindow` | só tela inteira (portal) | Xvfb + Window Maker: as três janelas abrem | Concluído e testado (X11); Wayland não validado |
| Apps QML/Kirigami com `QT_STYLE_OVERRIDE=kvantum` | estilo QML "kvantum" (Fusion) | mesmo ambiente | Spectacle fechava; com o módulo, abre | Concluído e testado |
| Gerenciador de tarefas | xfce4-taskmanager (instância única conferida) | mesmo | Xvfb | Concluído e testado |
| Firefox: Netscape sem bloquear temas | `-moz-pref(extensions.activeThemeID)` | mesmo | Firefox 159 em Xvfb: tema padrão = Netscape, tema escuro = sem Netscape; migração de perfis (teste) | Concluído e testado |
| WebApps no Firefox | WebApps do /etc/skel convertidos sem Brave | mesmo | WhatsApp do skel real abre no Firefox em modo app | Concluído e testado (X11) |
| Painel Informações | créditos à esquerda, "Copyleft 🄯 Nome" | — | wmaker compilado, captura no Xvfb | Concluído e testado |
| Ícones do Dock (margem, suavização, sombra) | patch 0014: 3/4 do ladrilho, média de área com alfa, sombra do clip.tiff | ContentSize 48 (sem sombra) | capturas antes/depois (OBS, Firefox, Thunar...) e wlmdock headless | Concluído e testado; sombra no Wayland não implementada |
| Pixmaps vazio | normal (wmaker.inst também cria vazio); pasta criada | — | log do wmaker.inst no Xvfb | Concluído |
| Resolução do GRUB | `grub-resolucao` + serviço de primeiro boot | — | 12 cenários com EDID/sysfs sintéticos; `--listar` real nesta máquina | Implementado; boot real BIOS/UEFI não testado |
| Kate e Spectacle na ISO | Packages-Desktop | — | Thunar continua com `inode/directory` | Implementado, ISO não gerada |
| WMeyes | dockapp + menu + ISO | não suportado (Wayland não expõe o ponteiro global) | Xvfb: olhos seguem o ponteiro | Concluído (X11); build completo do split fica para o CI |
| Zoom da tela inteira (Ctrl+Alt+roda) | — | — | — | Não implementado: no X11 exige compositor próprio ou transformação de CRTC, que não dá para validar em Xvfb; precisa da VM/hardware real |
| Calamares retrô (sidebar, páginas, animação) | — | — | — | Não implementado nesta rodada |
| VSCodium Insider | — | — | — | Não implementado nesta rodada |
| AshyTerm | patch da rodada anterior (big-comm/ashyterm#60) | — | — | Sem mudanças nesta rodada |

## Causas encontradas

- **Ponteiro sumindo:** a janela do ponteiro grande entrava com o fade do picom
  (~200 ms) e, sobre janela em tela cheia, forçava o picom a redirecionar a
  tela (`unredir-if-possible`); o cursor real já estava escondido. O estado
  "tem compositor" ficava guardado para sempre. Na troca para cursor do
  servidor apareceu outro problema real: com a extensão SECURITY, um cursor
  criado por um processo que saiu fica ilegível para todos
  (`XFixesGetCursorImage` → BadAccess, Xext/security.c:773). Por isso só o
  daemon troca cursores.
- **Spectacle (e outros apps QML) fechando:** `QT_STYLE_OVERRIDE=kvantum`
  também escolhe o estilo QML, e o Kvantum não tem um.
- **Firefox:** nenhuma política travava o tema; as regras `!important` do
  `userChrome.css` venciam qualquer tema, e o `chrome/` apagado voltava no
  login seguinte.
- **WebApps:** o `/etc/skel` do biglinux-webapps chama o Brave; o
  `auto-tweaks-browser` instala um `/usr/local/bin/brave` mesmo sem o Brave.
- **Ícones:** `RScaleImage` (vizinho mais próximo) e nenhuma margem acima de
  67 px; `RSmoothScaleImage` do wrlib descarta o alfa.

## Commits (ramos locais `feat/mestre-2026-11`, não enviados)

- big-wm: localizar o ponteiro; estilo QML kvantum; atalhos e ícones;
  Firefox e WebApps; grub-resolucao; Pixmaps; wmeyes no menu.
- wmaker (`big-wm/fixes-2026-11`): painel Info; ícones.
- packages: windowmaker 10 (patches 0013/0014, compilado com makepkg);
  big-dockapps 7 (wmeyes).
- iso-profiles: Kate, Spectacle, serviço do GRUB, wmeyes.

`tests/run.sh` do big-wm: 34 verificações, todas passando (com Xvfb).

## Pendências reais

- Testes em VM (instalação BIOS/UEFI, sessão Wayland instalada, teclas de
  atalho reais, GRUB no boot): precisam do acesso à VM.
- Zoom global, Calamares retrô, VSCodium: não implementados.
- Sombra dos ícones e ponteiro grande no wlmaker: exigem patch no
  compositor.
