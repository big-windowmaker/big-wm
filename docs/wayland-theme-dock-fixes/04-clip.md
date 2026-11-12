# 04 — Clip

O Clip tem duas partes com ciclos de vida diferentes:

- O ladrilho do Clip, os dockapps (nativos e X11) e a moldura: desenhados
  pelo **wlmaker** (`clip.c`, `icon_manager.c`). Recebem o `theme_changed`
  e acompanhavam o tema.
- Os **lançadores** (`firefox`, `thunar`, Welcome, Display, VSCodium,
  onboard, preferências do Dock com Abas): desenhados pelo processo
  **`wlmdock --clip`**, um cliente Wayland separado, iniciado pela unidade
  `app-big-wm-wayland-clip@autostart.service`.

## Causa

O wlmaker acha os wlmdock pelo nome do processo
(`wlmaker_dockapps_client_name()` lê `/proc/PID/comm`) e compara com
`"wlmdock"`. O wlmdock recomeça com o tema novo por
`execv("/proc/self/exe", …)`: o kernel dá ao processo o nome do arquivo
executado, **`exe`**. No começo da sessão já há um recomeço (o
`big-wm-wprefs-sync` do login), então desde então o wlmaker não acha mais
nenhum wlmdock: nem o SIGHUP (tema) nem o SIGUSR1 (lançadores mudados,
Dock movido) chegavam.

## Correção (wlmaker, patch 0145)

- `src/wlmdock.c`: `prctl(PR_SET_NAME, "wlmdock")` no início do `main()`
  (vale depois de cada `execv`).
- `src/clip.c`: `_wlmaker_clip_client_is_wlmdock()` aceita também o cliente
  cujo **executável** é `wlmdock` (`/proc/PID/exe`, com o sufixo
  " (deleted)" de um binário atualizado); os sinais enviados vão para o log.

Basta uma das duas: o wlmdock novo funciona com o wlmaker 31 que está
rodando (corrige a sessão aberta sem logout), e o wlmaker 32 funciona até com
um wlmdock antigo já chamado `exe`.

Nenhuma arte de aplicativo foi tocada: o ícone de cada lançador é o mesmo;
o que muda é o ladrilho (fundo, chanfro, indicador) que o wlmdock desenha.
