# Ctrl+Alt+Esc: a caveira vermelha (forçar o fechamento)

Ctrl+Alt+Esc → o ponteiro vira uma caveira vermelha → um clique (botão
esquerdo) numa janela encerra à força o cliente daquela janela → o ponteiro
volta ao normal. Esc, o botão direito (ou outro) e Ctrl+Alt+Esc de novo
cancelam. Nas duas sessões.

## Problema

No Wayland, Ctrl+Alt+Esc abria a janela GTK "Forçar o fechamento"
(`big-wm-kill`): uma lista de janelas, com o PID adivinhado pelo nome do
processo, pelo D-Bus ou pelo `.desktop` — sem o "aponte e clique" do xkill e
sem a certeza de pegar o processo certo. No X11, o `xkill` mostrava o
cursor fixo do tema e o Esc não cancelava.

## O cursor

`data/icons/big-wm-skull/`: `skull.svg` (caveira vermelha com ossos
cruzados, contorno escuro e halo claro: visível em fundos claros e escuros;
`evidencias/caveira-claro-escuro.png`) e `mkcursor.py`, que gera
`cursors/pirate` (formato Xcursor, 24 a 128 px, ponto ativo no centro, entre
os olhos e o nariz) com o `rsvg-convert`. O arquivo gerado fica no
repositório (o build não precisa do librsvg). Instalado como tema de cursor
`/usr/share/icons/big-wm-skull` pelo pacote big-wm-locate-pointer; o mesmo
arquivo serve ao wlmaker e ao X11. Tamanho moderado: o do cursor da sessão
(nas saídas com escala, a imagem do tamanho × escala).

## Wayland: no wlmaker (`src/force_close.c`)

- Ação nova `ForceCloseSelect`. Configuração:
  ```
  ForceClose = { CursorTheme = "big-wm-skull"; Cursor = "pirate"; };
  ```
  (sem o tema, o `pirate`, `X_cursor` ou `crosshair` do tema atual).
- A caveira substitui a imagem do cursor (`wlmim_set_cursor_override`, ver
  `localizar-ponteiro.md`); o que os clientes pedem fica guardado e volta no
  fim.
- Um filtro de botões roda antes do toolkit e dos clientes: o clique e a sua
  soltura não chegam a ninguém. O teclado fica com um *grab* só para o Esc
  (as outras teclas seguem normais; com a tela bloqueada, nada acontece).
- O clique: `wlr_scene_node_at` → a janela (`wlmtk_window`) que contém o nó
  (também no título e nas bordas) → a superfície do cliente:
  - **Wayland:** `wl_client_get_credentials` dá o PID da conexão; SIGKILL no
    PID e `wl_client_destroy` (a janela some mesmo se o processo estiver
    preso em E/S);
  - **XWayland:** `xcb_kill_client` na janela, pela conexão do XWM
    (`wlr_xwayland_get_xwm_connection`) — o XKillClient do xkill; o
    `wlr_xwayland_surface_close` não serve (manda WM_DELETE_WINDOW, que um
    programa travado ignora) — e SIGKILL no PID que o servidor X conhece
    (XRes, não o `_NET_WM_PID` escrito pelo próprio programa).
  Feito num *idle* do laço, fora dos tratadores de entrada.
- Recusados, com um aviso (diálogo do wlmaker com o ícone do aplicativo):
  o próprio wlmaker e os processos acima dele (a sessão), outro usuário,
  PID 1, Xwayland/Xorg, systemd*, dbus-*, pipewire*, wireplumber, portais,
  gnome-keyring, lightdm/sddm/gdm, picom/dunst/xsettingsd, os componentes
  `big-wm-*`, `wlm*` (wlmdock, wlmclock) e os dockapps `wm*` — pelo nome do
  processo, do executável e dos primeiros argumentos (scripts Python). A
  área de trabalho, o Dock, painéis e as janelas do próprio wlmaker
  (menus, diálogos) não são janelas de aplicativo: aviso também.
- Navegadores com vários processos: encerra o cliente da janela (o processo
  dono da conexão), nunca processos por nome; nada de `pkill`.

## X11: `big-wm-xkill` (`session/kill/big-wm-xkill.c`)

O xkill aceita `-button`, mas o cursor é fixo e o teclado não é capturado.
O `big-wm-xkill` faz o mesmo papel: `XGrabPointer` com a caveira do tema
`big-wm-skull` (reserva: a do tema atual), `XGrabKeyboard` para o Esc, e no
clique: o cliente dentro do quadro (a janela com `WM_STATE`, como
`XmuClientWindow`) → `XKillClient` e SIGKILL no PID do XRes. Recusa o
gerenciador (o Dock, o Clip, menus e ícones não têm cliente com
`WM_STATE`), dockapps (retirados, ou pelo nome) e os mesmos processos da
sessão, com uma notificação. Espera soltar o botão antes de sair (o clique
não chega ao programa de baixo). `big-wm-kill` no X11 chama o
`big-wm-xkill` (sem ele, o `xkill`).

## Arquivos

- wlmaker: `src/force_close.[ch]`, `src/input/*` (override, filtro de
  botões), `src/action.[ch]`, `src/server.[ch]`, `src/i18n.c`.
- big-wm: `data/icons/big-wm-skull/*`, `session/kill/big-wm-xkill.c`,
  `session/bin/big-wm-kill`, `Makefile` (`install-locate-pointer`),
  `pkgbuild/PKGBUILD` (libxres), `data/wlmaker/Config.plist.in`,
  `session/bin/big-wm-firstrun` (migração), `tests/run.sh`.

## Testes (laboratórios, só com apps descartáveis)

- Wayland (`evidencias/wl-caveira-*.png`, `wl-depois-de-encerrar-*.png`,
  `wl-aviso-*.png`):
  - app GTK Wayland travado de propósito (laço infinito, 100 % de CPU):
    Ctrl+Alt+Esc, clique na janela → processo encerrado, o xterm ao lado
    continua, cursor de volta ao normal, wlmaker vivo;
  - o mesmo app pelo XWayland, clique na barra de título → encerrado (log:
    "Closing ... (X11, PID ...)"), o outro cliente X do mesmo Xwayland
    continua;
  - Esc, botão direito e Ctrl+Alt+Esc de novo: cancelam (xterm vivo);
  - clique na área de trabalho: aviso "Isto não é a janela de um
    aplicativo"; app chamado `big-wm-fakeapp`: aviso "é um componente da
    sessão" (tema escuro);
  - com o cursor de hardware do backend também, sem erros no log.
- X11 (Xvfb + sessão Big WindowMaker do laboratório): `evidencias/x11-caveira-big-wm-xkill.png`
  (cursor "pirate" 24×24, ponto ativo 12,12, sobre a barra de título);
  app GTK X11 travado encerrado pelo título; Dock, dockapp, Clip e a raiz
  recusados; Esc, Ctrl+Alt+Esc e botão direito cancelam; wmaker vivo.
- Unitários: wlmaker `force_close` (nomes protegidos e não protegidos, o
  próprio processo, o pai, PID 1); `tests/run.sh` (compila sem avisos, nomes
  protegidos, o arquivo Xcursor válido com 24/48/72/96 px e o ponto ativo no
  centro).

## Limitações

- O aviso de recusa no X11 é uma notificação (`notify-send`); sem servidor
  de notificações, só a mensagem no terminal.
- Um processo em estado D (E/S do kernel) não morre com SIGKILL até sair
  dele; a janela some mesmo assim (conexão encerrada).
- Um cliente Wayland atrás de um proxy (waypipe, contexto de segurança)
  aparece com o PID do proxy: é ele que recebe o SIGKILL.

Capturas no repositório (sem o papel de parede do laboratório):
`docs/screenshots/ponteiro-caveira/`. As demais ficaram na máquina de
desenvolvimento, em `~/.cache/bwm-wt/cursor/evidencias/`.
