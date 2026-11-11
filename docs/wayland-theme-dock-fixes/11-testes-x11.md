# 11 — X11

As três correções são de código que só roda no Wayland: o wlmdock e o
`clip.c` do wlmaker, e o `big-wm-dockapps` (autostart
`OnlyShowIn=wlmaker`; o X11 usa o `big-wm-dockapps-autolaunch`). A suíte do
big-wm (`tests/run.sh`, que inclui as verificações do X11) passou inteira
depois das mudanças. O tema no X11 (Window Maker) foi validado na auditoria
anterior e não foi alterado aqui.
