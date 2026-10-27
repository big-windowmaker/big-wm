# X11 × Wayland: diferenças e limitações

✅ funciona e foi testado · 🔧 funciona com diferença · ⏳ depende de mudança
fora do dock (proposta pronta) · ❓ pendente de hardware/sessão real.

| Recurso | X11 (Window Maker 0.96) | Wayland (wlmaker big-wm/protocols) |
|---|---|---|
| Cliente nativo | ✅ janela `_NET_WM_WINDOW_TYPE_DOCK`, sem foco, em todas as áreas de trabalho | ✅ superfície `zwlr_layer_shell_v1` (camada TOP), sem nenhuma API X11 |
| Posição e alinhamento | ✅ | ✅ (o wlmaker centraliza na **área útil**, descontando o Clip: o dock fica alguns pixels à direita do centro da tela — comportamento do compositor) |
| Reserva de espaço | ✅ `_NET_WM_STRUT` + `_PARTIAL` (o Window Maker reserva a borda inteira; ícones e janelas maximizadas respeitam) | ✅ com o wlmaker empacotado (pkgrel 7, que inclui a proposta 0044); o head do fork `f11a08a` sem o 0044 não aplica a zona ao maximizar (regressão do wlmaker) |
| Auto-ocultar | ✅ a janela fica no lugar e vira uma faixa de 4 px pela forma (o Window Maker ignora mover janelas dock) | ✅ a superfície encolhe para 4 px |
| Vários monitores | 🔧 escolha por nome RandR, índice ou principal; ❓ não testado com dois monitores (o Xvfb dá um só ao GTK 3) | ✅ nome do conector por xdg-output (o GTK 3 diz "Unknown"), índice, ou a saída na origem; saída removida → volta para outra |
| Mudança de resolução | ✅ (xrandr 1600×900 → 1280×720) | ✅ (wlr-randr) |
| Escala HiDPI | ✅ `GDK_SCALE=2` | 🔧 escala inteira do GTK 3; fracionária vira escala inteira reduzida pelo compositor |
| Aplicativos em execução | ✅ `_NET_CLIENT_LIST`, WM_CLASS, `_GTK_APPLICATION_ID`, pid | ✅ app_id pelo `ext-foreign-toplevel-list` |
| Janela ativa / minimizada | ✅ `_NET_ACTIVE_WINDOW`, `_NET_WM_STATE_HIDDEN`/WM_STATE | ✅ com o wlmaker empacotado (wlr-foreign-toplevel, proposta 0042); só com `ext-foreign-toplevel-list`, sem estado |
| Ativar janela existente | ✅ `_NET_ACTIVE_WINDOW` (fonte 2, troca de área de trabalho) | 🔧 aplicativo DBusActivatable: `org.freedesktop.Application.Activate` com token xdg-activation (testado); os demais: menu com as janelas e "Nova janela" (nunca abre duplicata por engano); ⏳ com o patch ✅ |
| Minimizar / restaurar / fechar | ✅ WM_CHANGE_STATE, `_NET_CLOSE_WINDOW` | ⏳ só com o patch proposto (testado com ele) |
| Alternar janelas do mesmo app | ✅ clique e rolagem | ⏳ com o patch ✅ |
| Títulos das janelas no menu | ✅ | ⏳ o wlmaker manda o app_id no lugar do título (bug, correção de 1 linha proposta) |
| Arrastar dentro do dock | ✅ | ✅ (eventos do próprio ponteiro) |
| Arrastar de/para o gerenciador de arquivos | ✅ XDND (testado: COPY, original intacto) | ❌ o wlmaker não implementa arrastar e soltar entre clientes (`start_drag` ausente); use o menu "Adicionar arquivos/pastas/aplicativo" |
| Menus de contexto | ✅ | 🔧 o wlmaker não encerra o popup ao clicar fora nem dá teclado ao popup de uma camada: o dock fecha o menu 1,5 s depois de o ponteiro sair ou com um clique no dock |
| Atalhos de teclado | ✅ captura global `Super+Alt+1…9`, `Super+Alt+PgUp/PgDn` | 🔧 o layer-shell do wlmaker recusa teclado ON_DEMAND: ligue `big-wm-tabbed-dock --tab …` a teclas no `Config.plist` (exemplo em [uso-e-configuracao.md](uso-e-configuracao.md)) |
| Token de lançamento | startup-notification (o Window Maker ignora) | ✅ `XDG_ACTIVATION_TOKEN` do GTK em todo lançamento |
| Dock clássico junto | ✅ Dock do Window Maker à direita | ✅ `wlmdock` à direita |

## O que faltava no wlmaker (propostas, adotadas no pacote wlmaker pkgrel 7)

Em [propostas/](propostas/), gerados contra uma cópia exportada do branch
`big-wm/protocols` em `f11a08a` (o repositório do fork não foi alterado):

1. `0042-toplevel_tracker-wlr-foreign-toplevel-management.patch` — anuncia
   `zwlr_foreign_toplevel_manager_v1` (wlroots já tem a implementação) ao lado
   do `ext-foreign-toplevel-list`, com ativar, minimizar, fechar e o estado
   ativo. Compilado, e o dock passou a
   ativar, minimizar, restaurar e alternar janelas no Wayland; 20/20 testes do wlmaker passam com os três patches (resultados em
   [testes.md](testes.md)).
2. `0043-xdg_toplevel-forward-the-title-…patch` — `set_title` repassava o
   `app_id` ao rastreador; docks e barras recebiam o app_id como título.
3. `0044-layer_panel-Recomputes-the-usable-area-…patch` — a área útil só era
   recalculada no configure da superfície layer-shell, antes do map, e um
   painel invisível não conta: a zona exclusiva de qualquer dock/barra
   layer-shell ficava de fora e janelas maximizadas passavam por baixo.

Fora do dock e ainda sem proposta: arrastar e soltar entre clientes
(`wlr_seat_request_start_drag`), fechamento de popups de camadas, teclado
ON_DEMAND no layer-shell.
