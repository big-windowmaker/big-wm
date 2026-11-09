# i18n do big-wm — 10. Ferramentas da sessão convertidas

Cada linha: o que mostra texto, como foi marcado e o que ficou de fora.

| Ferramenta | Textos | Marcação |
|---|---|---|
| `big-wm-about` | painel Informações (Wayland) | tabela `T` com `N_()`, `tr(chave) = _(T[chave])`; "%d monitor found" com `ngettext`; "running"/"off" com comentário `TRANSLATORS` |
| `big-wm-airplay` | janela do receptor AirPlay | `_()` direto (21) |
| `big-wm-appmenu` | menus gerados: categorias de Aplicativos (`LABELS`), Dockapps (35 rótulos, 7 grupos, 8 do Wayland), "Nenhum dockapp instalado", nota dos dockapps do X11, Tema de ícones, Papéis de parede | `N_()` nas tabelas, `category_labels()`; frases com `%s` no lugar das concatenações |
| `big-wm-clipboard` | histórico da área de transferência | `_()` (35) e `ngettext` em "{n} item(s)" e "{n} favorite(s)"; nomes de teclas ("Enter", "Delete", "Ctrl+D") nas dicas sem tradução |
| `big-wm-dock-settings` | "Configurações de Aplicação na posição Dock" (réplica do diálogo do wmaker) | `_()` na tabela; traduções iguais às do `wmaker/po/pt_BR.po` |
| `big-wm-hp` | aviso do HPLIP sem PyQt5 | `_()` + `%` depois |
| `big-wm-kill` | Forçar o fechamento (Wayland) | `TEXT` com `N_()`, `text()` traduz |
| `big-wm-launcher` | Buscar aplicativos / Executar | `TEXT` com `N_()`; "{n} application(s)" com `ngettext`; categorias do appmenu |
| `big-wm-lock` | textos do layout do gtklock | `_t_layout` (gettext(1)) |
| `big-wm-lock-settings` | Configurações > Tela de bloqueio | tabelas com `N_()` |
| `big-wm-mic-tray` | ícone, dica e menu do microfone na bandeja | `_()` (20) |
| `big-wm-scrcpy` | diálogos do scrcpy/adb | `_()`; a mensagem de erro com três valores passou a `%(seconds)d`, `%(detail)s`, `%(log)s` |
| `big-wm-screenshot` | notificações da captura | `_t` |
| `big-wm-terminal` | erros (stderr e notificação) | `_t`, formatos `%s` com `xgettext: c-format` |
| `big-wm-tray` | balão e diálogos da bandeja | `TEXT` com `N_()` |
| `big-wm-wprefs-sync` | itens do menu Áreas de trabalho do menu convertido | `_ACTION_LABELS` com `N_()` e comentários `TRANSLATORS` (o gênero de "Nova"/"Próxima") |
| `big-wm-zoom` | aviso da ampliação | `warn(_("…"))` (traduz na chamada) |
| `big-wm-xkill.c` | avisos do Forçar o fechamento (X11) | `_()` |
| `big-wm-locate-pointer.c` | estado ligado/desligado (saída e notificação) | `_()`; antes só português |
| `big-wm-lockscreen.c` | janela de sessão bloqueada (X11) e fundo | `N_()` + `dgettext` uma vez na partida |
| `gtklock-keyboard.c` | dica do botão ⌨ | `dgettext("big-wm", …)` (antes "Teclado na tela / On-screen keyboard") |

## Textos achados fora da extração

Literais que o xgettext não via (tabelas, `"…" if pt else "…"`, helpers):

- `big-wm-appmenu`: `LABELS` (14), `DOCKAPPS` (35), `DOCKAPP_GROUPS` (7),
  `WAYLAND_DOCKAPPS` (8), as frases de `render_dockapps`,
  `x11_dockapps_note`, `render_icon_themes`, `render_wallpapers`;
- `big-wm-launcher`: `TEXT` (18 + o plural);
- `big-wm-kill` `TEXT` (11), `big-wm-tray` `TEXT` (10), `big-wm-about` `T`
  (12 + o plural), `big-wm-wprefs-sync` `_ACTION_LABELS` (5),
  `big-wm-dock-settings` `T` (18);
- `big-wm-lock-settings`: `BACKGROUNDS`, `SHOW`, `KEYBOARD_CHECKS`,
  `KEYBOARD_COMBOS` (25 rótulos). A última linha de `KEYBOARD_COMBOS` (tema
  claro/escuro do teclado) ficou de fora da primeira conversão e a janela
  não abria (`ValueError` ao desempacotar): a captura do laboratório
  mostrou; corrigido em cc273a7;
- `big-wm-lockscreen.c` `texts` (13), `big-wm-lock` (6), `big-wm-zoom`
  `warn` (helper que recebia os dois idiomas);
- todos conferidos no POT regenerado (297 entradas).

## Bugs e mudanças de comportamento

- `big-wm-locate-pointer` e `big-wm-terminal` passam a falar inglês fora do
  português (antes só português).
- `big-wm-zoom`, `big-wm-kill`, `big-wm-tray`, `big-wm-about`,
  `big-wm-dock-settings`, `big-wm-xkill` ignoravam o `LANGUAGE`; agora todos
  seguem a ordem do gettext (`LANGUAGE`, `LC_ALL`, `LC_MESSAGES`, `LANG`),
  com o `LANGUAGE` ignorado no locale C (como a glibc).
- Revisões pelo glossário (texto que mudou para o usuário): "Apagar" →
  "Excluir" no histórico da área de transferência (botão e três frases),
  "Restaurar padrão" → "Restaurar padrões", "Ok" → "OK" no diálogo do
  lançador (o resto do diálogo segue o `wmaker/po/pt_BR.po`).
