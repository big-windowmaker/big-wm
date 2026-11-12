# 05 — Integridade

Texto de prova: acentos, `ñ`, japonês, emoji, aspas tipográficas, tab,
espaços no início e no fim, CRLF e LF, quebra final.

| Caminho | Antes | Depois |
|---|---|---|
| Wayland → Wayland | íntegro | íntegro |
| Wayland → X11 | íntegro | íntegro |
| X11 → Wayland | **16/16 alteradas** (CRLF→LF) | 0 alteradas |
| X11 → X11 | **alteradas** | 0 alteradas |
| Fechar a origem (o último item mantido) | mantido, normalizado | mantido, bytes exatos |
| Sessão X11 pura | conteúdo trocado pelo do wmcliphist | o aplicativo continua dono; ao fechar, bytes exatos |

Depois da correção: **90/90** exatas na matriz sincronizada (cópias duplas
rápidas, colar 0,2 s depois, fechar a origem), comparando bytes crus
(`wl-paste -t text/plain;charset=utf-8`) e o texto que um aplicativo GTK cola
(o GTK troca CRLF por LF no destino, que é dele).
