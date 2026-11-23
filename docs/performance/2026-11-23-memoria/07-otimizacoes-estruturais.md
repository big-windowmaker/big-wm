# 07 — Otimizações estruturais

O maior ganho desta rodada não veio do alocador: veio de tirar o Python de
residentes pequenos que rodam em toda sessão. Critério: mesmo comportamento,
provado por testes caixa-preta que passam contra as duas versões antes da
troca.

## Feitas

| Residente | Sessão | Antes (Python) | Depois (C) | PR |
|---|---|---:|---:|---|
| `big-wm-kwin-screenshot` (Spectacle no wlmaker) | Wayland | 15,8 MiB | 0,6 MiB | big-wm #55 |
| `big-wm-mic-tray` (filtro do microfone na bandeja) | X11 e Wayland | 15,8 MiB | 1,0 MiB | big-wm #56 |

PSS no host, 3 rodadas de cada, 20 s após o início. Na máquina real
(OptiPlex 760, pacote do makepkg instalado): mic-tray 17,2 → 1,1 MiB no
Wayland e 1,1 MiB no X11; kwin-screenshot 0,7 MiB.

### big-wm-kwin-screenshot

C com GIO (`org.kde.KWin.ScreenShot2` e `org.kde.kwin.Effects`): grim,
slurp e wmake-zones como antes, captura numa thread, resposta pelo fd do
Spectacle. 21 testes caixa-preta (`tests/screenshot/test_kwin_screenshot.py`)
passaram contra as duas versões; ASan limpo; Spectacle de verdade gerou
PNGs de tela inteira e de monitor.

### big-wm-mic-tray

C com GIO, libdbusmenu-glib (a mesma biblioteca de menu) e json-glib (o
JSON do `biglinux-microphone-cli`). Mesmos textos do domínio `big-wm`
(msgids iguais), mesmo `--state`, `pactl subscribe`, inotify no
`settings.json`. O `GFileMonitor` usa só o VFS local: a versão em Python
acordava o `gvfsd` à toa. 20 testes caixa-preta
(`tests/mic-tray/test_mic_tray.py`, dbus-daemon próprio, CLI/wpctl/pactl
falsos) passaram contra as duas versões e com ASan/UBSan. Consequência no
pacote: `big-wm-session` deixa de ser `any` (x86_64), `json-glib` nas
dependências.

Partida (mediana de 10): nome no barramento em 81 → 5 ms no host e 167 →
6 ms no OptiPlex; CPU nos 3 s iniciais 170 → ~0 ms no OptiPlex
([13](13-ux-responsividade.md)).

## Propostas (não feitas)

| Item | Ganho estimado | Por que não agora |
|---|---|---|
| `big-wm-tray` em C (A11) | ~11 MiB no Wayland (o Python responde por ~11 dos 30 MiB; o resto é GTK) | 1911 linhas: SNI, dbusmenu-gtk3, paginação, balões, modo escuro, adoção pelo wlmaker. Risco alto na bandeja; precisa de rodada própria com `tests/integration/wayland-tray.sh` |
| indicator-sound-switcher | 20–25 MiB | mantido por decisão (A14) |

## Rejeitadas (já medidas antes)

`malloc_trim(0)` (−1,1 MiB no wlmaker), `XWAYLAND_NO_GLAMOR`, menu da
bandeja criado só no clique: ver
[../../performance-decisions.md](../../performance-decisions.md).

## Compositores

wlmaker e wmaker: nenhuma mudança estrutural nesta rodada. O heap deles é
pequeno ([00](00-auditoria-inicial.md)); o wlmaker sobe ~6 MiB nas
primeiras cargas de janelas e para nesse patamar (1 h, 240 janelas:
[10](10-estabilidade-e-stress-tests.md)).
