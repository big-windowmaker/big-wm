# Relatório final — tradução e internacionalização do Big WindowMaker (novembro de 2026)

Instalado na máquina de desenvolvimento em 09/11/2026. Nada publicado
(sem push, PR ou ISO). Os documentos por componente estão nesta pasta
(01 a 14); este resume o resultado, os números e as pendências.

## O que foi feito

- **Um domínio gettext por produto**, `.mo` em
  `/usr/share/locale/<idioma>/LC_MESSAGES/<domínio>.mo`: WindowMaker,
  WPrefs, WINGs, WRaster, wmgenmenu, wlmaker, big-wm, big-wm-welcome,
  big-wm-tabbed-dock, wmake-zones e big-wm-display-resolution
  ([02-arquitetura-gettext.md](02-arquitetura-gettext.md)).
- **wlmaker**: a tabela fixa de traduções deu lugar ao gettext (domínio
  `wlmaker`; `LC_NUMERIC` continua "C" por causa do `strtod` dos plists).
  Menus de janela, Dock, Clip, gaveta, appicons, diálogos, Alt+Tab e
  dockapps nativos traduzidos ([05-wlmaker-wayland.md](05-wlmaker-wayland.md)).
- **Window Maker**: o pt_BR antigo (263 traduzidas e 85 fuzzy) foi
  atualizado e revisado; WPrefs, WINGs, WRaster e wmgenmenu ganharam pt_BR
  (antes o glibc caía no `pt` europeu). `wmaker --help` e os erros de
  argumento passaram a ser traduzidos (o catálogo era carregado depois da
  linha de comando). "Sem título" na lista de janelas
  ([03](03-windowmaker-x11.md), [04](04-wprefs.md)).
- **Ferramentas da sessão** (big-wm): o esquema `tr(pt, en)` deu lugar ao
  gettext com msgids em inglês, em Python, C e shell; `.desktop` com o
  inglês padrão e `[pt_BR]` ([10-ferramentas-auxiliares.md](10-ferramentas-auxiliares.md)).
- **wmake-zones e Display Resolution**: POTs em dia, plurais, linha de
  comando traduzida e testes de catálogo ([08](08-wmake-zones.md), [09](09-display-resolution.md)).
- **Idioma da sessão**: `LANG`/`LANGUAGE` chegam ao wmaker, ao wlmaker e
  aos filhos; `big-wm-session-ctl` exporta `LANG` e `LC_*` ao systemd --user
  e ao D-Bus; `LC_ALL` nunca é definido ([13-integracao-sessoes.md](13-integracao-sessoes.md)).
- **Glossário e estilo**: `po/glossary/pt_BR.tsv` com a origem de cada
  escolha ([11-glossario-pt-br.md](11-glossario-pt-br.md)); Plural-Forms
  `nplurals=2; plural=(n > 1);`, tirado de gtk30.mo, glib20.mo, coreutils.mo
  e gettext-runtime.mo instalados ([12-plurais-placeholders.md](12-plurais-placeholders.md)).

## Revisão

Cada catálogo foi traduzido por um agente e revisado por outro, contra a
rubrica do skill gettext-translation (sentido, placeholders, plurais,
aceleradores, consistência X11 × Wayland, largura).

| Domínio | Revisadas | Corrigidas na revisão |
|---|---|---|
| WindowMaker | 437 | 52 |
| WPrefs | 592 | 12 |
| WINGs | 175 | 2 |
| WRaster | 34 | 0 |
| wmgenmenu | 493 | 1 |
| wlmaker | 194 | 5 (+1 do integrador: Unshade) |
| big-wm | 297 | 18 |
| big-wm-welcome | 163 | 1 |
| wmake-zones | 297 | revisado (+2 do integrador: Minimizada) |
| big-wm-display-resolution | 127 | revisado |

Decisões de vocabulário que valem para as duas sessões: Minimizar,
Sombrear / Tirar sombra, Manter sobre outros, Terminar aplicação, Definir
ícone..., Ocultar / Mostrar, Remover a última, Recolhido, gaveta, OK.

## Cobertura (catálogos instalados, `msgfmt --statistics` contra o POT atual)

| Domínio | pt_BR traduzidas | fuzzy | vazias | Antes |
|---|---|---|---|---|
| WindowMaker | 438 | 0 | 0 | 263 + 85 fuzzy |
| WPrefs | 593 | 0 | 0 | sem pt_BR |
| WINGs | 175 | 0 | 0 | sem pt_BR |
| WRaster | 34 | 0 | 0 | sem pt_BR |
| wmgenmenu | 493 | 0 | 0 | sem pt_BR |
| wlmaker | 194 | 0 | 0 | tabela fixa no código |
| big-wm | 297 | 0 | 0 | `tr(pt, en)` no código |
| big-wm-welcome | 164 | 0 | 0 | 163 |
| big-wm-tabbed-dock | 159 | 0 | 0 | 159 |
| wmake-zones | 297 | 0 | 0 | incompleto |
| big-wm-display-resolution | 127 | 0 | 0 | incompleto |
| **Total** | **2971** | **0** | **0** | |

Espanhol (catálogos que já existiam; não traduzimos outros idiomas, só
`msgmerge`): WPrefs 391/593, WindowMaker 317/437, big-wm 54/297, wmgenmenu
64/493. O resto cai no inglês, nunca no português.

## Testes executados

- `msgfmt --check`, um catálogo por chamada, em todos os domínios.
- big-wm `tests/run.sh`: 43 verificações, todas passando, inclusive a
  seção Traduções (POTFILES, POT em dia, texto sem marcação, ferramentas em
  pt_BR/en_US/es_ES). A suíte deixou de depender do big-wm instalado e da
  sessão de quem roda (correções 3643d1d e 9c0d14e).
- wlmaker `ctest` 29/29 (sem o `wlmaker_test`, que às vezes trava num
  subprocesso do teste do menu raiz — conhecido, não ligado à tradução) e
  `i18n_check`.
- Window Maker `make check`; wmake-zones `meson test` (5 ok, 1 pulado:
  multimonitor); Display Resolution `make check` 75 ok.
- Validação na tela com os pacotes instalados: X11 e Wayland × pt_BR,
  en_US e es_ES, login simulado sobre uma cópia da configuração, 176
  capturas. pt_BR sem inglês nas telas do projeto; en_US em inglês; es_ES
  em espanhol onde há catálogo e inglês no resto. `LC_ALL` ausente em todos
  os processos.
- Desempenho: wlmaker até o socket 12–19 ms (pt_BR) × 12–18 ms (C); wmaker
  até o WM aparecer 54–65 ms × 48–56 ms (ruído). Nenhum msgfmt/xgettext em
  tempo de execução.

## Defeitos achados pela validação e corrigidos

| Defeito | Correção |
|---|---|
| Rótulos cortados nos menus do wlmaker (224 px): "Mosaico automático: ligar/desligar", "Configurações avançadas de rede", "Window Organization Preferences..." | Menus de 248 px nos temas claro e escuro |
| Título da janela do Welcome fixo em inglês | Traduzível: "Boas-vindas do Big WindowMaker" |
| "Untitled" fixo na barra de título e na lista de janelas do wmaker | `_()` no título padrão da moldura e da lista: "Sem título" (conferido na tela) |
| Catálogo es do Window Maker traduzia a opção `--version` como `--versión` | Nome da opção restaurado |
| `wmaker --help` sempre em inglês | Catálogo carregado antes da linha de comando |
| WPrefs recusava o comando RESET_DOCK do menu | Comando conhecido; "Restaurar posição do Dock" |

## Pacotes instalados

wlmaker 1:0.8.r80.g7481afe-28, windowmaker 0.96.0.r130.g558e67f-21,
wmake-zones 0.3.1-6, big-wm-display-resolution 26.11.09-0644 e big-wm-*
26.11.09-0756 (11 pacotes). O wlmaker novo vale a partir do próximo login.

## Limitações e pendências

- **Terceiros sem catálogo**: wmcliphist e outros dockapps clássicos ficam
  em inglês.
- **Outros idiomas do upstream do Window Maker**: catálogos cs, fi, ja,
  no, pt e tr também trocam nomes de opções no `--help` (`--nodock`,
  `--versiyon`...); não mexemos em idiomas que não mantemos. Vale enviar ao
  upstream junto com a correção do carregamento do catálogo.
- **Mensagens de terminal** (`uso: …` no stderr de big-wm-hp e
  big-wm-zoom) seguem em português, sem marcação.
- **Não testado**: systemd --user real (o laboratório não tem); submenu
  Sessão (de propósito, por segurança).
- **Decisões do usuário (09/11/2026), aplicadas nos catálogos sem mexer
  no menu raiz** (sem migração de menus): "Remover a última" no X11 e no
  Clip do wlmaker, como no menu raiz; "Ocultar" no lugar de "Esconder" em
  WindowMaker, WPrefs, wmgenmenu e wlmaker, como no menu raiz e no Dock com
  abas; o submenu gerado pelo item "Conjuntos de ícones" passou a ter o
  mesmo nome (antes "Tema de ícones"; msgid "Icon sets").
