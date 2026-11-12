# 10 — Sinais e eventos

| Origem | Destino | Mecanismo | Estado |
|---|---|---|---|
| big-wm-wprefs-sync | wlmaker da sessão | SIGHUP ao dono do lock do WAYLAND_DISPLAY | funcionava |
| wlmaker (Clip) | wlmdock | SIGHUP (tema), SIGUSR1 (lançadores/posição), achado pelo nome do processo | **quebrado após o 1º recomeço; corrigido (0145)** |
| wlmdock | wlmaker | SIGUSR1 (lançadores do Clip mudaram) | funcionava |
| controlador | barra | ações e estados GAction pelo D-Bus; `g_bus_watch_name` | funcionava |
| controlador, barra | tema | `GFileMonitor` em `~/.config/big-wm/theme` | funcionava |

Os wlmdock recebem os sinais por signalfd (SIGHUP/SIGUSR1 bloqueados,
lidos no laço de eventos): não há reentrância nem tratador assíncrono.
