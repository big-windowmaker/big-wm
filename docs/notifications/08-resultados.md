# 8. Resultados

## O problema e a solução

As interrupções (foco perdido ao digitar, menu fechado, arraste do Inkscape
cortado, roda parada) vinham do **wlmaker**, não do servidor de
notificações: faltava o grab implícito do ponteiro nas superfícies Wayland e
não havia prevenção de roubo de foco no `xdg-activation`. Corrigido na
origem (wlmaker 44, patches 0158 e 0159, packages#45). Com isso, o lab de
foco passa inteiro também com o dunst.

Sobre essa base, o **Big WM Notification Center** (`big-wm-notifications`)
substitui o dunst nas sessões Big WindowMaker: balões OPENSTEP que nunca
pegam foco, central com histórico, busca, Limpar com desfazer, "não
perturbe" e o sininho com contador na bandeja, nas duas sessões.

## Arquivos

- packages: `wlmaker/0158-…patch`, `wlmaker/0159-…patch`, PKGBUILD (44).
- big-wm: `session/notifications/` (servidor), `session/bin/big-wm-notifyd`,
  `session/lib/big-wm-autostart-condition` (`notifications`),
  `session/systemd/autostart-conditions/dunst.service`,
  `session/bin/big-wm-tray` (IconPixmap que muda passa a atualizar),
  `session/bin/big-wm-kill` (protege o servidor), Makefile, PKGBUILD (pacote
  `big-wm-notifications`), `po/` (pt_BR, es), `tests/notifications/`,
  `tests/run.sh`, `docs/notifications/`.

## Limitações conhecidas

- `_NET_ACTIVE_WINDOW` de aplicativos X11 no XWayland ainda ativa sem
  condição.
- GTK 3 no Wayland: "apertar, arrastar e soltar" num menu não escolhe o item
  (igual antes; clicar e clicar funciona).
- Fullscreen/jogos: os balões não são suprimidos automaticamente (use "não
  perturbe"); agrupamento por aplicativo na central não foi feito.
- Não testado em hardware real nem com vários monitores/escala.

## Atualizar

`sudo pacman -Syu` traz wlmaker 44 e big-wm-notifications (após o merge e o
build dos repositórios). Para começar já sem sair da sessão:
`pkill -x dunst; big-wm-notifications &` (o próximo login usa o novo
automaticamente).
