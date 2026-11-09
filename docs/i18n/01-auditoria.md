# i18n do big-wm — 01. Auditoria (novembro de 2026)

Base: `feat/missao-integracao` (14c28a7), o que está instalado na máquina.
Branch deste trabalho: `feat/i18n-gettext-2026-11`.

## O que havia

| Onde | Como o idioma era escolhido | Idiomas |
|---|---|---|
| `big-wm-airplay`, `-hp`, `-scrcpy`, `-mic-tray`, `-clipboard`, `-lock-settings` | `tr(pt, en)` com `PT = (LANGUAGE or LC_ALL or LC_MESSAGES or LANG).startswith("pt")` | pt, en |
| `big-wm-about` | tabela `T = {chave: (pt, en)}` e `tr(chave)` | pt, en |
| `big-wm-dock-settings` | `'pt' if PT else 'en'` em cada valor de `T` | pt, en |
| `big-wm-kill`, `big-wm-tray` | `TEXT = {"pt": {...}, "en": {...}}` | pt, en |
| `big-wm-launcher` | `TEXT = {"pt", "es", "en"}` pela chave do locale | pt, es, en |
| `big-wm-appmenu` | `LABELS = {"pt", "es", "en"}` (categorias), tuplas `(…, pt, en, …)` dos dockapps e grupos, `"…" if pt else "…"` nas notas, temas de ícones e papéis de parede | pt, es (só categorias), en |
| `big-wm-wprefs-sync` | `_ACTION_LABELS = {"pt", "es", "en"}` (itens do menu Áreas de trabalho convertido) | pt, es, en |
| `big-wm-zoom` | `warn(text_pt, text_en)` | pt, en |
| `big-wm-xkill.c` | `pt()` por `LC_ALL/LC_MESSAGES/LANG` | pt, en |
| `big-wm-locate-pointer.c` | só português fixo na notificação e na saída | pt |
| `big-wm-lockscreen.c` | `texts[3][T_N]` com `pick_language()` | pt, es, en |
| `gtklock-keyboard.c` | dica fixa "Teclado na tela / On-screen keyboard" | as duas juntas |
| `big-wm-lock` (layout do gtklock) | `case ${LANG%%_*}` | pt, es, en |
| `big-wm-screenshot` | `case ${LANG%%_*}` | pt, es, en |
| `big-wm-terminal` | `die "…"` só em português (stderr e notify-send) | pt |
| `.desktop` do autostart e afins | `Name=`/`Comment=` em português na chave padrão | pt |
| `welcome/` | gettext, domínio `big-wm-welcome` | pt_BR completo (163) |
| `tabbed-dock/` | gettext + meson, domínio `big-wm-tabbed-dock`, `.desktop.in` | pt_BR completo (159) |

Problemas: cada ferramenta com sua regra de idioma (umas olhavam o
`LANGUAGE`, outras não); textos fora de qualquer catálogo (nenhum
tradutor os acha); o espanhol só em alguns lugares; o
`big-wm-locate-pointer` e o `big-wm-terminal` sempre em português;
autostarts com nome em português para qualquer idioma.

## Fora do escopo (decidido, não esquecido)

- Mensagens de diagnóstico no stderr e textos de uso (`uso: …`) das
  ferramentas de linha de comando e dos sincronizadores
  (`big-wm-wprefs-sync`, `big-wm-menu-migrate`, `big-wm-wmstyle`,
  `big-wm-dock-reset`, `big-wm-firstrun`, `big-wm-tray --autostart`…): são
  logs, não interface; ficam em português como estavam.
- Nomes de pastas criadas (`Imagens/Capturas de tela` do
  `big-wm-firstrun` e do `big-wm-screenshot`): são caminhos, não textos; o
  `case ${LANG%%_*}` deles ficou (marcado `i18n-lint: ignorar`).
- `big-wm-menu-migrate`: compara e insere rótulos dos menus `.pt_BR`
  (dados dos menus, não do gettext).
- `big-wm-airplay-setup` (roda como root pelo pkexec): as duas mensagens
  dele só aparecem fora do uso normal (sem root; ufw ausente sai com 0).
- Menus padrão (`WMRootMenu[.pt_BR]`, `RootMenu.plist[.pt_BR]`): um arquivo
  por idioma, escolhido pelo `big-wm-firstrun` (com hashes das versões
  distribuídas). Não foram reestruturados; ganharam um teste (ver
  `14-testes-validacao.md`).
- O menu do ladrilho do Dock/Clip no Wayland ("Opções do Clip",
  "Configurações...", "Terminar") é do wlmaker (domínio dele).

## Cobertura medida (`LC_ALL=C msgfmt --statistics`)

| Catálogo | Antes | Depois |
|---|---|---|
| `po/pt_BR.po` (big-wm) | não existia (textos no código) | 297 traduzidas |
| `po/es.po` (big-wm) | não existia | 54 traduzidas, 243 sem tradução (só o espanhol que já estava no código) |
| `welcome/po/pt_BR.po` | 163 traduzidas | 163 traduzidas (POT regenerado: nenhuma string nova nesta base) |
| `tabbed-dock/po/pt_BR.po` | 159 traduzidas | 159 traduzidas (POT regenerado pelo meson: só posições) |
