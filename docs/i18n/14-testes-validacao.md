# i18n do big-wm — 14. Testes e validação

## tests/run.sh, seção 9 (Traduções)

Rodar: `env -u DISPLAY -u WAYLAND_DISPLAY PATH=$PATH:<venv>/bin bash tests/run.sh`.
As ferramentas usam os catálogos do checkout: depois do `make install`
(passo 5) o teste exporta `BIGWM_LOCALEDIR=$destdir/usr/share/locale`
(sem isso os testes do menu Aplicativos e do launcher, que esperam
português, leriam o `.mo` do sistema ou nenhum).

- `msgfmt --check` em cada catálogo, um por chamada (`po/*.po`,
  `welcome/po/*.po`, `tabbed-dock/po/*.po`).
- `po/POTFILES` ausente, vazio ou com arquivo inexistente: falha. Todo
  arquivo que usa o domínio big-wm (`_TRANSLATION`, `_t()`,
  `bindtextdomain("big-wm"`, `= appmenu._`) tem de estar no POTFILES.
- `make pot` num diretório temporário: os msgids têm de bater com
  `po/big-wm.pot` e `welcome/po/big-wm-welcome.pot` (string nova sem `make
  update-po` falha), e `po/pt_BR.po`, `welcome/po/pt_BR.po` e
  `tabbed-dock/po/pt_BR.po` têm de estar completos (sem "untranslated" nem
  "fuzzy" depois do `msgmerge` com o POT novo); nenhum aviso do xgettext;
  `Plural-Forms` do pt_BR.
- `tests/i18n/lint.py` nas ferramentas da sessão: o esquema antigo
  (`def tr(pt, en)`, `if PT else`, `startswith("pt")`, `{"pt": {…}}`,
  `case ${LANG%%_*}` sem `i18n-lint: ignorar`), texto de interface fora de
  `_()`/`N_()`/`ngettext()` em `label=`, `title=`, `text=`,
  `tooltip_text=`, `set_title()`, `add_button()`... e em listas do
  `notify-send`; em shell, `notify-send`/`zenity`/`yad` com texto literal
  fora de `$(_t …)`; em C, `notify-send` com literal fora de `_()`. Com
  `--catalog po/pt_BR.po` (só nas fontes do POTFILES): texto em português
  do catálogo que ficou no código. O próprio lint é testado com um exemplo
  que tem de dar três achados.
- Cada programa Python do POTFILES, carregado com `runpy` (sem abrir
  janela), com `LANG=pt_BR.UTF-8` ("Cancelar", "Ampliação da tela"),
  `en_US.UTF-8` ("Cancel", "Screen zoom") e `es_ES.UTF-8` ("Cancelar" do
  es.po, "Screen zoom" na queda para o inglês).
- `big-wm-terminal --dir=/nada/aqui` nos três idiomas (mensagem de erro);
  `_t_layout "Session locked"` do `big-wm-lock` em pt_BR; o
  `big-wm-locate-pointer --disable` compilado escreve "Localizar o
  ponteiro: desligado" / "Locate pointer: off" / "Locate pointer: off".
- `tests/i18n/menus.py`: `WMRootMenu.pt_BR` e `RootMenu.plist.pt_BR` com a
  mesma estrutura, os mesmos comandos, atalhos e ações do inglês (a
  pergunta do `%a(...)` pode ser traduzida) e nenhum rótulo igual ao
  inglês fora de uma lista de nomes próprios; testado com um menu
  estragado de propósito. Resultado: os dois pares batem, nenhuma
  divergência a relatar.
- `.desktop` próprios: `Comment`, `GenericName` e `Keywords` com `[pt_BR]`;
  `Name` em português só com `Name[pt_BR]` e o inglês na chave padrão.
- `big-wm-session-ctl start` (systemctl e dbus-update-activation-environment
  falsos): leva `LANG`, `LANGUAGE`, `LC_*`; sem `LANG` não mexe; nunca
  `LC_ALL` (ver `13-integracao-sessoes.md`).

Resultado final nesta branch: 44 "ok" (shellcheck incluído) e as duas falhas
`firstrun: lançadores ... do Big WindowMaker`, que também falham na base
`feat/missao-integracao` sem nenhuma mudança deste trabalho (o teste
depende dos `.desktop` org.bigwindowmaker.* instalados no sistema).

## Laboratório (capturas)

`/home/ruscher/.cache/big-wm-lab/mklab.sh i18n`; as ferramentas desta
branch rodando com `BIGWM_LOCALEDIR=<worktree>/po/build`, `LANG` e
`LANGUAGE` de cada idioma. Capturas em
`/home/ruscher/.cache/bwm-wt/i18n/capturas/big-wm/` (montagens
`x11-*-montagem.png` e `wl-*-montagem.png`: pt_BR, en_US, es_ES da
esquerda para a direita; originais em `x11/` e `wayland/`).

- X11 (`x-up.sh`, wmaker instalado): airplay, hp (sem o HPLIP no PATH),
  scrcpy (sem o scrcpy no PATH), lock-settings, clipboard, launcher (apps e
  run), about. O lock-settings não abria na primeira rodada: era uma linha
  da tabela ainda com o par (pt, en) — corrigido e recapturado.
- Wayland (`wl-up.sh` com o `/usr/bin/wlmaker` instalado; o build
  compartilhado do laboratório recusava o menu raiz atual): about,
  dock-settings, kill (com uma janela aberta), clipboard, lock-settings,
  bandeja e o menu do ladrilho (o menu é do wlmaker).
- Microfone na bandeja: menu e dica lidos pelo D-Bus num barramento
  próprio (`mic-tray-menu.txt`): traduzidos em pt_BR, inglês em en_US e
  es_ES.

O espanhol aparece misturado onde só parte dos textos tinha espanhol (ex.:
o quadro "Teclado en pantalla" do lock-settings vem do texto do bloqueio,
que já existia em espanhol); é o comportamento de antes, agora num
catálogo que alguém pode completar.

## Lotes para o revisor

`/home/ruscher/.cache/bwm-wt/i18n/dominio-big-wm/lote01..06.{pending,output}.jsonl`
(297 entradas do pt_BR; o "tradutor" foi o texto que já existia, revisado
pelo glossário). `verify_translation_output.py --target-lang pt-BR`:
limpo nos seis. `check_ui_text.py --width 26 --max-lines 1` nos msgids de
até 20 colunas: quatro casos de duas linhas, nenhum é rótulo estreito
(item de menu do dockapp, mensagem e notificações).
