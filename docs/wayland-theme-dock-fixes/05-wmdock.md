# 05 — WMDock

O WMDock inteiro (ladrilho principal com o logo, `org.communitybig.ashyterm`,
`rofi`, `steam`, e as molduras dos dockapps) é o processo **`wlmdock`**
(unidade `app-big-wm-wayland-dock@autostart.service`), com a mesma causa e a
mesma correção do Clip (04). Os dockapps X11 do Dock (wmclock, wmcpuload,
wmmemload, wmnet, wmpulsemixer, wmusic, controlador) são janelas do XWayland
em ladrilhos que o wlmdock reserva; também ficaram claros depois do
recomeço do wlmdock.

Validação na sessão real: com os wlmdock novos, quatro trocas seguidas
(escuro, claro, escuro, claro) — os dois processos recomeçaram na última
(log às 22:34:00) e continuaram com o nome `wlmdock`; nenhum `exe`.
