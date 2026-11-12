# 08 — Dockapps "travados"

## Dock com Abas (controlador)

Sintoma: o ladrilho do Dock com Abas no WMDock não responde.

Evidência: **não havia processo** `big-wm-tabbed-dock-dockapp` na sessão; o
journal das 21:36:29 diz `big-wm-tabbed-dock-dockapp já está aberto nesta
sessão` (big-wm-dockapps). O ladrilho visto era o reservado para o dockapp
acoplado, com o ícone dele, sem programa por trás.

Causa: `big-wm-dockapps` procura um processo "já aberto" com `pgrep -x` nos
15 primeiros caracteres do nome (o limite do `comm` no Linux):
`big-wm-tabbed-d` — que é também o `comm` da **barra**
`big-wm-tabbed-dock`, rodando no mesmo DISPLAY. O controlador nunca era
iniciado no Wayland.

Correção (big-wm, `session/bin/big-wm-dockapps`): `is_program()` confere
cada candidato pelo nome completo (executável, `argv[0]`, ou `argv[1]` de um
interpretador). Testes em `tests/run.sh` (o primeiro falha sem a correção).

Na sessão real, depois de instalar: o controlador iniciou (um processo) e os
outros dockapps não foram duplicados.

## O que não era defeito

- O processo filho da barra com `libgtk-nocsd.so.0`: vigia do gtk-nocsd.
- Cliques injetados com `xdotool` direto no XWayland (XTEST) pararam de
  chegar ao controlador depois que o Dock desceu: as posições X dos
  dockapps só são ressincronizadas quando o **ponteiro do Wayland** entra
  num ladrilho (`_wlmaker_toplevel_icon_xwl_sync_position` e restack). Pelo
  caminho real do ponteiro (xdotool no Xvfb que hospeda o wlmaker
  aninhado), as setas funcionaram cinco vezes seguidas com o Dock subindo e
  descendo. Não foi registrado como defeito.
