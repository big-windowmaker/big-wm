# Paridade X11 ↔ Wayland

Matriz viva da sessão **Big WindowMaker — Wayland** (wlmaker) comparada com a
sessão **X11** (Window Maker), que é a referência. Cada linha diz o que foi
testado e onde. As evidências vêm da VM de testes (QEMU/KVM, virtio-gpu sem
3D, BIOS, sistema instalado a partir da ISO de 2026-10-26 e atualizado com os
pacotes desta versão), do teste de integração do CI (`tests/integration/`,
contêiner Manjaro com wlmaker headless) e do pipeline da ISO
(`big-windowmaker/build-iso`, run 40385383947: smoke test e instalação em
BIOS e UEFI).

**Status:** ✅ funciona e foi testado · 🔧 funciona com diferença ou sem teste
automático · ⏳ em correção (link para o item) · ❌ limitação conhecida, com
alternativa · ❓ pendente de hardware real.
**Prioridade:** P0 bloqueia uso/segurança · P1 quebra fluxo essencial ·
P2 consistência · P3 polimento.

Como reproduzir os testes: [wayland-test-lab.md](wayland-test-lab.md).

## Sessão e login

| ID | Recurso | X11 | Wayland | Solução / arquivo | Teste | Status |
|---|---|---|---|---|---|---|
| L1 | Entrar pelo greeter (slick-greeter) | ✅ | ✅ | `startbigwm-wayland` | VM instalada (captura) | ✅ |
| L2 | Autologin na sessão Wayland | ✅ | sem o grupo `autologin` o PAM recusa e o LightDM entra em laço tentando um greeter X11 num servidor "wayland" | o live usa a sessão `livecd` (X11); autologin Wayland exige o grupo `autologin` (padrão do Arch) | VM: `lightdm.log` | ❌ P2 (LightDM) |
| L3 | Saber qual sessão está escolhida no greeter | igual | igual | emblema próprio `big-wm-wayland.svg` com selo "W" | — | ✅ |
| L4 | Ambiente no `systemd --user` (`WAYLAND_DISPLAY`, `XDG_SESSION_ID`, `XDG_SESSION_TYPE`) | ✅ | ✅ | `big-wm-session-ctl` | VM: `systemctl --user show-environment` | ✅ |
| L5 | Novo login depois de uma sessão encerrada sem "stop" (X caiu, LightDM reiniciado) | autostart não subia | idem | `big-wm-session-ctl start` limpa o ambiente velho e reinicia o target | VM: 4 reinícios seguidos | ✅ |
| L6 | Atualização de quem já usava a versão anterior | menu e canto ativo migrados | menu e `Config.plist` migrados | `big-wm-firstrun` (troca só linhas antigas; menu idêntico ao modelo antigo vai para `.bak-*`) | `tests/run.sh`; VM (log `big-wm-firstrun`) | ✅ |
| L7 | Não afetar Plasma/XFCE/GNOME | ✅ | ✅ | autostarts com `OnlyShowIn`; drop-ins liberam tudo fora das sessões Big WindowMaker | `tests/run.sh` | ✅ |

## Menus e aplicativos

| ID | Recurso | X11 | Wayland | Solução / arquivo | Teste | Status |
|---|---|---|---|---|---|---|
| M1 | Menu raiz com a mesma estrutura | ✅ | ✅ (mesmos itens e submenus) | `data/wlmaker/RootMenu.plist*` | VM: F12 + navegação por teclado (captura) | ✅ |
| M2 | Aplicativos por categoria, traduzidos | ✅ (refeito a cada abertura) | ✅ (gerado no início da sessão; o wlmaker carrega o menu uma vez) | `big-wm-appmenu --wlmaker` (`GeneratePlistMenu`) | `tests/run.sh`; CI | 🔧 P3: app instalado durante a sessão só aparece no próximo login |
| M3 | Clique direito rápido na área de trabalho | abre o menu | abre o menu (clique com menos de 3 px de movimento não aciona item; arrastar até o item aciona) | wlmaker patch 0015 | teste unitário do wlmaker | ✅ |
| M4 | Buscar aplicativos (Super+Espaço) | ✅ rofi com o tema | ✅ rofi com o tema (nativo Wayland) | `big-wm-open apps` | VM | ✅ |
| M5 | Executar... | ✅ | ✅ | `big-wm-open run` | — | 🔧 |
| M6 | Papéis de parede | ✅ `wmsetbg` | ✅ `swaybg`, escolha guardada | `big-wm-wallpaper`, `big-wm-appmenu --wallpapers` | `tests/run.sh` | 🔧 |
| M7 | Temas e estilos (Aparência) | ✅ | ❌ um tema só (`Theme.plist`) | wlmaker tem `ThemeLoadFromFile`; faltam temas convertidos | — | ❌ P3 |
| M8 | Preferências (WPrefs) | ✅ | ❌ não existe ferramenta equivalente no wlmaker | roadmap upstream ("Configuration tool") | — | ❌ P2 |
| M9 | Aplicativos X11 (XWayland) | nativo | ✅ | `--start_xwayland` | CI (`xeyes`) | ✅ |

## Dock, Clip e bandeja

| ID | Recurso | X11 | Wayland | Solução / arquivo | Teste | Status |
|---|---|---|---|---|---|---|
| D1 | Lançadores no Dock | ✅ | ✅ | `data/wlmaker/State.plist` | VM | ✅ |
| D2 | Indicador de aplicativo em execução no Dock | ✅ | 🔧 `wlmdock` marca o lançador que tem `AppID`; os lançadores genéricos (`big-wm-open`) não têm | `wlmdock` (autostart `big-wm-wayland-dock`) | — | 🔧 P3 |
| D3 | Dockapps (relógio/calendário, CPU, memória, rede, bateria) | ✅ | ✅ nativos (`wlmcalclock`, `wlmcpugraph`, `wlmmemgraph`, `wlmnetgraph`; `wlmbattery` só quando existe bateria), ladrilhos abaixo do Clip | `big-wm-dockapps` (lista em `~/.config/big-wm/dockapps`) | VM: sessão real (captura 26/10) | ✅ |
| D4 | Dockapp de volume | ✅ wmix | ✅ `wlmmixer` (nativo, PulseAudio/PipeWire) | patch do wlmaker (big-wm-packages) | VM: sessão real (captura 26/10) | ✅ |
| D5 | Dockapps X11 (wmshutdown, wmusic, pclock...) | ✅ | ❌ não acoplam; o wmshutdown terminava com SIGSEGV aberto pelo menu | `NotShowIn=wlmaker;` nos .desktop (big-dockapps pkgrel 3); Aparência > Dockapps lista só os nativos | VM (coredump) | ✅ fora dos menus |
| D6 | Bandeja (StatusNotifierItem): rede, atualizações, Bluetooth | ✅ wmsystemtray + snixembed | ✅ `big-wm-tray` (ladrilho no canto inferior esquerdo; clique, menu dbusmenu, rolagem) | `session/bin/big-wm-tray` | VM: menu do nm-applet (captura); CI | ✅ |
| D7 | Ícones XEmbed antigos (GtkStatusIcon) | ✅ | ❌ não existe XEmbed no Wayland | apps atuais usam SNI | — | ❌ P3 |
| D8 | Ícones de janelas minimizadas | ✅ (com ícone de reserva) | ❌ wlmaker 0.8 não iconifica | roadmap upstream 0.9 ("Icon area") | — | ❌ P2 |
| D9 | Clip no canto superior esquerdo com Navegador e Arquivos fixos | ✅ | ✅ `wlmdock --clip` (`Clip.Launchers` no `State.plist`) | autostart `big-wm-wayland-clip` com `WLR_RENDERER=pixman` (o wlmdock caía no nó DRM e ficava invisível) | VM: sessão real (captura 26/10) | ✅ |
| D10 | Menu do botão direito no Dock, Clip e dockapps (como no X11) | ✅ | 🔧 em desenvolvimento no compositor | patches do wlmaker | — | 🔧 P1 |

## Janelas

| ID | Recurso | X11 | Wayland | Solução / arquivo | Teste | Status |
|---|---|---|---|---|---|---|
| W1 | Barra de título do Window Maker em apps GTK 3/4 e libadwaita | ✅ gtk-nocsd | ✅ gtk-nocsd + protocolo de decoração do KDE | wlmaker patch 0002; `big_wm_nocsd` em `big-wm-env.sh` (só com wlmaker que tem o protocolo) | VM, wlmaker headless: gnome-calculator e gedit (capturas) | ✅ |
| W2 | Barra de título em apps Qt | ✅ | ✅ (`xdg-decoration`) | — | CI | ✅ |
| W3 | Alt+Tab, maximizar, tela cheia, fechar, mover entre áreas | ✅ | ✅ | `Config.plist` | — | 🔧 |
| W4 | Ativação de janela (abrir app já aberto, links) | ✅ | ✅ `xdg-activation-v1` (na área de trabalho atual) | wlmaker patch 0001 | cliente de teste no wlmaker headless; CI | ✅ |
| W5 | Diálogos de arquivo do portal presos à janela | ✅ | 🔧 `xdg-foreign` v1/v2 anunciado; o wlmaker ainda não aplica `set_parent` | wlmaker patch 0008 | — | 🔧 P2 |
| W6 | Métodos de entrada (IBus/Fcitx5) | ✅ | ❌ sem `text-input`/`input-method` no wlmaker | — | CI (XFAIL) | ❌ P2 |

## Bloqueio, energia e serviços

| ID | Recurso | X11 | Wayland | Solução / arquivo | Teste | Status |
|---|---|---|---|---|---|---|
| S1 | Bloquear (menu, Super+L) com a imagem do Big WindowMaker | ✅ i3lock | ✅ swaylock (`ext-session-lock`) | `big-wm-lock` | VM: bloquear e desbloquear com a senha (capturas) | ✅ |
| S2 | `loginctl lock-session` | ✅ xss-lock `--session` | ✅ | `big-wm-lock --watch` | VM | ✅ |
| S3 | Bloquear antes de suspender | ✅ (`XSS_SLEEP_LOCK_FD`) | ✅ swayidle `-w before-sleep` (o wlmaker agora tem `ext-idle-notify`) | wlmaker patch 0009; `big-wm-lock --watch` | swayidle no wlmaker headless | 🔧 sem teste de suspensão real |
| S4 | Bloqueio com um menu (popup) aberto | ✅ | ✅ o bloqueio encerra as capturas (popup_done) e o teclado vai para o swaylock | wlmaker patches 0004 e 0005 | teste unitário; swaylock + PAM no wlmaker headless | ✅ (confirmar na VM) |
| S5 | Recuperar bloqueio se o locker morrer | — | ✅ outro locker assume o bloqueio abandonado | wlmaker patch 0003 | teste unitário; swaylock no wlmaker headless | ✅ (confirmar na VM) |
| S6 | Notificações | ✅ dunst | ✅ dunst | — | CI | ✅ |
| S7 | Clipboard e seleção primária | ✅ | ✅ | — | CI | ✅ |
| S8 | Rede (nm-applet) | ✅ | ✅ pela bandeja | — | VM | ✅ |
| S9 | Polkit (senha de administrador) | ✅ | ✅ | autostart `big-wm-polkit` | — | 🔧 |
| S10 | Monitores | ✅ arandr | 🔧 wdisplays (agora dependência) | `pkgbuild/PKGBUILD` | — | 🔧 |
| S11 | Captura de tela e área | ✅ | ✅ grim/slurp | `big-wm-screenshot` | CI | ✅ |
| S12 | Captura de janela | ✅ Alt+Print | ⏳ o wlmaker 0.9 tem `ext-image-copy-capture`; falta escolher a janela para o `grim -T` | — | — | ⏳ P2 |
| S13 | Compartilhar tela (portal ScreenCast) | ✅ | ❓ o portal wlr exige `linux-dmabuf` (GPU) | `wlmaker-portals.conf` | CI: SKIP sem GPU | ❓ |

## Desempenho

Sessão logo após o login, cache descartado, PSS (método em
[memoria.md](memoria.md)):

| | X11 | Wayland |
|---|---|---|
| PSS dos processos do usuário | 191 MB (live) | 335 MB (instalado) |
| Gerenciador de janelas | wmaker 14 MB | wlmaker 82 MB (renderização por software na VM) |
| Bandeja | snixembed 6 MB | big-wm-tray 33 MB (Python + GTK 3) |
| XWayland | — | sob demanda com `--lazy_xwayland` (wlmaker patch 0017): ~35 MB a menos até o primeiro app X11 |

As medições não são do mesmo sistema (live x instalado), então servem de
ordem de grandeza. Para baixar mais: uma bandeja nativa em C no lugar da de Python.

## ISO

| Cenário | Resultado | Evidência |
|---|---|---|
| Live BIOS e UEFI (smoke test), sessão X11 | ✅ | build-iso run 40385383947 |
| Instalação Calamares BIOS e UEFI, login X11 no instalado | ✅ | build-iso run 40385383947 (a falha da UEFI na 1ª tentativa foi download de artefato do GitHub; nova tentativa passou) |
| Login Wayland no sistema instalado (BIOS) | ✅ | VM, entrando pelo greeter |
| Login Wayland no sistema instalado (UEFI) | ❓ | pendente |
| Login Wayland no live | ❓ | o live inicia direto na sessão X11 |
