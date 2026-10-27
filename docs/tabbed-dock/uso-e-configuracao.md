# Compilar, instalar, configurar

## Compilar

Dependências (nomes do Arch/Manjaro): `meson gtk3 gtk-layer-shell wayland
wayland-protocols libx11 librsvg gettext`.

```sh
meson setup build tabbed-dock --prefix=/usr     # -Dx11=disabled / -Dwayland=disabled opcionais
meson compile -C build
meson test -C build                             # testes unitários do núcleo
```

Ou pelo Makefile do big-wm: `make tabbed-dock` e
`make install-tabbed-dock DESTDIR=…`. O `pkgbuild/PKGBUILD` gera o pacote
`big-wm-tabbed-dock` (x86_64) junto com os demais (arch=any).

## O que é instalado

| Caminho | Para quê |
|---|---|
| `/usr/bin/big-wm-tabbed-dock` | o dock |
| `/etc/xdg/autostart/big-wm-tabbed-dock.desktop` | início automático, `OnlyShowIn=WindowMaker;wlmaker;` (nunca em Plasma/XFCE/GNOME) |
| `/usr/lib/systemd/user/app-big\x2dwm\x2dtabbed\x2ddock@autostart.service.d/big-wm.conf` | `Restart=on-failure`, `KillMode=process` |
| `/usr/share/applications/big-wm-tabbed-dock-preferences.desktop` | "Preferências do Dock com Abas" no menu |
| `/usr/share/locale/pt_BR/LC_MESSAGES/big-wm-tabbed-dock.mo` | tradução |

Nada vai para `/etc/skel` e nenhuma configuração existente é alterada na
instalação.

## Escolher os docks da sessão

```sh
big-wm-tabbed-dock --dock-mode=status    # x11=both wayland=both
big-wm-tabbed-dock --dock-mode=tabbed    # só o dock com abas
big-wm-tabbed-dock --dock-mode=classic   # só o Dock clássico
big-wm-tabbed-dock --dock-mode=both      # os dois (padrão depois de instalar)
big-wm-tabbed-dock --dock-mode=none
```

Também em Preferências › "Docks na sessão". Vale a partir do próximo login
(no X11 o Window Maker lê `DisableDock` ao iniciar). O conteúdo do Dock
clássico (`~/GNUstep/Defaults/WMState`, `~/.config/wlmaker/State.plist`) nunca
é tocado. **Reverter tudo à mão**: `wdwrite WindowMaker DisableDock NO` e
apagar `~/.config/autostart/big-wm-tabbed-dock.desktop` e
`~/.config/autostart/big-wm-wayland-dock.desktop` (só os que têm
`X-Big-Tabbed-Dock-Managed=true`; um arquivo seu foi renomeado para
`*.user-backup`).

## Linha de comando

```
big-wm-tabbed-dock [--config ARQUIVO]   inicia (ou repassa as opções ao dock em execução)
  -t, --tab next|prev|N|ID              troca de aba
  -p, --preferences                     preferências
      --reload                          relê o arquivo (também: kill -HUP)
  -q, --quit                            grava e encerra
      --dock-mode MODO                  ver acima
      --print-state                     estado em texto (usado pelos testes)
  -v, --version
BTD_TIMING=1                            mostra o tempo até o primeiro quadro
```

## Atalhos no Wayland

O wlmaker não deixa uma camada pedir o teclado sob demanda, então as teclas
ficam com o compositor. Contas novas já recebem (em
`~/.config/wlmaker/Config.plist`, `KeyBindings`):

```
"Logo+Alt+1" = (ShellExecute, "big-wm-tabbed-dock --tab 1");   … até 9
"Logo+Alt+Page_Up" = (ShellExecute, "big-wm-tabbed-dock --tab prev");
"Logo+Alt+Page_Down" = (ShellExecute, "big-wm-tabbed-dock --tab next");
```

Para uma conta antiga, copie essas linhas para o seu `Config.plist`. No X11 o
dock captura `Super+Alt+1…9` e `Super+Alt+PgUp/PgDn` sozinho (desligável nas
preferências; uma tecla já usada por outro programa é pulada e registrada no
log).

## Arquivo de configuração

`~/.config/big-tabbed-dock/dock.conf` (respeita `XDG_CONFIG_HOME`), gravado
pelo dock; pode ser editado com o dock fechado ou seguido de `--reload`.
Exportar/Importar nas preferências usa o mesmo formato (a importação é
validada antes de aplicar e o estado anterior fica no `.bak`).

```ini
[Dock]
Version=2
Position=bottom          # bottom | top
Alignment=fill           # fill (padrão, de ponta a ponta) | center | left | right
Monitor=primary          # primary | nome (HDMI-A-1, DP-1…) | índice (0, 1…)
IconSize=48              # 24…96
AutoHide=false
ReserveSpace=true
Animations=true
ShowRunning=true
ClickMinimizes=true
GlobalShortcuts=true     # só X11
Style=classic            # classic (OPENSTEP 4.0 beta) | tiles (ladrilhos do Window Maker) | modern
ShowLabels=true          # nomes sob os ícones
ActiveTab=general
TabOrder=general;developer;documents;temporary;custom-1;

[Tab custom-1]
Builtin=false
Label=Jogos

[Item general 000]
Type=app                 # app | file | folder | appimage | separator
Target=org.gnome.Calculator.desktop
Icon=accessories-calculator   # opcional: nome do tema ou caminho absoluto
Label=Contas                   # opcional
```

Rótulo vazio numa aba padrão = nome traduzido. Um arquivo da versão 1 que
ainda tem `Alignment=center` (o padrão antigo, sempre gravado) passa a
`fill` ao ser lido; a partir da versão 2 a escolha é respeitada. Recuperação: um arquivo
ilegível vira `dock.conf.broken-<data>` e o `dock.conf.bak` é usado; sem
backup, abas padrão (as preferências do `.bak` são preservadas quando ele
existe). Restaurar o padrão: menu da aba › "Restaurar abas padrão…" (ou
"Restaurar conteúdo padrão…" de uma aba); preferências › Redefinir (posição e
comportamento, mantém as abas).

## Desinstalar

`pacman -R big-wm-tabbed-dock`. Opcionalmente `big-wm-tabbed-dock
--dock-mode=classic` antes, para religar o Dock clássico onde ele foi
desligado. `~/.config/big-tabbed-dock/` fica (dados do usuário).
