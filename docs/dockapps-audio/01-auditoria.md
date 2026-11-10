# 01 — Auditoria (volume e música no Dock, novembro de 2026)

## Problema

O Dock devia mostrar, nas duas sessões, a bandeja e logo abaixo o volume
(`wmpulsemixer`) e o controle de música (`wmusic`), iniciados com a sessão,
acoplados e com todos os controles funcionando.

## O que havia

| Item | X11 (Window Maker) | Wayland (wlmaker) |
|---|---|---|
| Bandeja | `wmsystemtray0.wmsystemtray` (pelo `big-wm-systemtray`), posição 0,2 do `WMState` padrão | `big-wm-tray`, adotada pelo wlmaker (`Dock.Dockapps` do `State.plist`) |
| Volume | `wmix.wmix` (pelo `big-wm-mixer`, ALSA), posição 0,3 | `wmix` pelo XWayland (grupo `big-wm-mixer\|wlmmixer` do `big-wm-dockapps`) |
| Música | `wmusic` só no catálogo do menu Dockapps | `wmusic` no catálogo XWayland do menu (já estava), fora do padrão |
| `wmpulsemixer` | não empacotado | não empacotado |

- **Início com a sessão.**
  - X11: o `AutoLaunch` do `WMState`, mais o `big-wm-dockapps-autolaunch`, que liga o `AutoLaunch` de dockapps acoplados sem ele.
  - Wayland: o autostart `big-wm-wayland-dockapps.desktop` roda o `big-wm-dockapps`, que respeita a lista `Removed` e as listas `Dock`/`Order` do `~/.config/wlmaker/Dockapps.plist`.
- **Posição.**
  - X11: a `Position` de cada entrada. O Window Maker grava uma lista por altura de tela (`Applications1440`…) além de `Applications`.
  - Wayland: um dockapp ainda não posto vai para o lugar dele na lista `Dock.Dockapps` do `State.plist`, antes do primeiro já posto que vem depois nela (`clip.c`). Os já postos seguem o `Dockapps.plist`.
- **Migrações das contas existentes.** São feitas pelo `big-wm-firstrun`, uma vez cada uma (marcas em `~/.config/big-wm`). O `dock_add_once` não passa da altura da tela; um dockapp que não cabe fica sem marca e tenta no próximo login.
- **Big Screen Monitor Display.**
  - Volume do sistema por um `pactl subscribe` de longa duração, guiado por eventos (`bsmd/media/volume.py`).
  - Players por MPRIS no D-Bus, com `PropertiesChanged` (`bsmd/media/mpris.py`).
  - Não depende de nenhum dockapp.

## Verificado nos programas (não presumido)

- **WM_CLASS do wmpulsemixer.** `wmpulsemixer -w`: janela principal e janela de ícone `"wmpulsemixer", "WmPulseMixer"`, logo `wmpulsemixer.WmPulseMixer` no `WMState` e `wmpulsemixer` no wlmaker (instância do líder). Sem `-w` ele abre uma janela comum.
- **WM_CLASS do wmusic.** `"wmusic", "DockApp"` (libdockapp), logo `wmusic.DockApp`.
- **A letra `-w` muda de sentido.** No wmusic, `-w` é *janela comum* (`--windowed`), o contrário do wmpulsemixer; o Dock o inicia sem `-w`.
- **A conta real do usuário** já tinha um Dock reorganizado: wmix na posição 10 e bandeja na 12, nas duas listas (`Applications` e `Applications1440`). A migração tem de respeitar isso.

## Defeitos encontrados

1. **O wmpulsemixer saía quando o servidor de áudio caía ou ainda não existia** (`gtk_main_quit()` no estado FAILED do libpulse): ladrilho vazio até o próximo login.
2. **A roda do mouse num canal mudo** partia do volume mostrado (0) e punha 80% em 4%.
3. **Um clique no menu de dispositivos do wmpulsemixer também agia nos controles** nas mesmas coordenadas.
4. **Cada clique no wmpulsemixer** registrava `Gdk-WARNING: gdk_window_set_user_time called on non-toplevel`.
5. **O wmusic acordava 10 vezes por segundo o tempo todo**, listava os players no D-Bus e pedia a posição ao player a cada vez (≈10 chamadas/s ao player), e redesenhava o ladrilho inteiro mesmo parado.

Correções em [03-wmpulsemixer.md](03-wmpulsemixer.md) e [04-wmusic.md](04-wmusic.md).
