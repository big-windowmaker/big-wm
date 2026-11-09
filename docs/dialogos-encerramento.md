# Diálogos de encerramento com o ícone do aplicativo

## Problema

Ao "Terminar" um aplicativo pelo menu do appicon (ou de um lançador do
Dock), a confirmação do wlmaker mostrava o logotipo do Dock (no Wayland, o
logotipo do wlmaker) em vez do ícone do aplicativo, e o painel usava o
degradê dos itens de menu — não o painel liso do Window Maker.

## Inventário das confirmações

| Onde | O quê | Antes | Agora |
|---|---|---|---|
| wlmaker `src/appicons.c` | menu do appicon > Terminar | logotipo | ícone do aplicativo (app_id do appicon → `.desktop`/`StartupWMClass` → tema de ícones, Flatpak; senão a imagem do appicon; senão o logotipo) |
| wlmaker `src/dock_menu.c` | menu do lançador / dockapp > Terminar | logotipo | ícone do aplicativo ou do dockapp; senão o logotipo |
| wlmaker `src/dock_menu.c` | Remover gaveta | logotipo | logotipo (não é de um aplicativo) |
| wlmaker `src/force_close.c` (novo) | Ctrl+Alt+Esc recusado | — | aviso de um botão, com o ícone do aplicativo |
| wlmaker `src/tl_menu.c` | menu da janela > Fechar | sem diálogo (pede para fechar) | igual |
| big-wm `big-wm-kill` (Wayland, lista GTK) | Forçar o fechamento | janela GTK | fica como alternativa pelo teclado; o Ctrl+Alt+Esc passa à caveira do wlmaker |
| X11: Window Maker | Matar/Terminar | painel do WINGs (referência visual) | igual |

## Solução (wlmaker `src/dialog.c`)

- `wlmaker_dialog_confirm_with_icon`: o diálogo recebe a imagem do ícone
  (cairo); `wlmaker_dialog_app_icon(server, app_id, instância, png)`
  resolve o ícone verdadeiro com o `src/app_icon.c` (o mesmo da lista de
  tarefas e dos appicons: entrada `.desktop` pelo app_id ou pela classe
  X11, `StartupWMClass`, tema de ícones do usuário e os que ele herda,
  hicolor, pixmaps, as exportações do Flatpak pelo `XDG_DATA_DIRS`), em
  64 px como os painéis do Window Maker, centrado no quadrado do ícone.
- Painel liso, como os do Window Maker, na cor clara do tema (a cor dos
  itens de menu, ou a primeira do degradê): cinza no tema claro, grafite no
  escuro, com o texto do tema. Título grande ao lado do ícone, linha
  gravada, mensagem centrada, botões à direita, o padrão com a seta de
  "Enter" do Window Maker.
- Um botão só (`other` = NULL): os avisos da caveira.
- Textos em inglês com tradução pt_BR no `src/i18n.c` ("Force Close" /
  "Forçar o fechamento" e os avisos).

## Testes

- Unitários (`dialog`): o desenho com dois botões e o aviso de um botão no
  tema escuro (painel liso nas duas pontas, ícone no canto superior
  esquerdo).
- Laboratório Wayland: `evidencias/wl-terminar-aplicacao-icone-calculadora.png`
  (menu do appicon da Calculadora > Terminar: o ícone da Calculadora, tema
  claro; "Não" mantém o app aberto) e
  `evidencias/wl-aviso-componente-tema-escuro.png` (tema escuro). O diálogo
  não ganha appicon próprio (sem ícone genérico na barra).

## Limitações

- O nome na mensagem é o do appicon/app_id (como no Window Maker, que usa a
  instância WM_CLASS), não o `Name=` traduzido do `.desktop`.

Capturas no repositório (sem o papel de parede do laboratório):
`docs/screenshots/ponteiro-caveira/`. As demais ficaram na máquina de
desenvolvimento, em `~/.cache/bwm-wt/cursor/evidencias/`.
