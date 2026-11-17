# 1. Auditoria: por que as notificações interrompiam o trabalho

Data: 17/11/2026. Sessão real do usuário: Wayland (wlmaker 1:0.8.r80-43),
aplicativos nativos Wayland (Firefox com `MOZ_ENABLE_WAYLAND=1`, GTK 3,
Qt `wayland;xcb`), dunst 1.13 iniciado pelo `big-wm-notifyd`.

## O que existia

- `session/bin/big-wm-notifyd`: `exec dunst -config /usr/share/big-wm/dunstrc`
  (ou `dunstrc-dark`); `~/.config/dunst/dunstrc` do usuário tinha prioridade.
- `session/autostart/big-wm-notifications.desktop` (OnlyShowIn WindowMaker e
  wlmaker); `big-wm-theme` recarregava o tema com `dunstctl reload`.
- O pacote dunst também traz `dunst.service` (`Type=dbus`): a ativação pelo
  D-Bus podia subir um dunst antes do autostart.
- Sem histórico visível, sem "não perturbe", sem sininho.

## Como foi reproduzido (sem tocar na sessão do usuário)

`tests/notifications/focus-lab/`: um wlmaker aninhado e headless (backend
`headless`, renderização pixman), com `XDG_RUNTIME_DIR`, HOME e barramento de
sessão próprios e `systemctl`/`loginctl`/`systemd-run` substituídos por
stubs que recusam. Um cliente de ponteiro e teclado virtuais
(`wlr-virtual-pointer-unstable-v1`, `virtual-keyboard-unstable-v1`) dirige um
aplicativo GTK 3 de prova que registra cada foco, entrada/saída do ponteiro,
clique, rolagem, grab quebrado e o abrir/fechar do menu. Para o X11, um Xvfb
privado com o Window Maker e o xdotool.

## Causas encontradas

| Sintoma relatado | Causa (com evidência) | Componente |
|---|---|---|
| Arraste do Inkscape interrompido; a roda para até um novo clique | Com um botão apertado, sair da superfície ou **uma superfície nova aparecer sob o ponteiro** (o balão) mandava `wl_pointer.leave`, e o `button` solto ia para outra superfície. O GTK nunca via o soltar e ficava com o botão "apertado", ignorando a roda. Não havia grab implícito nas superfícies Wayland (TODO do próprio wlmaker em `_wlmtk_surface_element_pointer_button`). | wlmaker (`src/toolkit/surface.c`) |
| Menu aberto no clique fica preso | A mesma causa: o soltar se perdia. | wlmaker |
| Foco perdido ao digitar; menu fechado | O wlmaker aceitava **qualquer** pedido `xdg-activation` ("there is no focus stealing prevention"). Um aplicativo que se apresenta sozinho junto com a notificação (mensageiros, navegadores) tomava o teclado. Reproduzido com um cliente que pede o token sem entrada do usuário. | wlmaker (`src/server.c`) |
| Idem (variação) | Token com seat e **serial antigo sem superfície**: o wlroots não confere o foco nesse caso (apontado na revisão do PR packages#45 e reproduzido). | wlmaker |

O próprio dunst **não** roubava foco nem fechava menus: no X11 o balão é
override-redirect, no Wayland é layer-shell com teclado `NONE`. Com o
wlmaker corrigido, o dunst passa em todos os critérios do lab (ver
`05-testes.md`). O XWayland já tinha o grab implícito (patch 0156).

Fora do escopo, registrado: `_NET_ACTIVE_WINDOW` de clientes X11 no
XWayland continua aceito sem condição (o wlroots não informa a origem nem o
horário do pedido); o GTK 3 no Wayland não ativa o item de menu num
"apertar, arrastar e soltar" (antes e depois da correção).
