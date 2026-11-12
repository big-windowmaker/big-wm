# 01 — Auditoria

| Item | Encontrado |
|---|---|
| Sessão | Wayland (wlmaker 32 → 33 instalado; o da sessão aberta troca no próximo login) |
| Spectacle | 1:6.7.5-1 |
| wl-clipboard / grim / slurp / xsel | 2.3.0 / 1.5.0 / 1.5.0 / 1.2.1 (xclip ausente) |
| Protocolos do wlmaker | wl_data_device_manager, zwp_primary_selection, zwlr_data_control_v1, zwlr_screencopy, zwlr_foreign_toplevel, zxdg_output |
| wlroots | 0.20.2: nega a leitura e a escrita da seleção pela ponte a clientes X11 sem o foco |
| Histórico | `wl-paste --type text --watch big-wm-clipboard --store` (Wayland), wmcliphist no XWayland (`clipboard CLIPBOARD`, `auto_take_up yes`) |
| Captura | `big-wm-kwin-screenshot` (org.kde.KWin.ScreenShot2 com grim/slurp) |
| Origem do pacote do wmcliphist | big-dockapps 10, branch `fix/dockapps-wayland-2026-11` do packages (não o da auditoria) |
| Aplicativos | Firefox, VSCodium, gedit, kate, Telegram: Wayland nativos; X11 só GParted e os dockapps |
