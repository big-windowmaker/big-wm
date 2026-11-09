# i18n 08 — wmake-zones (pt_BR)

Domínio gettext: `wmake-zones` (`/usr/share/locale/<lang>/LC_MESSAGES/wmake-zones.mo`),
um só para o serviço do X11, o dockapp do X11, o dockapp do Wayland
(`wmake-zones-wl`) e as preferências/editor (`wmake-zones-prefs`).

Worktree: `~/.cache/bwm-wt/i18n-zd/wmake-zones`, branch `i18n/pt-br-2026-11`,
base `origin/main` (56364ad, #8). O pacote instalado (0.3.1-5) é a tag
`v0.3.1` + patches 0001–0005 do `packages/wmake-zones`; os commits desta
branch aplicam limpos sobre essa árvore (conferido: build + `meson test po engine`).

## Mecanismo do projeto

- Extração: `po/meson.build` (`i18n.gettext`, preset `glib`, `--keyword=N_`),
  lista em `po/POTFILES`, alvo `meson compile -C BUILD wmake-zones-pot`.
- Mescla: `msgmerge` (o projeto guarda as obsoletas `#~`; mantido).
- `.desktop`: traduções escritas à mão nos próprios arquivos (`Name[pt_BR]`...),
  sem passar pelo POT; mantido.
- Plural-Forms: `nplurals=2; plural=(n > 1);` — o mesmo dos `.mo` instalados
  do GTK 3, GLib, coreutils e gettext-runtime (msgunfmt). O catálogo já usava.

## O que foi feito

1. **POT regenerado.** O POT do repositório estava atrás do código (faltavam
   as mensagens do PR #5/#6: "Window Maker options...", "Application
   compatibility", "Always _float this application", a ajuda com `windows -d`...).
   `src/prefs/wzp-draw.c` entrou no `POTFILES` (traduz nomes de zona com `_(nome)`).
2. **Texto fora da extração achado e marcado** — os motivos do mosaico na
   página Regras → Compatibilidade de aplicativos. A coluna mostrava o texto do
   protocolo `windows -d` como chega do serviço, em português fixo
   ("mosaico desligado neste monitor", "não cabe: mínimo 760x500, célula ...")
   para qualquer idioma. O texto do protocolo é o mesmo no serviço do X11 e no
   wlmaker (`src/zones.c`) e os testes dos dois procuram por ele, então ele
   ficou; as preferências agora o convertem em rótulos traduzíveis
   (`compat_reason()`, tabela com `N_()` + `sscanf` dos formatos com tamanhos,
   e o sufixo " (mínimo LxA)" do wlmaker). Motivo desconhecido aparece como veio.
   17 msgids novos + 3 com formato (`%d×%d`).
3. **pt_BR completo**: a ajuda (`--help`) recebeu a linha `windows [-l|-d]` e
   "Zones or Auto Tiling" virou "as Zonas ou o Mosaico automático"; os 20
   novos traduzidos pelo glossário.
4. `po/glossary/pt_BR.tsv` (glossário comum + termos do domínio: Mosaico
   automático, Flutuante, Enrolada, Encaixar, Principal + pilha).
5. `Keywords[pt_BR]` no `wmake-zones-prefs.desktop` (as palavras em português
   estavam misturadas no `Keywords` em inglês).

Não traduzido de propósito: comandos e argumentos (`zones toggle`, `mode`,
`windows -d`...), nomes de algoritmo usados como chave (`master`, `bsp`...),
chaves do arquivo de configuração, nomes de teclas (`Super+Ctrl+Left`), unidades.

## Cobertura (msgfmt --statistics)

| | msgids | pt_BR |
|---|---|---|
| antes (catálogo do `origin/main` contra o código) | 277 | 276 traduzidas, 1 fuzzy (a ajuda) |
| depois | 297 | 297 traduzidas |

## Testes

- `tests/po-check.sh` (meson test `po`, TAP): falha alto sem `po/POTFILES`,
  `po/LINGUAS` ou o POT; arquivo listado que não existe; fonte com `_()`/`N_()`
  fora da lista; rótulo/dica/título do GTK com literal cru; POT diferente do que
  o código extrai (msgcmp nos dois sentidos, mesmas palavras-chave do preset
  glib); `msgfmt --check` um catálogo por chamada; catálogo sem as mensagens do
  POT. Conferido que falha: sem POTFILES, com um arquivo inexistente na lista,
  com `gtk_label_set_text(..., "Layout saved")`, com POT velho.
- `meson test`: engine, x11-functional (56), x11-dockapp (17), metainfo, po (8)
  OK; x11-multimonitor pulado (sem Xephyr).
- `check_ui_text.py --width 24 --max-lines 1` nos motivos curtos: ok.
- Harness `verify_translation_output.py` no lote: limpo.
- Laboratório (`~/.cache/big-wm-lab-i18n-zd`, build instalado em
  `~/.cache/bwm-wt/i18n-zd/stage`), `LANG` pt_BR/en_US/es_ES (es cai no inglês):
  - X11 (wmaker num Xvfb): todas as páginas das preferências, Sobre, editor,
    Compatibilidade com duas janelas (motivo traduzido em pt_BR, inglês em
    en/es), menu do dockapp, overlay de zonas.
  - Wayland (wlmaker aninhado): todas as páginas, Sobre, editor; dockapp
    `wmake-zones-wl` no Dock.
  - Capturas: `~/.cache/bwm-wt/i18n/capturas/wz-*-{x11,wl}-{pt_BR,en_US,es_ES}.png`.
  Nenhum texto cortado nem acento quebrado nas capturas.

Lotes para o revisor: `~/.cache/bwm-wt/i18n/wmake-zones/lote1.{pending,output}.jsonl`.

## Limitações

- O protocolo `windows -d` (saída de linha de comando e a página Avançado →
  Diagnóstico) continua em português, no serviço do X11 e no wlmaker: é texto
  de diagnóstico que os testes dos dois conferem. Traduzir lá pede mudar os
  dois lados e os testes juntos.
- Mensagens de erro do protocolo (`error=modo desconhecido`) e os registros
  (`wzx_log`) continuam em português (diagnóstico).
- Os `.desktop` seguem sem passar pelo gettext (mecanismo atual do projeto).
- A tradução é do tradutor; falta a revisão independente pela rubrica.
