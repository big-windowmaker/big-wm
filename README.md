# Big WindowMaker

Desktop Window Maker do BigLinux/BigCommunity: sessões X11 e Wayland, configuração
padrão, temas e integração com o sistema.

Este repositório gera três pacotes:

| Pacote | Conteúdo |
|---|---|
| `big-wm-session` | Sessão **Big WindowMaker — X11** (padrão, estável): wrapper de sessão, configuração padrão do Window Maker, integração com systemd, notificações, polkit, bandeja e bloqueio de tela |
| `big-wm-wayland` | Sessão **Big WindowMaker — Wayland (experimental)** com o compositor nativo [wlmaker](https://github.com/phkaeser/wlmaker) |
| `big-wm-themes` | Temas e estilos do Window Maker, ícones e papéis de parede |

## Como a sessão funciona

```
LightDM ──▶ startbigwm (X11)            ──▶ wmaker
        └─▶ startbigwm-wayland (Wayland) ──▶ wlmaker --start_xwayland
                │
                ├─ big-wm-firstrun        copia os padrões de /usr/share/big-wm
                │                         para ~/GNUstep ou ~/.config/wlmaker,
                │                         sem sobrescrever nada do usuário
                └─ big-wm-session-ctl     exporta o ambiente gráfico e inicia
                                          big-wm-session.target (systemd --user)
                                            ├─ graphical-session.target
                                            └─ xdg-desktop-autostart.target
                                                 ├─ big-wm-polkit        (polkit-gnome)
                                                 ├─ big-wm-notifications (dunst)
                                                 ├─ big-wm-tray          (snixembed, X11)
                                                 ├─ big-wm-locker        (xss-lock + i3lock, X11)
                                                 └─ autostart dos apps   (nm-applet, picom…)
```

Princípios:

- **Nenhum binário de outro pacote é alterado.** O `/usr/bin/wmaker` é chamado como está.
- **Nada em `/etc/skel`.** A configuração padrão é aplicada no primeiro login,
  também para contas que já existiam. `big-wm-firstrun --reset x11` restaura os
  padrões e guarda a configuração anterior em `*.bak-<data>`.
- **Convive com Plasma, XFCE e GNOME.** Todo autostart instalado aqui tem
  `OnlyShowIn=WindowMaker;` e/ou `wlmaker;`. Os testes garantem isso, e também
  que nenhum arquivo é instalado fora dos caminhos próprios do projeto.
- **Sem scripts de instalação no pacote.** Nada roda como root durante a instalação.

O `picom` é opcional porque o pacote dele instala um autostart que vale para
qualquer ambiente X11, inclusive o Plasma.

## Atalhos

| Ação | X11 | Wayland |
|---|---|---|
| Menu raiz | F12 / clique direito na área de trabalho | F12 / Ctrl+Esc |
| Buscar aplicativos | Super+Espaço | Super+Espaço |
| Terminal | Ctrl+Alt+T | Ctrl+Alt+T / Super+Enter |
| Arquivos | Super+E | Super+E |
| Bloquear tela | Super+L | Super+L / Ctrl+Alt+L |
| Captura da tela / janela / área | Print / Alt+Print / Shift+Print | Print / — / Shift+Print |
| Fechar janela | Alt+F4 | Alt+F4 |

## Desenvolvimento

```sh
make check                       # testes estáticos (tests/run.sh)
make install DESTDIR=/tmp/teste  # instalação de teste
cd pkgbuild && BIGWM_SOURCE="git+file://$PWD/.." makepkg   # pacotes locais
```

A sessão Wayland é **experimental**: o wlmaker 0.8 ainda não tem arrastar e soltar
entre aplicativos, xdg-activation, métodos de entrada nem bandeja do sistema.
Veja `docs/` no repositório [big-windowmaker/.github](https://github.com/big-windowmaker/.github)
para a decisão de arquitetura.

## Licença

GPL-2.0-or-later. Veja `LICENSE` e `CREDITS.md`.
