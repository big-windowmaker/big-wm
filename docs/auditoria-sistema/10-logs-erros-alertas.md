# 10 — Logs, erros e alertas

| Gravidade | Achado | Situação |
|---|---|---|
| Crítico | Reinício da máquina às 06:34:57 durante a auditoria | ver abaixo |
| Alto | Dock com Abas morto sem voltar (`XKillClient` 04:03, `NRestarts=0`) | corrigido |
| Médio | `wmaker: ... autostart: não foi possível executar o script` a cada início | arquivo do usuário, apontado |
| Baixo | 15 temporários órfãos do WINGs em `~/GNUstep/Defaults` (gravações interrompidas no desligamento; um das 06:34) | `big-wm-doctor --repair` os move |
| Baixo | `X_KillClient BadValue` do Window Maker (o segundo kill, cliente já fechado) | inofensivo |
| Info | core dumps de 09/11: gedit (SIGSEGV), wmix (SIGABRT, antes da troca pelo wmpulsemixer), o controlador do laboratório de 09/11 (corrigido na 0.2.0) | — |

## O reinício das 06:34:57

`systemd-logind: reboot requested from client PID 52550 ('reboot') (unit
session-3.scope)`. Os comandos da auditoria rodam no escopo do VSCodium
(`app-codium-insiders-7270.scope`, onde o journal registra os
`systemctl --user daemon-reload` deles), não no `session-3.scope`, e nenhum
chamou `reboot`. O mesmo pedido (`'reboot'`, `session-3.scope`) aparece em
09/11 às 00:10 e às 03:07. Quem roda no `session-3.scope` é o que a sessão
X11 iniciou (Window Maker, menu, Dock, terminais abertos por eles). A causa
não foi determinada.

## O bloqueio das 06:47

Inatividade: protetor de tela de 600 s (o `xset s 1800` do autostart nunca
roda) e o `xss-lock` chamou o `big-wm-lock`. Esperado.
