# i18n do big-wm — 12. Plurais e placeholders

## Plural-Forms

- pt_BR: `nplurals=2; plural=(n > 1);` — tirado dos catálogos instalados
  (`msgunfmt` de `gtk30.mo`, `glib20.mo`, `coreutils.mo`,
  `gettext-runtime.mo` em `/usr/share/locale/pt_BR/LC_MESSAGES`), como o
  welcome e o tabbed-dock já usam.
- es: `nplurals=2; plural=(n != 1);` — dos mesmos quatro catálogos em `es`.

Com `(n > 1)`, 0 é singular: "0 item", "0 favorito", "0 aplicativo"
(antes o código escolhia o plural para 0). É a regra do GTK/GLib em
pt_BR; as frases com número continuam naturais ("0 item · 0 favorito").

Plurais do domínio: `{n} item`/`{n} items`, `{n} favorite`/`{n}
favorites` (big-wm-clipboard), `{n} application`/`{n} applications`
(big-wm-launcher), `%d monitor found`/`%d monitors found` (big-wm-about).

## Placeholders

| Estilo | Onde | Conferido por |
|---|---|---|
| `%s`, `%d` (Python `%`) | hp, airplay, scrcpy, kill, appmenu, dock-settings | `python-format` + `msgfmt --check` |
| `%(nome)s` | scrcpy (três valores: o tradutor pode reordenar) | `python-format` |
| `{n}`, `{cmd}`, `{err}` (`str.format`) | launcher, clipboard | `python-brace-format` |
| `%s` (C) | xkill | `c-format` |
| `%s` (shell, `printf`) | big-wm-terminal | `# xgettext: c-format` → `c-format, sh-printf-format` |

Literais `%s`/`%d` que não são formatação: os dois avisos do diálogo do
lançador ("%s will be replaced with current selection", "%d will be
replaced with the file name") mostram o marcador do wmaker; o
`python-format` só obriga a tradução a mantê-lo, o que é o desejado.

Marcação preservada: `<b>`, `<i>`, `<tt>` (Pango) no airplay, hp, scrcpy;
`\n`; `›` (caminhos de menus do iPhone/Android); `“%s”` (aspas
tipográficas do kill/xkill). O harness do skill
(`verify_translation_output.py`) passou limpo nos seis lotes.
