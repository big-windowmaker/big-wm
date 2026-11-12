# 03 — Cadeia do tema no wlmaker

1. `big-wm-theme light|dark` grava `~/.config/big-wm/theme`, troca o
   `ThemeFile` quando é o do sistema e chama `big-wm-wprefs-sync`.
2. O `big-wm-wprefs-sync` refaz `~/.config/wlmaker/Theme.plist` (tema do
   modo + aparência do Window Maker) e manda **SIGHUP ao compositor desta
   sessão** (o dono do lock do `WAYLAND_DISPLAY`; um wlmaker aninhado não é
   tocado).
3. `wlmaker_server_reload()` relê a configuração e o tema
   (`wlmaker_action_theme_load_from_file`), que emite `theme_changed`.
4. O Clip (`_wlmaker_clip_handle_theme_changed`) refaz os próprios
   ladrilhos, a sobreposição, a imagem e os espaçadores e **manda SIGHUP aos
   clientes `wlmdock`**.
5. Cada wlmdock recomeça com o tema novo: `execv("/proc/self/exe", argv)`.

Os passos 1–4 funcionavam (o log do wlmaker mostra os quatro
"Reloading configuration" às 21:52–21:53 e o clipe ficou claro). O passo 5
nunca aconteceu depois do primeiro (ver 04 e 05).
