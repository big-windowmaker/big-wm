# i18n do big-wm — 02. Arquitetura do gettext

## Domínios

| Domínio | Fontes | Catálogos | `.mo` instalado por |
|---|---|---|---|
| `big-wm` | ferramentas da sessão (`po/POTFILES`) | `po/<idioma>.po` | `make install-session` (pacote `big-wm-session`) |
| `big-wm-welcome` | `welcome/` | `welcome/po/` | `make install-welcome` |
| `big-wm-tabbed-dock` | `tabbed-dock/` (meson) | `tabbed-dock/po/` | `meson install` |

Um domínio só para as ferramentas da sessão: são muitos programas
pequenos com o mesmo vocabulário (Cancelar, Configurações...). O `.mo` vai
no `big-wm-session`, de que dependem o `big-wm-wayland`; o
`big-wm-locate-pointer` e o `big-wm-lockscreen` usam o mesmo domínio (sem
o `big-wm-session` instalado ficam em inglês, nunca quebram).

msgids em inglês (o inglês que já existia no `tr(pt, en)`); o português é
a tradução (o texto que o usuário já via, revisado pelo glossário).

## Python

No topo de cada programa, uma vez por processo:

```python
_LOCALE = os.environ.get("LC_ALL") or os.environ.get("LC_MESSAGES") or os.environ.get("LANG") or "C"
_TRANSLATION = gettext.translation("big-wm", os.environ.get("BIGWM_LOCALEDIR"), fallback=True,
                                   languages=["C"] if _LOCALE.split(".")[0] in ("C", "POSIX") else None)
_ = _TRANSLATION.gettext
ngettext = _TRANSLATION.ngettext   # onde há plural


def N_(text):                      # tabelas: marca para o xgettext
    return text
```

- Como na glibc, o `LANGUAGE` não vale no locale C (o Python sozinho o
  usaria: `LANG=C ferramenta` numa sessão em português saía em português,
  diferente dos programas em C e do GTK).
- Tabelas (`TEXT`, `LABELS`, `DOCKAPPS`, `BACKGROUNDS`...) guardam o msgid
  com `N_()` e traduzem onde mostram (`_(valor)`); funções `text()`,
  `category_labels()`, `_labels()` devolvem o dicionário traduzido.
- `big-wm-launcher` carrega o `big-wm-appmenu` como módulo e usa o `_`,
  `N_` e `ngettext` dele (um catálogo aberto por processo).
- `big-wm-appmenu` guarda o menu Aplicativos em cache: a chave agora inclui
  o `LANGUAGE` e o arquivo `.mo` (caminho e mtime); `CACHE_VERSION` 5.
- `BIGWM_LOCALEDIR`: diretório de catálogos fora de `/usr/share/locale`
  (só testes e laboratório).

## C

`big-wm-xkill`, `big-wm-locate-pointer`: `setlocale(LC_MESSAGES, "")`
(só as mensagens: o `strtod` da configuração continua no locale C),
`bindtextdomain("big-wm", BIGWM_LOCALEDIR ou LOCALEDIR)`,
`bind_textdomain_codeset("UTF-8")`, `#define _(s) gettext(s)`.
`big-wm-lockscreen` já fazia `setlocale(LC_ALL, "")`; a tabela de textos
virou `N_()` em inglês, traduzida uma vez na partida com `dgettext`. O
módulo do gtklock (`gtklock-keyboard.c`) usa `dgettext("big-wm", …)` sem
mexer no `textdomain` do gtklock.

## Shell

`_t "texto"` chama o `gettext(1)` com `TEXTDOMAIN=big-wm` só no comando
(nada exportado para os filhos), com reserva sem o binário:

```bash
_t() {
	if command -v gettext >/dev/null; then
		TEXTDOMAIN=big-wm TEXTDOMAINDIR=${BIGWM_LOCALEDIR:-/usr/share/locale} gettext "$1"
	else
		printf '%s' "$1"
	fi
}
```

Só em notificações e erros mostrados (big-wm-screenshot, big-wm-terminal,
layout do gtklock no big-wm-lock), fora de laços e fora do caminho do
login (≈1 ms por chamada). `$"…"` foi descartado: o bash expande o texto
traduzido como uma string entre aspas duplas (o xgettext avisa que a
sintaxe é insegura) e só o bash 5.2+ tem o `noexpand_translation`. Formatos com `%s` vêm marcados com
`# xgettext: c-format` (o `msgfmt --check` confere o `%s` da tradução). No
layout do gtklock a tradução passa por `_t_layout`, que tira `& < > | \`
(entra num XML e num `sed`).

## Extração (`make pot`)

`po/POTFILES` lista as fontes; a linguagem vem da extensão (`.c`) ou da
primeira linha (python, sh). Três chamadas do xgettext (Python com `-k_
-kN_ -kngettext:1,2`; Shell com `-k_t -k_t_layout`; C com `-k_ -kN_`),
`--add-comments=TRANSLATORS`. Arquivo do POTFILES que não existe é erro.
O mesmo alvo regenera o POT do welcome (sem posições, como o projeto já
guardava). `make update-po` faz o `msgmerge`; `make mo` compila em
`po/build/` (gitignored). O tabbed-dock segue com o meson
(`big-wm-tabbed-dock-pot`, `-update-po`).

## .desktop

Formato nativo: a chave padrão em inglês e o `[pt_BR]` ao lado
(`Name`, `GenericName`, `Comment`, `Keywords`). Só o tabbed-dock usa
`.desktop.in` + `i18n.merge_file` (meson), e os dois `.desktop.in` dele já
estão no POT.

## Desempenho

`import gettext` ≈0,25 ms e a leitura do `.mo` ≈0,5 ms por processo;
`big-wm-appmenu --dockapps`: 40,7–41,8 ms antes, 42,5–43,6 ms depois
(30 execuções, ruído da máquina na mesma ordem). Nada de msgfmt/xgettext
em tempo de execução.
