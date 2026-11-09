# Ícones de aplicativos (appicons), Dock e Clip — X11 e Wayland

Missão Big WindowMaker, novembro de 2026. Ramos `feat/appicons-dock-2026-11`
em wlmaker, wmaker e big-wm.

## Problema

1. **Wayland**: o `big-wm-appicons` mostrava um ícone por aplicativo aberto,
   mas sem o menu do Window Maker, sem arrastar o ícone para o Dock ou o
   Clip e sem a organização/persistência do X11. Os aplicativos do XWayland
   nem apareciam (o wlmaker não os publicava no foreign toplevel).
2. **X11 e Wayland**: "pego o ícone aberto, coloco no Clip/Dock e o duplo
   clique não abre". No X11 o Window Maker guardava como comando a linha de
   comando do processo (`WM_COMMAND`, tirada de `/proc/<pid>/cmdline`): no
   `WMState` havia `Command = "biglinux-tts main.py"`, que não roda (um
   script que mudou o próprio nome, com o arquivo relativo à pasta do
   processo). E com o programa aberto mas sem janela (Telegram ou Steam na
   bandeja), o duplo clique só "desescondia" — nada aparecia
   (`wmaker/src/dock.c`, `iconDblClick`). No wlmaker não havia como
   transformar o ícone de um programa aberto em lançador.

## Inventário dos menus do Window Maker (o que o Wayland segue)

Código: `wmaker/src/appicon.c` (`createApplicationMenu`, `appIconMouseDown`,
`iconDblClick`), `wmaker/src/dock.c` (`dockMenuCreate`), traduções
`po/pt_BR.po`.

| Onde | Window Maker (X11) | Wayland (wlmaker) |
|---|---|---|
| Appicon, botão direito | Mostrar Aqui, Esconder/Mostrar, Executar, Colocar ícone..., Terminar (com "Terminar Aplicação ... Confirme, por favor.") | os mesmos, na mesma ordem; desabilitados como no X11 (sem janela) |
| Appicon, duplo clique | mostra o aplicativo na área da janela; Shift: aqui; Ctrl: executa de novo; Alt (MOD): esconde os outros | o mesmo (Alt = `MoveWindowModifier`) |
| Appicon, botão do meio | Executar | o mesmo |
| Appicon, arrastar | solto no Dock/Clip/gaveta vira ícone acoplado (comando, ícone); fora, o appicon fica onde foi solto | solto no Dock/Clip/gaveta vira lançador (`State.plist`); fora, o appicon fica na fileira |
| Ícone acoplado | Executar, Mostrar Aqui, Esconder, Configurações... (comando, ícone, Iniciar com o Window Maker, Travado), Terminar; Clip: Remover Ícone, Opções do Clip | já existiam no wlmaker (D10, D16 de `wayland-parity.md`) |

Não foram criadas opções que o Window Maker não tem. "Manter ícone" do
Window Maker só existe para ícones atraídos pelo Clip (Autoattract), que o
wlmaker não tem; "Esconder outros" do Window Maker é o Alt+duplo clique,
não um item de menu. "Colocar ícone..." do X11 é o seletor de ícones do
Window Maker; no Wayland é o seletor de arquivos do `big-wm-dock-settings`
com prévia, gravando o mesmo `WMWindowAttributes` (`Icon`,
`AlwaysUserIcon = YES`).

## Decisão: onde implementar (Wayland)

Os appicons continuam desenhados pelo `big-wm-appicons` (cliente GTK,
layer shell): só ele resolve o que o compositor não sabe — ícone do tema
(SVG), `.desktop` do aplicativo (app_id/StartupWMClass/WM_CLASS → desktop
id), `WMWindowAttributes`. A **interação** passou para o **wlmaker**, que já
tem tudo do Dock do Window Maker: menus no estilo do Window Maker, diálogo
de Terminar, "Trazer aqui" entre áreas de trabalho (o protocolo foreign
toplevel não move janelas de área), o arraste de ladrilhos com fantasma,
deslize e encaixe (`tile_drag.c`), a gravação do `State.plist` e o aviso ao
`wlmdock`. Mesma solução usada para o Dock (`wlmdock` desenha, o compositor
trata o ponteiro).

O cliente publica a fileira em `$XDG_RUNTIME_DIR/big-wm-appicons.plist`
(nome, app IDs das janelas, `CommandLine = "gtk-launch <id>.desktop"`, ícone
desenhado como PNG 56×56 em `~/.local/share/wlmaker/icons`, onde o Dock
procura os ícones dos lançadores). O wlmaker reconhece a superfície
`big-wm-appicons` (como reconhece `wlmdock`) e:

- botão direito → menu do appicon; botão do meio → Executar;
- duplo clique com Shift/Ctrl/Alt como o `iconDblClick`;
- arrastar (limiar de 8 px) → o ladrilho segue o ponteiro, o fantasma mostra
  onde vai ficar (Dock, Clip ou gaveta aberta, com as regras do
  `wDockSnapIcon`), soltar desliza para o lugar e insere o lançador; **Esc
  cancela** (vale para qualquer arraste de ladrilho); solto fora não muda
  nada;
- balão com o nome, como nos ícones do Dock.

Alternativas descartadas: arrastar e soltar do `wl_data_device` (o Esc não
chega ao cliente durante o arraste, e o fantasma/encaixe teriam de ser
refeitos); protocolo próprio (mais código nos dois lados para o mesmo
resultado); mover os appicons inteiros para o compositor (ele não tem tema
de ícones nem índice de `.desktop`).

## Lançador confiável

- Wayland: o comando é o `gtk-launch` do `.desktop` (o arquivo inteiro, com
  `.desktop`: `gtk-launch org.telegram.desktop.desktop` funciona; sem o
  sufixo, não). Sem `.desktop`, o wlmaker monta o comando a partir do
  processo da janela (`/proc/<pid>/cmdline`, `cwd`, `exe`): programa fora do
  PATH vira o executável do processo, programa ou arquivo relativo vira
  absoluto, argumentos com espaço/aspas vão entre aspas
  (`appicons.c: wlmaker_appicons_command_from_process`, com teste).
- X11: o Window Maker do Big WindowMaker faz o mesmo ao acoplar o ícone de
  um aplicativo aberto (`wmaker/src/appcommand.c`): `.desktop` por
  StartupWMClass, desktop id, última parte do id reverso ou programa do
  `Exec` (também pelo `_NET_WM_PID` de uma janela do aplicativo, já que o
  líder do GTK não tem); senão a linha de comando tratada, com as aspas que
  o `ExpandOptions` e o `wtokensplit` esperam. Dockapps ficam como estavam.
- X11, duplo clique num ícone acoplado de aplicativo aberto **sem nenhuma
  janela** (mapeada, minimizada, enrolada ou escondida): roda o comando, o
  que traz a janela nos programas de instância única. O estado do ícone não
  muda.
- X11, entradas antigas: `big-wm-dock-commands` (do `startbigwm`, antes do
  wmaker, uma vez por conta, marca em
  `~/.config/big-wm/migrations/dock-commands`) troca no `WMState` o comando
  que não roda (programa inexistente ou script relativo) pelo `gtk-launch`
  do `.desktop` correspondente; o `PasteCommand`/`DropCommand` derivados
  também, salvo se o usuário os mudou. Sem `.desktop` correspondente, fica
  como está. Cópia em `WMState.bak-dock-commands`.

## Arquivos

| Repositório | Arquivos |
|---|---|
| wlmaker | `src/appicons.[ch]` (novo: fileira publicada, menu, duplo clique, lançador, comando do processo), `src/layer_panel.c` (superfície `big-wm-appicons`), `src/tile_drag.[ch]` (lançador novo, Esc), `src/input/manager.[ch]` (`wlmim_has_key_grab`), `src/toplevel_tracker.[ch]` (todas as janelas; estado minimizado espelhado), `src/xwl_surface.c` (janelas XWayland no foreign toplevel, mudança pequena), `src/context_menu.c` (menu dentro da tela depois de ter os itens), `src/i18n.c` |
| big-wm | `tabbed-dock/src/ui/btd-appicons.c`, `session/bin/big-wm-dock-settings` (`appicon <AppID>`), `session/lib/big-wm-dock-commands`, `session/bin/startbigwm`, `Makefile`, `tests/run.sh` |
| wmaker | `src/appcommand.[ch]` (novo), `src/dock.c`, `src/Makefile.am`, `test/appcommand_test.c`, `test/Makefile.am` |

## Testes feitos

Unitários:

- wlmaker: `ctest` (29/29), com os novos `appicons.command`,
  `appicons.load` e `toplevel_tracker.app_windows` (todas as janelas).
- wmaker: `make -C test check` → `appcommand_test` (StartupWMClass, id com
  `.desktop`, id = classe, última parte do id, programa do Exec com e sem
  executável, subpasta `kde4-`, `Hidden`, `Type=Link`, script renomeado,
  aspas para ExpandOptions/wtokensplit).
- big-wm: `tests/run.sh` inteiro passou; novo bloco 6a1a
  (`big-wm-dock-commands`: TTS e Telegram trocados, `DropCommand` do usuário
  mantido, sem `.desktop`/comando que roda/dockapp intocados, uma vez só).

Laboratório (Xvfb, wlmaker aninhado deste ramo, `wlmdock` e
`big-wm-appicons` deste ramo; X11 com o wmaker deste ramo; safebin):

- Wayland: GTK 3 (gedit, duas janelas → um appicon), GTK 4 (Calculadora),
  Qt 6 (KWrite, Qt Designer), Chrome e VSCodium (Electron) pelo XWayland,
  xterm pelo XWayland, Firefox (Wayland; tem lançador no Clip, então fica lá
  e não na fileira, como no X11), aplicativo sem `.desktop` (script Python).
  Todos com `CommandLine` do `.desktop` (`gtk-launch google-chrome.desktop`,
  `gtk-launch vscodium-insiders-bin.desktop`...), menos o sem `.desktop`,
  acoplado como `python3 /…/apps/nodesk/./sem-desktop.py arg1` (relançado
  pelo duplo clique depois de Terminar).

  ![Fileira de appicons depois de Alt+duplo clique no gedit (os outros escondidos, esmaecidos)](screenshots/appicons-dock-clip/wayland-linha.png)
  ![Menu do appicon](screenshots/appicons-dock-clip/wayland-menu.png)
  ![Arrastando a Calculadora para o Clip / depois de soltar](screenshots/appicons-dock-clip/wayland-arrastar-clip.png)
  ![xterm (XWayland) para o Dock](screenshots/appicons-dock-clip/wayland-arrastar-dock.png)
  ![Fantasma no Dock durante o arraste; Esc cancela (State.plist inalterado)](screenshots/appicons-dock-clip/wayland-fantasma-esc.png)
  ![Menu do lançador acoplado no Clip](screenshots/appicons-dock-clip/wayland-menu-clip.png)
  ![Terminar Aplicação](screenshots/appicons-dock-clip/wayland-terminar.png)
  ![Depois de reiniciar o wlmaker: Calculadora e o sem .desktop no Clip, xterm no Dock](screenshots/appicons-dock-clip/wayland-reinicio.png)
  ![Colocar ícone...: antes e depois (WMWindowAttributes)](screenshots/appicons-dock-clip/wayland-colocar-icone.png)

- X11 (wmaker deste ramo): aplicativo de instância única que fecha para a
  bandeja (`org.lab.Bandeja`, `.desktop` com StartupWMClass) e a
  Calculadora (GTK 4) acoplados no Clip → `WMState` com
  `Command = "gtk-launch org.lab.Bandeja.desktop"` e
  `"gtk-launch org.gnome.Calculator.desktop"` (antes da correção do líder
  GTK, a Calculadora ficava `"gnome-calculator"`). Janela fechada para a
  bandeja (`IsUnMapped`), duplo clique no ícone do Clip → `IsViewable`.
  **Controle**: o mesmo com o `/usr/bin/wmaker` instalado → continua
  `IsUnMapped`. Com o wmaker instalado, um script renomeado
  (`exec -a renamed-tts python3 sem-desktop.py`) era acoplado como
  `Command = "sem-desktop.py"` (o caso relatado).

  ![Acoplando no Clip do X11](screenshots/appicons-dock-clip/x11-acoplar-clip.png)
  ![Bandeja: janela fechada / de volta pelo duplo clique](screenshots/appicons-dock-clip/x11-bandeja.png)

## Limitações e pendências

- Flatpak: há um instalado (AnyDesk), não aberto no laboratório (serviço de
  rede); o `.desktop` exportado em `/var/lib/flatpak/exports/share` entra na
  busca nos dois lados, mas não foi conferido com uma janela de verdade.
- Os appicons do Wayland ficam numa fileira (não se soltam em qualquer
  lugar da tela como no X11) e não há miniaturas de janelas minimizadas.
- O balão do lançador acoplado mostra o AppID (como os outros lançadores do
  wlmaker), não o nome do `.desktop`.
- No laboratório X11 um `xdotool mousedown` que cai na raiz começa a seleção
  por retângulo do Window Maker com o servidor agarrado e o próprio xdotool
  fica preso (artefato do teste sintético, não do código).
- Ícone PNG desenhado do tema na hora de publicar: trocar de tema não
  redesenha os lançadores já acoplados (como o cache de ícones do Window
  Maker).
