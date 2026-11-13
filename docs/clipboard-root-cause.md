# Ctrl+V colava um favorito: causa-raiz e correção (2026-11-13)

**Sintoma.** Depois de copiar PRIMEIRO, SEGUNDO e TERCEIRO, o Ctrl+V colava
um item favorito ou mais antigo, e não TERCEIRO. Isso acontecia sobretudo
no Wayland.

## Causa (comprovada)

1. O `big-wm-clipboard --dump` listava `ORDER BY favorite DESC, used DESC`.
   O wmcliphist carrega essa lista de trás para frente, então **os favoritos
   ficavam no topo** da lista dele. Isso acontecia ao iniciar e a cada
   SIGUSR2, que chega a cada cópia de um cliente Wayland
   (`--store` → `notify_dockapp`).
2. Com `auto_take_up yes`, quando o dono da CLIPBOARD sumia, o wmcliphist
   republicava `g_list_first(history_items)`, que era o favorito.
3. No XWayland, o dono da CLIPBOARD é a ponte do wlroots (`xwm`). Ela larga
   a seleção (`XCB_WINDOW_NONE`, `xwayland/selection/selection.c`) quando a
   seleção do Wayland fica vazia, por exemplo quando o aplicativo que copiou
   fecha. O wmcliphist via "o dono sumiu" e, pelo `bigwm_copy` (wl-copy),
   punha o favorito na área de transferência do Wayland.

O resultado era duas peças cuidando da mesma coisa, com a ordem errada.

## Correção

O histórico, os favoritos e a área de transferência ativa ficam separados.
Em cada sessão, uma única peça guarda a última cópia:

| Sessão | Quem devolve a última cópia quando o aplicativo fecha |
|---|---|
| X11 | wmcliphist, com `last_copy`: o que foi copiado por último ou escolhido no menu. Fica vazio depois de uma imagem ou de uma senha. |
| Wayland | `big-wm-clipboard --store` com `CLIPBOARD_STATE=nil`: devolve o texto da marca `$XDG_RUNTIME_DIR/big-wm/clipboard-last` (só o hash), se ele era texto, ainda está no histórico e a área de transferência continua vazia. No XWayland, o wmcliphist não republica nada. |

- `--dump` lista os favoritos e os N itens mais recentes **por ordem de
  cópia**.
- `--watch` deixou de usar `--type text`. Assim, uma imagem copiada também
  conta como última cópia, e nenhum texto antigo volta no lugar dela.
- Ao iniciar, nada é colado sem ser escolhido: o histórico fica no menu e no
  Super+V.
- Abrir o Super+V ou o menu do wmcliphist não muda a área de transferência.
  Só escolher um item muda.

**Limite conhecido.** O protocolo data-control não tem "definir só se
estiver vazio". Se uma cópia nova chegar menos de ~150 ms depois de o
aplicativo de origem fechar, a devolução pode cobri-la (medido no
laboratório). Com 0,3 s de intervalo, isso não aconteceu.

## Provas

| Teste | Antes | Depois |
|---|---|---|
| wlmaker aninhado + XWayland, favorito PRIMEIRO, cópias e fechamento por wl-copy, imagem, 30 cópias com a origem fechando (34 verificações) | 32 falhas | 0 |
| X11 (Xvfb, xsel), recarregamento do histórico, origem fechando, 100 cópias seguidas | 11 falhas | 0 |
| `tests/run.sh`, seção `big-wm-clipboard` (ordem do `--dump`, `nil`, imagem, senha, item apagado, 100 cópias) | 4 falhas | passa |

Patch do wmcliphist: `packages/big-dockapps/wmcliphist-last-copy.patch`
(big-dockapps 13).
