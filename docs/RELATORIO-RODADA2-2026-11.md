# Relatório — rodada 2 de novembro de 2026 (regressões, menus, Dock, acessibilidade)

Instalado na máquina de desenvolvimento em 09/11/2026: wlmaker 27,
big-wm 26.11.09-0537 (11 pacotes), big-wm-display-resolution 26.11.09-0537.
Nada publicado (sem push, PR ou ISO).

| Funcionalidade | X11 | Wayland | Evidência | Documento |
|---|---|---|---|---|
| Preferências do Dock com Abas em Configurações | Concluído e testado | Concluído e testado | login simulado numa cópia da conta real; testes de migração | [menu-configuracoes.md](menu-configuracoes.md) |
| Localizar o ponteiro (o próprio cursor ampliado) | Concluído e testado | Implementado, pendente de teste em hardware real | laboratório: cursor padrão, I-beam do XWayland, cursor próprio de app, 2 saídas com escalas 1 e 2; falta o plano de cursor de hardware DRM | [localizar-ponteiro.md](localizar-ponteiro.md) |
| Posição livre no Dock e no Clip | Concluído (já era livre) | Concluído e testado | inserção antes/depois de cada dockapp, reinício do wlmaker, coluna cheia | [appicons-dock-clip.md](appicons-dock-clip.md) |
| Rolagem do Display Resolution | Concluído e testado | Implementado, conferência visual pendente | 66 testes; laboratório X11 com roda, touchpad, teclado | [display-resolution.md](display-resolution.md) |
| Diálogos de encerramento com o ícone do app | Concluído e testado | Concluído e testado | laboratório claro/escuro | [dialogos-encerramento.md](dialogos-encerramento.md) |
| Ctrl+Alt+Esc com caveira vermelha | Concluído e testado (`big-wm-xkill`) | Concluído e testado (`ForceCloseSelect`) | app travado encerrado, vizinho e compositor vivos; Esc/botão direito cancelam; componentes da sessão recusados | [cursor-caveira.md](cursor-caveira.md) |
| Menu Configurações reorganizado | Concluído e testado | Concluído e testado | migração de menus editados, pt_BR/inglês | [menu-configuracoes.md](menu-configuracoes.md) |
| Lupa × escala | Lupa = wmagnify; Aumentar/Diminuir Escala pelo picom | Lupa = ZoomIn (não há lupa real no Wayland); Aumentar/Diminuir Escala | laboratórios | [zoom-acessibilidade.md](zoom-acessibilidade.md) |
| ARandR/wdisplays fora do padrão | Concluído | Concluído | PKGBUILD, ISO, Welcome, conversor | [aplicativos-padrao.md](aplicativos-padrao.md) |
| Welcome: Adicionar/remover programas | Concluído e testado | Concluído e testado | testes do Welcome | [welcome.md](welcome.md) |
| Spectacle no monitor do ponteiro | Já correto | Concluído e testado | teste unitário com 3 saídas; laboratório com 3 saídas | [spectacle-multimonitor.md](spectacle-multimonitor.md) |
| Onboard no Dock | Concluído e testado | Ícone corrigido; o Onboard não digita no Wayland (limitação) | dois defeitos que apagavam/duplicavam corrigidos | [onboard-dock.md](onboard-dock.md) |
| HP Device Manager / HP Scan | Implementado, pendente de impressora real | idem | falta o `python-pyqt5` na máquina: janela explica | [aplicativos-hp-scrcpy-uxplay.md](aplicativos-hp-scrcpy-uxplay.md) |
| scrcpy | Implementado, pendente de celular real | idem | estados do adb simulados | idem |
| UxPlay (Receptor AirPlay) | Implementado, pendente de iPhone real | idem | Avahi desligado e ufw sem as portas na máquina: botão "Configurar rede..." | idem |
| Big Video Player, BigCam padrão; mpv/guvcview/rofi/scrcpy console ocultos | Concluído e testado | Concluído e testado | ajustes de .desktop só no menu do Big WindowMaker | [aplicativos-padrao.md](aplicativos-padrao.md) |
| BigLinux Microfone na bandeja | Concluído e testado | Concluído e testado | laboratório; estado real conferido só leitura | [systray-microfone.md](systray-microfone.md) |
| Ícone big-wm-magnify | Concluído | Concluído | — | [menu-configuracoes.md](menu-configuracoes.md) |

## Validação na máquina (antes do novo login)

- Simulação do login numa cópia da configuração (wlmaker, big-wm, GTK,
  Kvantum, autostart, GNUstep): `big-wm-firstrun` e `big-wm-wprefs-sync`
  produziram o menu novo nas duas sessões (Configurações completo, Dock com
  Abas, Monitores, Lupa, escala, Forçar o fechamento com a caveira) e os
  atalhos (Ctrl+Alt+rolagem, Ctrl+Alt+0, Ctrl+Alt+Esc).
- wlmaker 27 instalado, iniciado com essa configuração, Steam real com GPU:
  12 arrastes sem divergência, conteúdo na moldura.
- Testes: big-wm `tests/run.sh` (todas as verificações), wlmaker `ctest`
  28/28, Display Resolution `make check`.
- libbase: a correção dos pipes de subprocessos agora entra no pacote (patch
  aplicado no submódulo); no wlmaker 26 o ponteiro do submódulo não tinha
  efeito.

## Pendências

- Decisões do usuário: squeekboard como teclado virtual do Wayland; ligar o
  avahi-daemon e abrir as portas do AirPlay (botão "Configurar rede...").
- `wlmaker_test` às vezes trava esperando um subprocesso do teste do menu
  raiz (1 em 5 execuções; também com a correção dos pipes); na sessão real os
  geradores terminam e os menus são preenchidos.
- Testes em hardware real: cursor ampliado no plano de cursor DRM, zoom nos
  monitores, impressora HP, celular Android, iPhone.
