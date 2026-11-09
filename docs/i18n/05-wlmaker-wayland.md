# i18n do wlmaker (sessão Wayland): gettext no lugar da tabela fixa

Agente i18n-wlmaker, 09/11/2026. Worktree do wlmaker
`~/.cache/bwm-wt/i18n-wlmaker/wlmaker`, branch `feat/i18n-gettext-2026-11`
(base `fix/xwl-toplevel-geometry`, a do wlmaker 27 instalado; libbase no
commit do superprojeto, 348968be).

## Problema

O wlmaker traduzia com uma tabela fixa em `src/i18n.c`
(`_wlmaker_i18n_pt_br[]`, 152 pares) e um `wlmaker_i18n()` que lia
`LANGUAGE`/`LC_ALL`/`LC_MESSAGES`/`LANG` à mão. Só servia ao português, cada
texto novo precisava entrar na tabela, e vários textos mostrados nem passavam
por ela:

- menu da janela (Maximizar, Sombrear, Fechar...): sempre em inglês;
- erros do menu raiz gerado ("Failed, exit code %d"...), "Unnamed" do toolkit,
  títulos e categorias dos menus do `wlmtool`;
- `wlmaker_i18n(cond ? "A" : "B")`: o xgettext só enxerga o primeiro texto
  (conferido: "Bbb" não sai no POT), então metade dos itens Mover/Esconder
  ficaria fora de qualquer catálogo;
- frases montadas de pedaços (`nome + " will be forcibly closed..."`,
  `título + " is part of the session..."`), que outro idioma não consegue
  reordenar.

## Solução (commits no wlmaker)

- `193b5cf` wlmbattery: Say why the status codes are not translated
- `f7d040f` i18n: Translate through gettext (domain "wlmaker"), mark all texts shown
- `339b6e4` po: pt_BR with Window Maker's words for its window actions

O que mudou:

- `wlmaker_i18n(text)` = `dgettext("wlmaker", text)`; a API ficou igual, as
  centenas de chamadas não mudaram. `wlmaker_i18n_init()` no início do
  `wlmaker` e do `wlmdock`: `setlocale(LC_ALL, "")`, depois
  `setlocale(LC_NUMERIC, "C")`, `bindtextdomain("wlmaker", LOCALEDIR)`,
  `bind_textdomain_codeset(UTF-8)`, `textdomain`. `LOCALEDIR` vem do CMake
  (`CMAKE_INSTALL_FULL_LOCALEDIR`, `/usr/share/locale` no pacote); a variável
  de ambiente `WLMAKER_LOCALEDIR` troca o diretório (build, laboratório).
  O `wlmtool` faz o mesmo no `main()`.
- **LC_NUMERIC fica "C"** (decisão): o parser de plist da libbase usa
  `strtod()` (`src/strutil.c`), e o wlmaker grava números em plist; com
  `pt_BR` o `1.5` viraria `1`. Testado: com `LC_ALL=pt_BR.UTF-8`, depois do
  init, `printf("%.1f")` dá `1.5` e `LC_NUMERIC` é `C`.
- A tabela saiu; as traduções dela foram para `po/pt_BR.po` como sugestão
  (dos 152 pares: 132 msgids continuam iguais; 3 fragmentos viraram frases
  inteiras com `%s` e "Workspace" virou "Workspace %zu"; os 14 nomes de layouts do wmake-zones, que agora
  vêm do catálogo dele, e 2 itens que nada usava ficaram de fora).
- Marcas: `N_()` nas tabelas (menu da janela, níveis do Dock, mover dockapp,
  balões de Dock/Clip/gaveta cheios), `wlmaker_i18n()` em cada ramo dos
  ternários, frases inteiras ("%s will be forcibly closed.\n...",
  "“%s” is part of the session...", "This window is part of the session..."),
  `"Workspace %zu"` para áreas novas (como "Workspace %i" do Window Maker),
  `dgettext("wlmaker", ...)` no toolkit e `WLMTOOL_I18N()` no wlmtool (eles não
  ligam o `i18n.c`). `wlmaker_i18n` tem `format_arg(1)`: o `-Wformat` continua
  valendo nos formatos traduzidos.
- Zonas: nomes dos layouts internos do wmake-zones ("Metades",
  "Quadrantes"...) e das zonas ("Editor", "Terminal", "Browser") vêm do
  catálogo do próprio wmake-zones (domínio `wmake-zones`, já traduzido), como
  no overlay X11 dele. Não se misturam domínios: o texto é do wmake-zones.
- `po/` (CMake): `POTFILES`, `LINGUAS`, `wlmaker.pot`, `pt_BR.po`,
  `glossary/pt_BR.tsv`; `update-pot.sh` roda o xgettext com
  `-k --keyword=wlmaker_i18n --keyword=N_ --keyword=dgettext:2
  --keyword=WLMTOOL_I18N` (sem as palavras-chave padrão: `gettext()` de outro
  domínio não entra) e o `extract-plist.awk` para os títulos do
  `share/wlmaker/Dockapps.plist` (o wlmaker os traduz em tempo de execução;
  título sem aspas faz a extração falhar). O build compila cada catálogo com
  `msgfmt --check` em `<build>/po/locale/<lang>/LC_MESSAGES/wlmaker.mo` e
  instala em `${CMAKE_INSTALL_LOCALEDIR}`. Alvos: `make wlmaker-pot`,
  `make wlmaker-update-po`.

### Textos achados fora da extração

| Onde | Textos | Antes |
|---|---|---|
| `tl_menu.c` (menu da janela) | Maximize, Unmaximize, Fullscreen, Miniaturize, Shade, Unshade, Move to workspace ..., Close | sempre em inglês |
| `root_menu.c` (menu gerado que falhou) | Failed, exit code %d; Failed, signal %d; Failed to parse/populate ... | inglês |
| `toolkit/primitives.c`, `toolkit/window.c` | Unnamed; Unnamed window %p | inglês |
| `tool/menu.c` (wlmtool) | 16 categorias, Other, Appearance, System Themes, User Themes, Applications | inglês (TODO do upstream) |
| `dock_menu.c` | Normal, Auto raise & lower (tabela) | inglês |
| ternários em `clip.c`, `wlmdock.c`, `dock_menu.c`, `appicons.c` | Move to Left Side/Top, Bring Here, Hide | invisíveis ao xgettext |
| `tile_drag.c`, `icon_manager.c` | balões "... is full", Move Up/Down/Left/Right (variáveis) | só pela tabela |
| `action.c` | Workspace %zu | "Área 3" (tabela, fragmento) |
| `Dockapps.plist` | 105 títulos dos menus dos dockapps | só pela tabela |

Não traduzidos, por decisão: registros (`bs_log`); `--help` e mensagens de
erro na saída padrão dos programas (a descrição dos argumentos é impressa
pela libbase, `bs_arg_print_usage`; fica para depois, exige mexer na
libbase); os códigos CHRG/DISC/FULL/UNKN/AC do `wlmbattery` (quatro letras no
ladrilho, como o wmbattery); o título "Clip" do menu (nome); as respostas do
`windows -d` das zonas ("flutuante por escolha do usuário"...): são o
protocolo IPC do wmake-zones, em português nas duas sessões, e o
`wmake-zones-prefs` os traduz (commit b85d763 do agente i18n-zd); mudar o texto
no wlmaker quebraria esse mapeamento.

## Menus raiz (RootMenu.plist) e Config.plist: decisão

Os menus localizados continuam sendo **modelos por idioma** no big-wm
(`data/wlmaker/RootMenu.plist` e `RootMenu.plist.pt_BR`), escolhidos pelo
`big-wm-firstrun` (`localized()`: `.pt_BR`, `.pt`, depois o inglês) e copiados
uma vez para `~/.config/wlmaker/RootMenu.plist`; o Window Maker faz o mesmo com
`WMRootMenu.<lang>`. Não passamos os rótulos do menu pelo gettext porque:

- o menu é arquivo do usuário: ele edita, o `big-wm-wprefs-sync` converte o
  `WMRootMenu` do X11 para ele, as migrações do `big-wm-firstrun` procuram itens
  pelo rótulo; traduzir em tempo de execução traduziria rótulos escritos pelo
  usuário e quebraria essas buscas;
- os rótulos são dados, não msgids do código.

Melhoria possível (no big-wm, não aqui): gerar os `RootMenu.plist.<lang>` no
build a partir do inglês e de um `.po` próprio dos menus, para não manter
duas cópias à mão. O `Config.plist` do wlmaker não tem texto mostrado (só
chaves e comandos). Sem catálogo, o wlmaker cai no inglês: os textos dele são
os msgids, e o menu embutido (`etc/RootMenu.plist`) é inglês. Nada mudou nos
menus (são do big-wm e de outro agente).

## Tradução pt_BR

- Fluxo do skill gettext-translation: glossário `po/glossary/pt_BR.tsv` (o
  comum mais 16 termos do domínio, com a origem), exportação com
  `export_po_work.py --strip-existing --suggest-from` (a tabela antiga como
  sugestão), 4 lotes, harness `verify_translation_output.py` limpo nos 4,
  `check_po_placeholders.py` limpo, `msgfmt --check` limpo, `msgcat` para
  normalizar.
- Lotes para o revisor: `~/.cache/bwm-wt/i18n/wlmaker/lote-0{0..3}/`
  (`pending.jsonl`, `output.jsonl`) e `all.pending.jsonl`. **Tradução não
  revisada**: o revisor independente confere contra a rubrica.
- Plural-Forms: `nplurals=2; plural=(n > 1);` (fonte: gtk30.mo, glib20.mo,
  coreutils.mo, gettext-runtime.mo instalados). O domínio ainda não tem
  plurais.
- Vocabulário do Window Maker nas duas sessões (guia corrigido em 09/11, e o
  catálogo do agente do Window Maker, branch `feat/i18n-pt-br-wmaker`):
  Minimizar, Sombrear/Dessombrear, Manter sobre outros, Terminar (aplicação),
  Recolhido, Trazer à frente ao apontar, Mostrar aqui, Remover ícone, OK.
- Mudanças em relação à tabela antiga: Kill Application "Terminar Aplicação"
  → "Terminar aplicação"; Force Close "Forçar o fechamento" → "Forçar
  encerramento" (título novo, glossário); Collapsed "Minimizado" → "Recolhido";
  OK "Ok" → "OK"; "Próxima área"/"Área anterior"/"Destruir última área" →
  "Próxima área de trabalho"/"Área de trabalho anterior"/"Remover a última
  área de trabalho" (o guia evita "Área" sozinho); "Mostrar Aqui"/"Remover
  Ícone" em minúsculas; "Update" dos dockapps → "Atualização" (título de
  submenu de intervalos); "%s será fechado à força" → "O aplicativo %s será
  fechado à força" (o gênero do nome variava: "Calculadora será fechado",
  visto no laboratório).

### Cobertura (`msgfmt --statistics`)

| Catálogo | Antes | Depois |
|---|---|---|
| wlmaker pt_BR | sem catálogo (tabela fixa: 152 pares, 132 com o mesmo msgid de hoje) | 194 traduzidas, 0 fuzzy, 0 sem tradução (194 entradas no POT) |

## Testes

- `ctest` no build: 30/30. Novos:
  - `wlmaker_test`, conjunto `i18n`: com um locale real gerado (o gettext
    ignora `LANGUAGE` no "C" e no "C.UTF-8") e `LANGUAGE=pt_BR`,
    `wlmaker_i18n("Close")` dá "Fechar" pelo `.mo` do build; texto
    desconhecido e `""` voltam iguais; `LANGUAGE=es_ES` (sem catálogo) e
    domínio ligado a um diretório inexistente voltam ao inglês; `LC_NUMERIC`
    continua "C" com `pt_BR.UTF-8`. Obs.: a glibc guarda as traduções por
    locale; depois de mudar `LANGUAGE` é preciso `setlocale()` de novo.
  - `i18n_check` (`po/check-i18n.sh`): POTFILES lista todo `.c` com textos
    marcados e nada que falte; POT regenerado igual ao do repositório;
    nenhum texto puro passado a menus, diálogos, títulos, balões ou
    `cairo_show_text` (sem `wlmaker_i18n`/`N_` ou o comentário
    `i18n: not translated`); cada catálogo passa `msgfmt --check` e está
    mesclado (`msgcmp`). Conferido que falha alto: texto novo sem marca,
    arquivo tirado do POTFILES, msgid novo sem regenerar o POT, POTFILES
    apagado.
- `shellcheck` limpo em `po/update-pot.sh` e `po/check-i18n.sh`.
- Laboratório Wayland (`mklab.sh i18n-wlm`, wlmaker e wlmdock do worktree,
  `WLMAKER_LOCALEDIR=<build>/po/locale`), capturas em
  `~/.cache/bwm-wt/i18n/wlmaker/shots/`:
  - `LANG=pt_BR.UTF-8` (refeitas depois da correção de vocabulário): menu do Dock (`pt_BR-01`), Posição do Dock
    (`pt_BR-02`), menu do dockapp wlmcalclock com as preferências do
    Dockapps.plist (`pt_BR-04`), menu do Clip (`pt_BR-06`), menu da janela e o
    submenu de áreas (`pt_BR-08`, `-09`; numa primeira rodada, "Nova área de
    trabalho" criou "Área de trabalho 5", visto nesse submenu), menu
    do appicon (`pt_BR-10`), confirmação de Terminar aplicação (`pt_BR-11`),
    aviso do Ctrl+Alt+Esc fora de janela (`pt_BR-12`), Alt+Tab (`pt_BR-13`,
    só ícone e título), aviso de componente da sessão (`pt_BR-15`).
  - `LANG=en_US.UTF-8` e `LANG=es_ES.UTF-8`: Dock, Clip e dockapp em inglês
    (`en_US-*`, `es_ES-*`); no es_ES a calculadora (GTK) e o wlmcalclock
    (LUN) aparecem em espanhol, o wlmaker em inglês: o fallback sem catálogo.

## Limitações e próximos passos

- `--help` dos programas e mensagens de erro no terminal seguem em inglês
  (libbase imprime as descrições).
- O POT inclui os títulos do `Dockapps.plist` do wlmaker; entradas que o
  usuário acrescentar no `~/.config/wlmaker/Dockapps.plist` aparecem como
  escritas (sem msgid).
- Novo idioma: acrescentar a `po/LINGUAS`, `msginit -l <lang> -i
  po/wlmaker.pot -o po/<lang>.po`, traduzir; o build compila e instala.
- O overlay das zonas e o arrasto com zonas não foram capturados (as strings
  vêm do mesmo catálogo; os nomes dos layouts, do catálogo do wmake-zones).

## Para o integrador (PKGBUILD do wlmaker)

- Regenerar o patch combinado (hoje 0144) com os três commits acima; nada
  muda na libbase (o `libbase-0001` fica como está).
- `makedepends`: `gettext` (o CMake exige `msgfmt`/`msgmerge`:
  `find_package(Gettext REQUIRED)`). `checkdepends`: `gettext` (o
  `i18n_check` usa `xgettext`, `msgcat`, `msguniq`, `msgcmp`); `awk` e `grep`
  já estão no base.
- O pacote passa a instalar `/usr/share/locale/pt_BR/LC_MESSAGES/wlmaker.mo`
  (o `cmake --install` cuida). Nenhuma dependência nova em tempo de execução
  (libintl é da glibc). O catálogo `wmake-zones.mo` (nomes dos layouts) vem
  do pacote wmake-zones.
- `check()`: o teste de tradução precisa de um locale UTF-8 gerado que não
  seja C (en_US.UTF-8 no chroot do Arch); sem nenhum, ele só verifica o
  fallback e avisa no registro.
- A verificação "nenhum arquivo cita `$srcdir`" não é afetada (`.mo` é binário
  e o LOCALEDIR compilado é `/usr/share/locale`).
- Sem migração de configuração: os menus do usuário não mudam.
