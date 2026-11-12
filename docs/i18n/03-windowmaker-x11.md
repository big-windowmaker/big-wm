# i18n pt_BR do Window Maker X11 (WindowMaker, WINGs, WRaster, wmgenmenu)

Agente i18n-wmaker, 09/11/2026. Worktree do wmaker:
`~/.cache/bwm-wt/i18n/wmaker`, branch `feat/i18n-pt-br-wmaker` (base
`fix/x11-borderless-games`, o windowmaker 17 instalado). O WPrefs está em
[04-wprefs.md](04-wprefs.md).

## Problema

- `po/pt_BR.po` (domínio `WindowMaker`) era de 2000 (0.60.0): `Language: pt`,
  sem `Plural-Forms`, "379 traduzidas" mas só **263 de 437** casavam com o
  código atual (85 viravam fuzzy e 89 não tinham tradução). Com o `.mo`
  instalado, cada mensagem sem tradução cai no catálogo `pt` (europeu) e,
  se faltar lá também, no inglês: metade do Inspetor, as gavetas, o menu de
  maximização e o diálogo de saída apareciam misturando pt_BR, pt_PT e
  inglês.
- WINGs (175 mensagens: painéis de arquivo, fonte e cor, alertas), WRaster (34)
  e wmgenmenu (493) não tinham pt_BR. O pacote instalava em
  `/usr/share/locale/pt_BR/LC_MESSAGES/` só o `WindowMaker.mo`
  (`pacman -Ql windowmaker | grep LC_MESSAGES`).
- Várias traduções antigas estavam erradas: "could not stat menu" como "não
  foi possível menu rápido", "screen" como "janela", "key" (chave de
  configuração) como "tecla", "Docked Application Settings" como
  "Configurações de Aplicação na posição Dock", "fallou", "banco de datos",
  "tirar sombra" em minúscula.
- `make check` falhava em `po/`: o `appcommand.c` (patch do Dock do Big
  WindowMaker) entrou em `wmaker_SOURCES` mas não em `POTFILES`, e o
  `script/check-translation-sources.sh` do upstream exige os dois iguais
  (o PKGBUILD roda `make check`).

## Auditoria do código

- Inicialização: `src/main.c` (setlocale, `bindtextdomain("WindowMaker")` e o
  `MENU_TEXTDOMAIN`, `bind_textdomain_codeset(..., "UTF-8")`, `textdomain`);
  `WINGs/wapplication.c` (domínio `WINGs`, UTF-8); `WPrefs.app/main.c`
  (`WPrefs`); `util/wmgenmenu.c` (`wmgenmenu`); `wrlib/misc.c` (construtor da
  biblioteca, `bindtextdomain("WRaster")` sem `bind_textdomain_codeset`: as
  mensagens vão para o stderr no charset do locale, o que dá UTF-8 nos locais
  .UTF-8). Todos usam `NLSPATH` como diretório de catálogos quando definido,
  senão `LOCALEDIR` = `/usr/share/locale`.
- Macros: `_()` = `gettext`/`dgettext(domínio)` (WINGs `wconfig.h`, wrlib
  `wr_i18n.h`) e `N_()` no-op para tabelas (menu de maximização, atributos
  do Inspetor, ações do WPrefs, programas do wmgenmenu). Os `po/Makefile.am`
  extraem com `xgettext --keyword=_ --keyword=N_` (alvo `<domínio>.pot`) e
  atualizam com `make update-lang PO=<lang>` (`script/generate-po-from-template.sh`,
  que é um `msgmerge --update`).
- Idiomas: `m4/wm_i18n.m4` lista os `.po` de cada `po/` na hora do
  `autoreconf` e instala os pedidos em `LINGUAS`; o PKGBUILD passa
  `LINGUAS` = os `.po` de `po/`, que já inclui `pt_BR`. Basta o arquivo
  existir (e o `autoreconf -fiv` que o PKGBUILD já faz). Os `EXTRA_DIST`
  ganharam `pt_BR.po` (só para o `make dist`).
- O `.pot` não é versionado no upstream (é `CLEANFILES`): mantido assim.
- Strings dos patches do Big WindowMaker: painel Informações ("Based on
  BigLinux", créditos), menus de posição e de monitor do Dock ("Primary
  monitor", "Monitor %d: %s (%ux%u)", "Restore Dock position" etc.) já usam
  `_()` e estão no POT; "Big WindowMaker", "Big WindowMaker Custom",
  "Copyleft" e os nomes dos autores são marca/nome e ficam sem tradução de
  propósito. O `appcommand.c` não tem texto visível (só monta comandos);
  entrou no `POTFILES` para o `make check`. O `wmagnify` (patch 0010) não tem
  gettext; o título "Magnify %sx%s" e a ajuda ficam em inglês (fora do
  escopo; ver limitações).
- Strings visíveis fora da extração (busca por literais em `WMSetLabelText`,
  `WMSetButtonText`, `wMenuAddCallback`, `WMAddPopUpButtonItem`, tabelas):
  nenhuma nova. Ficam sem marcação, de propósito: "Mod1".."Mod5", "msec",
  "FF", "X", nomes de fonte e o texto de exemplo "The Lazy Fox Jumped Ipsum
  Foobar 1234" do WPrefs (código, unidades ou amostra).

## Solução

| Catálogo (domínio) | Antes (casando com o código atual) | Depois |
|---|---|---|
| `po/pt_BR.po` (WindowMaker) | 263/437 (85 fuzzy, 89 vazias) | 437/437 |
| `WPrefs.app/po/pt_BR.po` (WPrefs) | não existia (0/592) | 592/592 |
| `WINGs/po/pt_BR.po` (WINGs) | não existia (0/175) | 175/175 |
| `wrlib/po/pt_BR.po` (WRaster) | não existia (0/34) | 34/34 |
| `util/po/pt_BR.po` (wmgenmenu) | não existia (0/493) | 493/493 (394 nomes de programas iguais ao inglês) |

Números de `msgfmt --check --statistics`. As 74 entradas obsoletas `#~` do
`po/pt_BR.po` ficam (o `update-lang` do projeto as mantém, e outros catálogos
do upstream também têm).

- Cabeçalhos: `Language: pt_BR`, `Content-Type: text/plain; charset=UTF-8`,
  `Plural-Forms: nplurals=2; plural=(n > 1);` (fonte: `msgunfmt` de
  gtk30.mo, glib20.mo, coreutils.mo e gettext-runtime.mo pt_BR instalados;
  nenhuma mensagem do Window Maker usa plural hoje). Os tradutores históricos
  (Alfredo Kojima, Cyro Mendes de Moraes Neto, Marcia Norie Nakaza, Eliphas
  Levy Theodoro) continuam nos comentários.
- Vocabulário: o guia (`ESTILO-pt_BR.md`) diz que o histórico do
  `po/pt_BR.po` é "Miniaturizar/Enrolar/Manter em cima", mas o catálogo
  instalado desde 2000 (e o espelho do wlmaker em `src/i18n.c`) usa
  **Minimizar, Sombrear, Manter sobre outros, Terminar**. Mantive o catálogo
  real e usei os mesmos termos no WPrefs/WINGs; o glossário do projeto
  (`po/glossary/pt_BR.tsv` no wmaker) registra isso com a origem. Se o
  coordenador preferir "Miniaturizar/Enrolar", é uma troca de ~15 entradas
  nos catálogos X11 **e** no `wlmaker/src/i18n.c`.
- Mudanças de rótulos que o wlmaker espelha (para manter as duas sessões
  iguais, o wlmaker `src/i18n.c` precisa acompanhar): "OK" (era "Ok"),
  "Recolhido" (era "Minimizado", para o Clip/gaveta recolhidos, já que
  "Minimizar" é Miniaturize), "Mostrar aqui", "Remover ícone", "Terminar
  aplicação" (maiúsculas só na primeira palavra, como o guia pede e como os
  menus novos do Dock já faziam).
- Termos de diretórios: "diretório" nos três domínios (como o catálogo
  WindowMaker já usava), não "pasta".

Commits (branch `feat/i18n-pt-br-wmaker`):

- `f38b7e62` po: add appcommand.c to the translation sources
- `38241f70` po/pt_BR: Update to the current sources
- `92980df1` WPrefs.app/po: Add the Brazilian Portuguese translation
- `ca0cf290` WINGs/po: Add the Brazilian Portuguese translation
- `be2abfb3` wrlib/po, util/po: Add the Brazilian Portuguese translations
- `a9e5e209` po: Brazilian Portuguese glossary of the Big WindowMaker translations

Lotes para o revisor (pending.jsonl + output.jsonl):
`~/.cache/bwm-wt/i18n/{WindowMaker,WPrefs,WINGs,WRaster,wmgenmenu}/lote-NN/`;
correções feitas em traduções existentes e depois do laboratório:
`~/.cache/bwm-wt/i18n/WindowMaker/revisao-existentes.*.jsonl`,
`~/.cache/bwm-wt/i18n/WPrefs/revisao-laboratorio/`,
`~/.cache/bwm-wt/i18n/WINGs/revisao-laboratorio/`. A tradução **não** está
revisada: falta o revisor independente.

## Testes

- Build no worktree com as opções do PKGBUILD (`autoreconf -fi`, `configure
  ... LINGUAS=<os .po de po/>`, `make`): "Translated languages ... pt_BR";
  `make DESTDIR=... install` gera os cinco
  `usr/share/locale/pt_BR/LC_MESSAGES/{WindowMaker,WPrefs,WINGs,WRaster,wmgenmenu}.mo`.
- `make check`: passa (antes falhava em `po/` com "source file appcommand.c is
  not in the translation list").
- `msgfmt --check` de cada catálogo, um por chamada: ok. Placeholders:
  `check_po_placeholders.py` ok nos cinco; harness
  `verify_translation_output.py` limpo em todos os lotes (uma falsa
  acusação no WPrefs lote-01: crases de `superfluous' lidas como código).
- Laboratório X11 (Xvfb próprio, wmaker e WPrefs do build, catálogos do
  DESTDIR via `NLSPATH`, safebin e sem barramento do sistema), capturas em
  `~/.cache/bwm-wt/i18n/wmaker-work/shots/`:
  - pt_BR: menu da janela e submenus (`ptbr-menu-janela*.png`), menu do
    ícone do aplicativo (`ptbr-menu-appicon.png`), Dock e submenus Posição/
    Monitor (`ptbr-menu-dock*.png`), Clip (`ptbr-menu-clip.png`), painel
    Informações (`ptbr-info.png`), diálogo de sair (`ptbr-dialogo-sair.png`),
    Inspetor (`ptbr-inspetor*.png`, `ptBR-final-inspetor.png`), painel de
    arquivo do WINGs (`wprefs/ptbr-wings-arquivo.png`).
  - en_US.UTF-8: tudo em inglês (`enUS-*.png`); es_ES.UTF-8: espanhol onde há
    `es.po` (WindowMaker, WPrefs) e inglês onde não há (WINGs), nada de
    português vazando (`esES-*.png`, `m-en-es-*.png`).
  - Cortes achados e corrigidos: "Desativar barra de redimensionamento"
    quebrava por cima da linha seguinte no Inspetor (agora "Desativar barra
    inferior"); "Salvar o estado das áreas de trabalho" quebrava no diálogo de
    sair ("Salvar a sessão"); "Mover Para" (maiúscula).

## Limitações e próximos passos

- Revisão independente pendente (rubrica do skill).
- WRaster não chama `bind_textdomain_codeset`: inofensivo em locais UTF-8.
- `wmagnify` (Lupa) e `wmiv` não têm gettext; ficam em inglês.
- O título da tabela do painel de aparência do WPrefs e outras áreas de pixel
  fixo foram conferidos só com a fonte padrão do laboratório (Noto Sans);
  fontes maiores podem cortar.
- Os menus do Big WindowMaker (WMRootMenu.pt_BR, `big-wm-appmenu`) são de
  outro domínio (big-wm) e não foram tocados.

## Integração (pacote windowmaker)

1. Acrescentar ao PKGBUILD os seis patches gerados por `git format-patch
   d40bfa5b..feat/i18n-pt-br-wmaker` (cópia em
   `~/.cache/bwm-wt/i18n/wmaker-work/patches/`, aplicam limpos sobre
   `fix/x11-borderless-games`), depois dos patches atuais, com comentário.
   O 0001 (`appcommand.c` em `POTFILES`) depende do patch que cria
   `src/appcommand.c` ("dock: a docked application's command launches it"):
   sem ele, `make check` reclamaria do contrário.
2. Nada muda em `build()`: o `LINGUAS` do PKGBUILD já tem `pt_BR` e o
   `autoreconf -fiv` do `prepare()` faz o configure enxergar os `.po` novos.
   Não há dependência nova (msgfmt já é usado no build).
3. Conferir no pacote: `pacman -Qlp windowmaker-*.pkg.tar.zst | grep
   pt_BR/LC_MESSAGES` deve listar os cinco `.mo`.
4. Sem migração de configuração. Avisar o agente do wlmaker sobre as
   mudanças de rótulos espelhados em `src/i18n.c` (lista acima).
5. Achado fora da i18n: o editor de menu do WPrefs não conhece o comando
   `RESET_DOCK` (patch 0009) e avisa "Comando de menu inválido ... foi
   removido" ao abrir o painel Menu; salvar o menu pelo WPrefs apagaria o item
   "Restaurar posição do Dock". Ver [04-wprefs.md](04-wprefs.md).
