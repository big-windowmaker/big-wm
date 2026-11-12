# 01 — Auditoria do sistema

| Item | Antes | Depois |
|---|---|---|
| Sessão | Wayland (wlmaker 31, XWayland preguiçoso), 3 saídas, 9440×1440 | igual |
| wlmaker | 1:0.8.r80.g7481afe-31 | -32 (patch 0145) instalado; o da sessão aberta só troca no próximo login |
| big-wm-* | 26.11.10-0721 | 26.11.10-2222 |
| big-wm-tabbed-dock | 1:0.2.1-1 | igual (sem defeito próprio) |
| Tema pedido (`~/.config/big-wm/theme`) | `light` | `light` |
| `ThemeFile` | `~/.config/wlmaker/Theme.plist` (gerado pelo big-wm-wprefs-sync, já com o gradiente claro `a6a6b6→515561`) | igual |
| wlmdock (Dock e lançadores do Clip) | 2 processos, `comm` = **`exe`** | 2 processos, `comm` = `wlmdock` |
| Controlador do Dock com Abas | **nenhum processo** | 1 processo |
| Processo 4563 (`ld-linux … libgtk-nocsd.so.0 big-wm-tabbed-dock`) | filho da barra, 1,8 MB, lendo um pipe | é o vigia do próprio gtk-nocsd (relança o programa sem a biblioteca se ele cair): não é travamento nem duplicata |

Repositórios: wlmaker = upstream 7481afe + patches de `packages/wlmaker`
(árvore de trabalho `~/.cache/bwm-wt/tema-dock/wlmaker`, branch
`fix/tema-dock-2026-11-10`); big-wm e packages nos worktrees
`~/.cache/bwm-wt/audit/*`, branch `fix/auditoria-2026-11-10`; Tabbed Dock no
repositório próprio `big-wm-tabbed-dock` (a cópia antiga do big-wm não foi
tocada).

Backup antes de instalar: `~/.cache/bwm-audit/backup-tema-222411/`
(`~/.config/wlmaker`, `~/.config/big-wm`, `~/GNUstep/Defaults`).
