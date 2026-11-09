# i18n 09 — Big WM Display Resolution (pt_BR)

Domínio gettext: `big-wm-display-resolution`
(`/usr/share/locale/pt_BR/LC_MESSAGES/big-wm-display-resolution.mo`, instalado
pelo `make install`).

Worktree: `~/.cache/bwm-wt/i18n-zd/big-wm-display-resolution`, branch
`i18n/pt-br-2026-11`, base `integ-2026-11` (a6f848e, o instalado, com a
rolagem do WINGs).

## Mecanismo do projeto

- `make po`: `xgettext` (Python) e `po/make-pt_BR.py`, que **gera** o
  `pt_BR.po` do POT e de um dicionário de traduções (o `.po` não é editado à
  mão). Mantido; o script passou a escrever entradas de plural (tupla
  `(forma 0, forma 1)`), e o alvo lê a lista de fontes de `po/POTFILES`
  (`XGETTEXT_ARGS` com `P_:1,2` e `ngettext:1,2`).
- Plural-Forms: `nplurals=2; plural=(n > 1);` — o dos `.mo` instalados do
  GTK 3, GLib, coreutils e gettext-runtime (msgunfmt).
- `.desktop`: `[pt_BR]` escrito nos arquivos (mantido).

## O que foi feito

1. **Plurais de verdade** (antes: "%d monitor(es)", "em %d s"):
   - contagem do diálogo: "Reverting to the previous configuration in %d
     second(s)." → "Voltando à configuração anterior em %d segundo(s)." A
     contagem vai de 15 a 1 (com 0 o diálogo já respondeu); com a regra
     `(n > 1)` o 1 é singular, e o 0 também seria;
   - "%d monitor connected / monitors connected" (status) e o prazo do aviso
     de mudanças pendentes.
2. **"Keep these display settings?"** → "Manter estas configurações de
   monitores?" (pedido do usuário).
3. **Gênero quebrado**: "Adaptive sync is %s" recebia o "off" compartilhado
   com "Atual: desligado" — saía "A taxa variável está desligado". Agora são
   duas frases inteiras (ligada / desligada).
4. **Texto fora da extração achado** (antes só `ui.py` era extraído):
   - `cli.py`: a ajuda (`--help`/`-h`, que agora sai com 0; antes o docstring
     em português) e as mensagens de `--show`, `--profiles`,
     `--apply-profile` estavam fixas em português; agora inglês no código e
     traduzidas;
   - `backend.py`: `layout_summary()` (cada monitor no `--show`) escrevia
     "desligado", "espelhando", "em", "principal" para qualquer idioma.
   - `po/POTFILES` com `ui.py`, `cli.py`, `backend.py`; `po/LINGUAS`.
5. Glossário (`po/glossary/pt_BR.tsv`): "Reset" → "Redefinir" (era
   "Desfazer", que o glossário reserva para Undo). "Refresh rate:" ficou
   "Frequência:" de propósito: "Taxa de atualização:" alargou a janela de 866
   para 929 px (medido no laboratório); "Taxa de atualização" fica no texto e
   no VRR. `GenericName[pt_BR]` → "Configurações de monitores".

Não traduzido: opções (`--restore`, `--show`...), conectores (DP-1,
HDMI-A-1), EDID, VRR/HDR/FreeSync, Hz, mm, nomes de rotação do xrandr
(`normal`, `90`...), "X11"/"Wayland".

## Cobertura (msgfmt --statistics)

| | msgids | pt_BR |
|---|---|---|
| antes (só `ui.py`; CLI e resumo fixos em português) | 115 | 115 |
| depois (`ui.py`, `cli.py`, `backend.py`) | 127 (5 com plural) | 127 |

## Testes

- `tests/test_i18n.py` (no `make check`): `po/POTFILES`/`LINGUAS` existem
  (falha alto), todo fonte com `_()`/`P_()`/`ngettext` está na lista, nenhum
  rótulo/botão/dica do GTK com literal cru, POT igual ao que o código extrai
  (msgcmp nos dois sentidos, argumentos lidos do Makefile), `msgfmt --check`
  por catálogo sem não traduzidas/fuzzy, `make-pt_BR.py` gera exatamente o
  `pt_BR.po` do repositório, `--help` em pt_BR ("Uso:"), en_US e es_ES
  ("Usage:", es cai no inglês) com as opções intactas, `--show` em pt_BR e os
  plurais (1 e 15 segundos, 0 e 2 monitores) e o cabeçalho do `.mo`.
  Conferido que falha sem POTFILES e com `"Keep"` sem `_()`.
- `test_guard_cli.py::test_cli_entry_points` agora roda em `C.UTF-8` sem
  catálogo (esperava "nenhum" fixo).
- `make check`: 75 passed.
- `check_ui_text.py --width 22 --max-lines 1` nos rótulos estreitos: ok.
  Harness nos lotes: limpo.
- Laboratório com o backend de teste (arquivo JSON: nada é aplicado em
  monitor nenhum), `tests/lab/i18n_lab.py`: janela principal, aviso de
  mudança pendente, diálogo "Manter..." com a contagem (respondido com
  Reverter), janela depois da reversão, Salvar/Renomear/Excluir perfil e o
  segundo monitor (Informações), em X11 (wmaker/Xvfb) e Wayland (wlmaker
  aninhado), LANG pt_BR/en_US/es_ES. `--show` no X11 do Xvfb (só leitura).
  Capturas: `~/.cache/bwm-wt/i18n/capturas/dr-*-{x11,wl}-{pt_BR,en_US,es_ES}.png`.
  Sem texto cortado; as Informações reticenciam valores longos em todos os
  idiomas (dica com o texto inteiro), como antes.

Lotes para o revisor: `~/.cache/bwm-wt/i18n/big-wm-display-resolution/`
(`lote1` novos, `lote2-termos` troca pelo glossário, `lote3` resumo do `--show`).

## Limitações

- Registros (`logger`, `BIGWM_DISPLAY_VERBOSE`) e erros internos
  (`migrate.py`, `guard.py`) seguem em português: diagnóstico.
- Três msgids antigos usam argumentos posicionais (`%d–%d Hz`,
  `%d × %d mm (%.1f″)`, `%s at %d, %d`): o xgettext avisa; a ordem serve
  ao pt_BR.
- O status "%d monitor connected... this monitor" no singular vale também
  para 0 com `(n > 1)`; com 0 monitores a janela não tem o que mostrar.
- A tradução é do tradutor; falta a revisão independente pela rubrica.
