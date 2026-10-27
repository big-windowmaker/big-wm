# Paridade X11 ↔ Wayland

Matriz viva da sessão **Big WindowMaker — Wayland** (wlmaker) comparada com a
sessão **X11** (Window Maker), que é a referência. Cada linha diz o que foi
testado e onde. As evidências vêm da VM de testes (QEMU/KVM, virtio-gpu sem
3D, BIOS, sistema instalado a partir da ISO de 2026-10-26 e atualizado com os
pacotes desta versão), do teste de integração do CI (`tests/integration/`,
contêiner Manjaro com wlmaker headless) e do pipeline da ISO
(`big-windowmaker/build-iso`, run 40385383947: smoke test e instalação em
BIOS e UEFI).

**Status:** ✅ funciona e foi testado · 🔧 funciona com diferença ou sem teste
automático · ⏳ em correção (link para o item) · ❌ limitação conhecida, com
alternativa · ❓ pendente de hardware real.
**Prioridade:** P0 bloqueia uso/segurança · P1 quebra fluxo essencial ·
P2 consistência · P3 polimento.

Como reproduzir os testes: [wayland-test-lab.md](wayland-test-lab.md).

## Sessão e login

| ID | Recurso | X11 | Wayland | Solução / arquivo | Teste | Status |
|---|---|---|---|---|---|---|
| L1 | Entrar pelo greeter (slick-greeter) | ✅ | ✅ | `startbigwm-wayland` | VM instalada (captura) | ✅ |
| L2 | Autologin na sessão Wayland | ✅ | sem o grupo `autologin` o PAM recusa e o LightDM entra em laço tentando um greeter X11 num servidor "wayland" | o live usa a sessão `livecd` (X11); autologin Wayland exige o grupo `autologin` (padrão do Arch) | VM: `lightdm.log` | ❌ P2 (LightDM) |
| L3 | Saber qual sessão está escolhida no greeter | igual | igual | emblema próprio `big-wm-wayland.svg` com selo "W" | — | ✅ |
| L4 | Ambiente no `systemd --user` (`WAYLAND_DISPLAY`, `XDG_SESSION_ID`, `XDG_SESSION_TYPE`) | ✅ | ✅ | `big-wm-session-ctl` | VM: `systemctl --user show-environment` | ✅ |
| L5 | Novo login depois de uma sessão encerrada sem "stop" (X caiu, LightDM reiniciado) | autostart não subia | idem | `big-wm-session-ctl start` limpa o ambiente velho e reinicia o target | VM: 4 reinícios seguidos | ✅ |
| L6 | Atualização de quem já usava a versão anterior | menu e canto ativo migrados | menu e `Config.plist` migrados | `big-wm-firstrun` (troca só linhas antigas; menu idêntico ao modelo antigo vai para `.bak-*`) | `tests/run.sh`; VM (log `big-wm-firstrun`) | ✅ |
| L7 | Não afetar Plasma/XFCE/GNOME | ✅ | ✅ | autostarts com `OnlyShowIn`; drop-ins liberam tudo fora das sessões Big WindowMaker | `tests/run.sh` | ✅ |
| L8 | Tela de login wdm (opcional; na ISO fica instalada e desativada) | ✅ | ✅ abre `startbigwm-wayland` | big-wm-packages/wdm | VM: `~/.xsession-errors` ("wdm: sessão Wayland iniciada") | ✅ |
| L9 | Sair da sessão sem deixar serviços morrendo com o X | ✅ | ✅ | `big-wm-session-ctl stop` para o `graphical-session.target` e espera (antes o autostart parava depois, com o X já fechado: unidades falhas e picom abortando) | contêiner com `systemd --user`: 0 unidades falhas ao fechar o X (eram 5) | ✅ |
| L10 | Portais com a configuração da sessão mesmo acordados antes do login | ✅ | ✅ (`wlmaker-portals.conf`, backend wlr) | `big-wm-session-ctl start` reinicia os portais que um serviço do login (biglinux-snapshot-detect) acordou sem `DISPLAY`/`XDG_CURRENT_DESKTOP` | contêiner com `systemd --user`: portal reiniciado com `XDG_CURRENT_DESKTOP` | ✅ |

## Menus e aplicativos

| ID | Recurso | X11 | Wayland | Solução / arquivo | Teste | Status |
|---|---|---|---|---|---|---|
| M1 | Menu raiz com a mesma estrutura | ✅ | ✅ (mesmos itens e submenus) | `data/wlmaker/RootMenu.plist*` | VM: F12 + navegação por teclado (captura) | ✅ |
| M2 | Aplicativos por categoria, traduzidos | ✅ (refeito a cada abertura) | ✅ (gerado no início da sessão; o wlmaker carrega o menu uma vez) | `big-wm-appmenu --wlmaker` (`GeneratePlistMenu`) | `tests/run.sh`; CI | 🔧 P3: app instalado durante a sessão só aparece no próximo login |
| M3 | Clique direito rápido na área de trabalho | abre o menu | abre o menu (clique com menos de 3 px de movimento não aciona item; arrastar até o item aciona) | wlmaker patch 0015 | teste unitário do wlmaker | ✅ |
| M4 | Buscar aplicativos (Super+Espaço) | ✅ rofi com o tema | ✅ rofi com o tema (nativo Wayland) | `big-wm-open apps` | VM | ✅ |
| M5 | Executar... | ✅ | ✅ | `big-wm-open run` | — | 🔧 |
| M6 | Papéis de parede | ✅ `wmsetbg` | ✅ `swaybg`, escolha guardada | `big-wm-wallpaper`, `big-wm-appmenu --wallpapers` | `tests/run.sh` | 🔧 |
| M7 | Temas e estilos (Aparência) | ✅ | ✅ Aparência > Temas e Estilos aplicam o estilo do Window Maker (`setstyle`), convertido para o `Theme.plist` na hora | `big-wm-wprefs-sync --open-menu`; wlmaker patch 0079 (recarregar) | `tests/wprefs`; CI (captura `wayland-wprefs-after.png`) | 🔧 texturas com imagem viram cor sólida |
| M7b | Tema de ícones do GTK | ✅ bigicons-papient (escuro: bigicons-papient-dark) | ✅ o mesmo | `big-wm-theme` (a cada login, pelo `big-wm-firstrun`, e ao trocar o modo) grava o tema no gsettings, que o GTK lê no Wayland, e no `settings.ini` do GTK 3 e 4, que o GTK lê no X11 sem XSETTINGS; antes a conta instalada ficava com o Adwaita nas duas. Tema de ícones escolhido pelo usuário fica | contêiner: GTK no Xvfb e no wlmaker headless leem o mesmo tema antes/depois (Adwaita → bigicons-papient → -dark → claro); `tests/run.sh` | ✅ |
| M8 | Preferências (WPrefs) | ✅ | ✅ o próprio WPrefs (XWayland, Configurações > Window Maker); o que ele grava chega ao wlmaker em menos de 1 s | `big-wm-wprefs-sync` + wlmaker patches 0075–0079; ver [Preferências do Window Maker no Wayland](#preferências-do-window-maker-wprefs-no-wayland) | `tests/wprefs` (34 testes, com arquivos gravados pelo WPrefs); CI | 🔧 o que o wlmaker não tem fica de fora (tabela) |
| M9 | Aplicativos X11 (XWayland) | nativo | ✅ | `--start_xwayland` | CI (`xeyes`) | ✅ |

## Preferências do Window Maker (WPrefs) no Wayland

O WPrefs também abre na sessão Wayland (XWayland), pelo menu Configurações >
Window Maker, e tudo o que ele grava em `~/GNUstep/Defaults` (e o que o
`setstyle`, o editor de menu e os papéis de parede gravam) passa para o
wlmaker pelo **`big-wm-wprefs-sync`**:

- **No login**, o `startbigwm-wayland` roda `big-wm-wprefs-sync --no-reload`
  antes de o wlmaker ler a configuração (o `big-wm-firstrun wayland` já
  copiou os padrões do Big WindowMaker para `~/GNUstep/Defaults`, para o
  WPrefs começar deles).
- **Durante a sessão**, o autostart `big-wm-wayland-wprefs-sync` roda
  `big-wm-wprefs-sync --watch`: vigia `~/GNUstep/Defaults` e
  `~/GNUstep/Library/WindowMaker/autostart` (inotify, sem dependência nova),
  junta as gravações em sequência do WPrefs e sincroniza.
- **Recarregar sem sair**: depois de gravar, o sync manda `SIGHUP` ao wlmaker
  (só quando o binário tem o patch 0079 — um wlmaker sem ele terminaria com o
  sinal; nesse caso vale no próximo login). O wlmaker relê o `Config.plist`
  e aplica atalhos, teclado, mouse, foco, posicionamento, cantos ativos, o
  tema e o menu raiz (o menu só se estiver fechado). O mesmo pela ação
  `ReloadConfiguration` ou pelo menu Sessão > Recarregar a configuração.
  Valem só no próximo login: nomes e número das áreas de trabalho
  (`State.plist`) e a tecla de mover janelas para janelas já abertas.
- **Modo claro/escuro** (`big-wm-theme`): o estilo padrão de referência é o
  do modo (`BigWindowMaker.style` ou `BigWindowMaker-Dark.style`), então a
  troca de modo em si não é "mudança do WPrefs": o wlmaker fica com o tema
  curado do modo (`Theme.plist` ou `Theme-Dark.plist`, que o `big-wm-theme`
  escolhe no `ThemeFile`). O `big-wm-theme` chama o `big-wm-wprefs-sync`
  depois de trocar, e o wlmaker recarrega: a troca vale na hora, sem novo
  login.

**Mesclagem.** Cada valor convertido é um item (arquivo + caminho no plist).
O estado da última sincronização fica em `~/.config/wlmaker/.wprefs-sync.json`.
Um item só é gravado quando o valor que vem do Window Maker **mudou** desde a
última vez; o resto dos arquivos do wlmaker fica como está, com os
comentários (a edição é feita no texto, só no trecho do valor). Assim uma
edição à mão no `Config.plist`/`Theme.plist` sobrevive enquanto o WPrefs não
mexer no mesmo item; se mexer, o WPrefs vence (fica registrado no log).
Na primeira vez a referência é a configuração padrão do Big WindowMaker: só
o que o usuário mudou na sessão X11 é levado — exceto os atalhos de teclado,
que vão todos (o Wayland ganha Alt+M, Alt+1…Alt+0 etc., que só existiam no
X11). Um item que volta ao padrão recebe o valor do arquivo padrão do
wlmaker (`/usr/share/big-wm/wlmaker`), não a conversão: o tema curado do Big
WindowMaker volta exatamente como era. O menu raiz é um item só: se o menu
do Window Maker mudar, o `RootMenu.plist` é gerado de novo e uma versão
editada à mão vai antes para `RootMenu.plist.bak-<data>`. O tema também é
tratado inteiro: quando a aparência do Window Maker difere do estilo padrão
do modo, `~/.config/wlmaker/Theme.plist` é gerado do tema curado do modo com
as diferenças por cima (marcado "Gerado pelo big-wm-wprefs-sync" no início)
e o `ThemeFile` aponta para ele; sem diferenças, o arquivo sai e o
`ThemeFile` volta ao tema do modo. Ele é refeito só quando a aparência ou o
modo mudam (uma edição à mão fica até lá e então vai para
`Theme.plist.bak-<data>`). Um `Theme.plist` sem a marca (feito pelo usuário)
ou um `ThemeFile` para outro arquivo não são tocados, e a aparência do
Window Maker não é levada. Ações que só existem no wlmaker do Big WindowMaker (patches) só são
usadas se aparecem no binário instalado: uma ação desconhecida impediria o
wlmaker de iniciar. Sem `~/.config/wlmaker/Config.plist` (sessão Wayland
nunca usada) o sync não faz nada; o primeiro login Wayland faz tudo.

Comandos: `big-wm-wprefs-sync` (sincroniza e recarrega), `--dry-run` (mostra
o que mudaria), `--full` (aplica todos os valores convertidos), `--reload`,
`--watch`, `--open-menu`/`--open-plmenu` (submenus gerados). Testes:
`python3 -I -m unittest discover -s tests/wprefs` (também no `tests/run.sh`),
com arquivos gravados pelo próprio WPrefs 0.96 em
`tests/wprefs/fixtures/wprefs-real`; no CI, `tests/integration/wayland.sh`
muda o estilo com o `wdwrite`, sincroniza e confere na captura que a barra de
título mudou sem reiniciar o wlmaker.

Patches do wlmaker (big-windowmaker/packages, `wlmaker/0075`–`0079`, branch
`feature/wprefs-reload` do fork): 0075 ações `WorkspaceSwitchTo`,
`WindowToWorkspace` e `WindowToggleShaded`; 0076 cantos ativos com comando
(e a correção dos cantos de baixo, que estavam trocados); 0077 `Justify` do
título; 0078 seção `Mouse` (aceleração, duplo clique) e `FocusMode = Sloppy`;
0079 recarregar com `SIGHUP`/`ReloadConfiguration`.

**Legenda:** ✅ já funciona igual no Wayland (nativo, sem conversão) ·
🔄 convertido pelo `big-wm-wprefs-sync` · 🩹 convertido e precisou de patch no
wlmaker (feito) · ❌ não dá (motivo na linha).

### Foco das janelas

| Chave | Wayland | Status |
|---|---|---|
| `FocusMode` (manual / auto, sloppy) | `FocusMode = Click / Sloppy` (a janela sob o ponteiro é ativada, sem subir) | 🩹 0078 |
| `RaiseDelay` (subir sozinha) | o wlmaker não tem auto-raise | ❌ |
| `IgnoreFocusClick` | o wlmaker sempre entrega ao aplicativo o clique que ativa a janela | ❌ |
| `AutoFocus` (focar janelas novas) | sempre ligado no wlmaker | ✅ |
| `CirculateRaise` | Alt+Tab do wlmaker sempre sobe a janela | ✅ |
| `ColormapMode` | não existe colormap no Wayland | ❌ |

### Manipulação de janelas

| Chave | Wayland | Status |
|---|---|---|
| `WindowPlacement` | `WindowPlacement`: center → Center; auto, smart, cascade, random, manual → Smart | 🔄 |
| `WindowPlaceOrigin` | sem equivalente | ❌ |
| `EdgeResistance`, `Attraction` | o wlmaker não tem resistência nem atração nas bordas | ❌ |
| `OpaqueMove`, `OpaqueResize`, `OpaqueMoveResizeKeyboard` | o Wayland sempre move e redimensiona com o conteúdo | ✅ |
| `NoWindowOverDock`, `NoWindowOverIcons` | maximizar e posicionar usam a área útil, sem cobrir Dock e Clip (patches 0031, 0044) | ✅ |
| `DragMaximizedWindow`, `ResizeIncrement` (Mod+roda) | sem equivalente | ❌ |

### Menus

| Chave | Wayland | Status |
|---|---|---|
| `ScrollableMenus` | menus maiores que a tela deslizam (patch 0063) | ✅ |
| `WrapMenus` | submenus abrem do outro lado quando não cabem (patch 0062) | ✅ |
| `MenuScrollSpeed`, `AlignSubmenus` | velocidade e alinhamento fixos | ❌ |
| `ViKeyMenus` (h j k l) | os menus do wlmaker usam as setas e Esc | ❌ |

### Ícones, miniaturas e ergonomia

| Chave | Wayland | Status |
|---|---|---|
| `IconPosition`, `IconificationStyle`, `AutoArrangeIcons`, `MiniPreviewSize`, `MiniwindowPreviewBalloons`, `StickyIcons`, `IconSize`, `EnforceIconMargin` | o wlmaker não tem mini-ícones nem área de ícones (D8) | ❌ |
| `SingleClickLaunch` | o wlmdock lê `SingleClickLaunch` do `Config.plist` (padrão NO, clique duplo, como no Window Maker: D17); o sync ainda não copia a chave do WPrefs | 🔧 |
| `MoveDisplay`, `ResizeDisplay` | o wlmaker não mostra posição/tamanho ao mover | ❌ |
| `WorkspaceBorder`, `WorkspaceBorderSize` | sem equivalente | ❌ |
| `AppIconBalloons` | balões dos ícones do Dock, do Clip e dos dockapps sempre ligados, como no Big WindowMaker X11 (D13); a chave não é lida | 🔧 |
| `HelpBalloons`, `WindowTitleBalloons`, `MiniwindowTitleBalloons` | sem esses balões no wlmaker | ❌ |
| `BounceAppIconsWhenUrgent`, `RaiseAppIconsWhenBouncing`, `DoNotMakeAppIconsBounce` | sem ícones de aplicativo que pulam | ❌ |
| `PixmapPath` | usado para achar a imagem do `WorkspaceBack` | 🔄 |
| `IconPath` | o wlmdock procura ícones nos diretórios dele | ❌ |

### Dock, Clip e áreas de trabalho

| Chave | Wayland | Status |
|---|---|---|
| `DisableDock` | esconde o autostart do Dock (wlmdock) e o para/inicia na hora; um override do Big Tabbed Dock tem prioridade | 🔄 |
| `DisableClip` | some só a coluna de lançadores do Clip (`wlmdock --clip`); o Clip faz parte do wlmaker (os dockapps se prendem nele) | 🔄 parcial |
| `DisableDrawers` | o wlmaker não tem gavetas | ✅ |
| `ClipAutoraiseDelay`, `ClipAutolowerDelay`, `ClipAutoexpandDelay`, `ClipAutocollapseDelay` | o Clip do wlmaker não sobe, desce nem recolhe | ❌ |
| `CycleWorkspaces` | o wlmaker sempre dá a volta da última para a primeira | ✅ |
| `AdvanceToNewWorkspace`, `DontLinkWorkspaces` | arrastar a janela para a área seguinte não existe | ❌ |
| `WorkspaceNameDisplayPosition` | o nome aparece só no Clip | ❌ |
| Nomes das áreas (`WMState` > `Workspaces`) | `State.plist` > `Workspaces` | 🔄 próximo login |

### Outros (Configurações), expert e caminhos

| Chave | Wayland | Status |
|---|---|---|
| `DisableAnimations`, `IconSlideSpeed`, `ShadeSpeed`, `SmoothWorkspaceBack`, `Superfluous` | o wlmaker não tem animações | ❌ |
| `ColormapSize`, `DisableDithering` | o Wayland é sempre em cores reais | ❌ |
| `NewStyle` (barra de título new/old/next) | um estilo de barra só | ❌ |
| `AntialiasedText` | o wlmaker sempre suaviza | ✅ |
| `HotplugMonitor` | o wlroots trata a conexão de monitores | ✅ |
| `DisableMiniwindows` | o wlmaker não tem mini-ícones | ✅ |
| `NoXSetStuff` | só muda o WPrefs | ✅ |
| `IgnoreGtkHints`, `EnableWorkspacePager`, `SaveSessionOnExit`, `UseSaveUnders`, `DontConfirmKill`, `DisableBlinking`, `CycleActiveHeadOnly`, `CycleAllWorkspaces`, `CycleIgnoreMinimized`, `WindowListAppIcons`, `SwitchPanelImages`, `ShowClipTitle`, `HighlightActiveApp`, `KbdModeLock`, `WindowSnapping`, `SnapEdgeDetect`, `SnapCornerDetect`, `SnapToTopMaximizesFullscreen`, `MoveHalfMaximizedWindowsBetweenScreens`, `AlternativeHalfMaximized`, `PointerWithHalfMaxWindows`, `OpenTransientOnOwnerWorkspace`, `WrapAppiconsInDock`, `DbClickFullScreen`, `CloseRootMenuByLeftOrRightMouseClick`, `KeepDockOnPrimaryHead`, `MouseWheelFocus` | recursos que o wlmaker não tem (sessão X11, painel de troca, encaixe nas bordas, meia tela, pager); o duplo clique no título enrola a janela (patch 0019) | ❌ |

### Atalhos de teclado

Nome das teclas: `Mod1`/`Alt`/`Meta` → `Alt`, `Control` → `Ctrl`, `Mod4`/`Super`/`Hyper` → `Logo`,
`Shift`, `Mod2`, `Mod3`, `Mod5`; `Shift+Tab` vira `Shift+…+ISO_Left_Tab` (é o que o wlmaker recebe);
tecla com `Lock` ou nome que o xkbcommon não conhece é ignorada (o wlmaker não iniciaria).

| Ação do Window Maker | Ação do wlmaker | Status |
|---|---|---|
| `RootMenuKey` | `RootMenu` | 🔄 |
| `CloseKey` | `WindowClose` | 🔄 |
| `MaximizeKey` | `WindowToggleMaximized` | 🔄 |
| `MiniaturizeKey` | `WindowMinimize` | 🔄 |
| `RaiseKey`, `LowerKey` | `WindowRaise`, `WindowLower` (o wlmaker ainda não os implementa: só registra no log) | 🔄 |
| `FocusNextKey`, `FocusPrevKey` | `TaskNext`, `TaskPrevious` | 🔄 |
| `NextWorkspaceKey`, `PrevWorkspaceKey` | `WorkspaceNext`, `WorkspacePrevious` | 🔄 |
| `MoveToNextWorkspaceKey`, `MoveToPrevWorkspaceKey` | `WindowToNextWorkspace`, `WindowToPreviousWorkspace` | 🔄 |
| `ExitKey` | `Quit` | 🔄 |
| `RunKey` | `ShellExecute big-wm-open run` | 🔄 |
| `ScreenCaptureKey`, `WindowCaptureKey`, `PartialCaptureKey` | `ShellExecute big-wm-screenshot full / window / area` | 🔄 |
| `SHORTCUT` dos itens do menu raiz | atalho com a ação do item | 🔄 |
| `Workspace1Key` … `Workspace10Key` | `(WorkspaceSwitchTo, "N")` | 🩹 0075 |
| `MoveToWorkspace1Key` … `MoveToWorkspace10Key` | `(WindowToWorkspace, "N")` | 🩹 0075 |
| `ShadeKey` | `WindowToggleShaded` | 🩹 0075 |
| `WindowListKey`, `WindowMenuKey`, `HideKey`, `HideOthersKey`, `MinimizeAllKey`, `VMaximizeKey`, `HMaximizeKey`, `LHMaximizeKey`, `RHMaximizeKey`, `THMaximizeKey`, `BHMaximizeKey`, `TLCMaximizeKey`, `TRCMaximizeKey`, `BLCMaximizeKey`, `BRCMaximizeKey`, `MaximusKey`, `CenterKey`, `KeepOnTopKey`, `KeepAtBottomKey`, `OmnipresentKey`, `RaiseLowerKey`, `MoveResizeKey`, `SelectKey`, `FocusWindowLeftKey`, `FocusWindowRightKey`, `FocusWindowUpKey`, `FocusWindowDownKey`, `GroupNextKey`, `GroupPrevKey`, `MarkSetKey`, `MarkUnsetKey`, `MarkBringKey`, `MarkJumpKey`, `MarkSwapKey`, `WorkspaceMapKey`, `LastWorkspaceKey`, `NextWorkspaceLayerKey`, `PrevWorkspaceLayerKey`, `MoveToLastWorkspaceKey`, `MoveToNextWorkspaceLayerKey`, `MoveToPrevWorkspaceLayerKey`, `WindowShortcut1Key` … `WindowShortcut10Key`, `MoveTo12to6Head`, `MoveTo6to12Head`, `WindowRelaunchKey`, `ScreenSwitchKey`, `DockRaiseLowerKey`, `ClipRaiseLowerKey`, `ToggleKbdModeKey`, `KeychainTimeoutDelay` | o wlmaker não tem a ação (lista de janelas, menu da janela pelo teclado, ocultar aplicativo, meia tela/cantos, manter acima, marcas, pager, troca de idioma do teclado por janela); o atalho não é criado | ❌ |

### Cantos ativos

| Chave | Wayland | Status |
|---|---|---|
| `HotCorners`, `HotCornerDelay`, `HotCornerActions` | `HotCorner`: `TriggerDelay` e `TopLeftEnter`… com `(ShellExecute, "comando")`; `big-wm-lock` vira `LockScreen`. O grupo vai inteiro (canto vazio = sem ação) | 🩹 0076 |
| `HotCornerEdge` | o canto do wlmaker é o último pixel | ❌ |

### Mouse e teclado

| Chave | Wayland | Status |
|---|---|---|
| Velocidade (`xset m N/10 T` no `autostart`) | `Mouse = { AccelSpeed; AccelProfile; }` do libinput (escala logarítmica: 2 → 0, 10 → 1; `1/1` = Flat) | 🩹 0078 |
| Limiar (`T` do `xset m`) | o libinput não tem limiar | ❌ |
| `DoubleClickTime` | `Mouse.DoubleClickTime` (duplo clique no título); os aplicativos WINGs leem o próprio `WMGLOBAL` | 🩹 0078 |
| `ModifierKey` (arrastar janelas) | `Cursor.MoveWindowModifier` (janelas abertas depois da mudança) | 🔄 |
| `MouseLeftButtonAction`, `MouseMiddleButtonAction`, `MouseRightButtonAction`, `MouseBackwardButtonAction`, `MouseForwardButtonAction`, `MouseWheelAction`, `MouseWheelTiltAction`, `DisableWSMouseActions` | cliques na área de trabalho são fixos no wlmaker (direito abre o menu raiz) | ❌ |
| Repetição do teclado (`xset r rate D R` no `autostart`) | `Keyboard.Repeat` (`Delay`, `Rate`). O painel de teclado do WPrefs 0.96 não é compilado ("not_yet_fully_implemented") | 🔄 |
| Layout do teclado | as duas sessões leem o do sistema (`localectl`) | ✅ |

### Aparência (estilo) e fontes

Texturas: `solid` → `SOLID`; `hgradient`/`vgradient`/`dgradient` → `HGRADIENT`/`VGRADIENT`/`DGRADIENT`;
`mhgradient`/`mvgradient`/`mdgradient` → degradê da primeira à última cor; `igradient` → cor média;
`thgradient`/`tvgradient`/`tdgradient` → o degradê, sem a imagem; `tpixmap`/`spixmap`/`cpixmap`/`mpixmap`/`fpixmap`
→ a cor de fundo da textura (o wlmaker não desenha imagens nas barras). Cores: `#rgb`, `#rrggbb`,
`#rrrrggggbbbb`, `rgb:r/g/b` e nomes do X11 (`rgb.txt`). Fontes (fontconfig ou XLFD): primeira família,
negrito a partir de `weight=180`/`bold`, `pixelsize` ou `size` (pontos × 96/72).

| Chave | Wayland (`Theme.plist`) | Status |
|---|---|---|
| `FTitleBack`, `UTitleBack` | `Window.TitleBar.FocussedFill`, `BlurredFill` | 🔄 |
| `FTitleColor`, `UTitleColor` | `Window.TitleBar.FocussedTextColor`, `BlurredTextColor` | 🔄 |
| `TitleJustify` | `Window.TitleBar.Justify` (Left/Center/Right) | 🩹 0077 |
| `ResizebarBack` | `Window.ResizeBar.Fill` | 🔄 |
| `FrameBorderColor`, `FrameBorderWidth` | `Window.Border.Color`, `Width` | 🔄 |
| `FrameFocusedBorderColor`, `FrameSelectedBorderColor` | o wlmaker tem uma cor de borda só | ❌ |
| `PTitleBack`, `PTitleColor` (dona da janela focada) | o wlmaker não tem esse estado | ❌ |
| `MenuTextBack`, `MenuTextColor`, `MenuDisabledColor` | `Menu.Item.Fill`, `EnabledTextColor`, `DisabledTextColor` | 🔄 |
| `HighlightColor`, `HighlightTextColor` | `Menu.Item.HighlightedFill` (sólido), `HighlightedTextColor` | 🔄 |
| `MenuStyle` | `flat` → `Menu.Item.BezelWidth = 0`; `normal`/`singletexture` → 1 (a textura é sempre por item) | 🔄 |
| `MenuTitleBack`, `MenuTitleColor`, `MenuTitleFont` | o título dos menus usa o estilo da barra de título das janelas | ❌ |
| `IconBack` | `Tile.Fill` (ladrilhos do Clip e dos dockapps) | 🔄 |
| `ClipTitleColor`, `ClipTitleFont` | `Clip.TextColor`, `Clip.Font` | 🔄 |
| `CClipTitleColor` | o Clip do wlmaker não recolhe | ❌ |
| `IconTitleColor`, `IconTitleBack`, `IconTitleFont` | sem mini-ícones | ❌ |
| `WindowTitleExtendSpace`, `MenuTitleExtendSpace`, `MenuTextExtendSpace` | alturas fixas no tema | ❌ |
| `WindowTitleFont`, `MenuTextFont` | `Window.TitleBar.Font`, `Menu.Item.Font` | 🔄 |
| `LargeDisplayFont` | `TaskList.Font` (a lista do Alt+Tab, o texto grande do wlmaker) | 🔄 |
| `SystemFont`, `BoldSystemFont`, `DefaultFontSize` (`WMGLOBAL`) | o WPrefs e os outros aplicativos WINGs leem o `WMGLOBAL` também no XWayland | ✅ |
| `WorkspaceBack` | papel de parede: imagem (`spixmap`/`fpixmap` → fill, `mpixmap` → fit, `cpixmap` → center, `tpixmap` → tile, caminho resolvido pelo `PixmapPath`) ou cor (`color:#rrggbb`) em `~/.config/big-wm/wallpaper`, aplicado pelo `big-wm-wallpaper` (swaybg); a cor também em `BackgroundColor` | 🔄 |
| `WorkspaceSpecificBack` (fundo por área) | o swaybg é um fundo para todas as áreas | ❌ |
| Estilos e temas (`setstyle`, menu Aparência) | menus Aparência > Temas e Estilos gerados pelo `big-wm-wprefs-sync --open-menu` (`setstyle` + sync) | 🔄 |

### Menu raiz (editor de menu do WPrefs) e atributos

| Item do `WMRootMenu` | `RootMenu.plist` | Status |
|---|---|---|
| `EXEC`, `SHEXEC` | `ShellExecute` (`arandr` → `wdisplays`, `xkill` → `big-wm-kill`, `wmagnify` → `OutputMagnify`, `big-wm-lock` → `LockScreen`) | 🔄 |
| `SHORTCUT` | atalho no `KeyBindings` | 🔄 |
| `OPEN_PLMENU "\|\| big-wm-appmenu …"` | `GeneratePlistMenu "big-wm-appmenu --wlmaker …"` | 🔄 |
| `OPEN_PLMENU` (outro comando ou arquivo), `OPEN_MENU` (diretórios `WITH` comando, arquivo de menu no formato antigo, `\| comando`) | `GeneratePlistMenu "big-wm-wprefs-sync --open-…"`, convertido ao abrir a sessão; `WITH setstyle` sincroniza o estilo, `WITH wmsetbg` vira `big-wm-wallpaper` | 🔄 |
| `WORKSPACE_MENU` | submenu com Nova, Remover a última, Anterior, Próxima | 🔄 |
| `EXIT`, `SHUTDOWN` | `Quit` | 🔄 |
| `RESTART` (sem argumento) | "recarregar a configuração" (`big-wm-wprefs-sync`) | 🔄 |
| `RESTART outro-wm`, `HIDE_OTHERS`, `SHOW_ALL`, `ARRANGE_ICONS`, `REFRESH`, `SAVE_SESSION`, `CLEAR_SESSION`, `INFO_PANEL`, `LEGAL_PANEL`, `WINDOWS_MENU`; comandos com `%a`/`%A` (perguntam) ou `%w`/`%W` (id de janela X11) | o item sai do menu Wayland (submenu vazio também) | ❌ |
| `WMRootMenu` como caminho de um arquivo de menu | lido (plist ou formato antigo com `#include`) | 🔄 |
| Atributos de janela (`WMWindowAttributes`: sem barra, sempre acima, área fixa, ícone…) | o wlmaker não tem regras por aplicativo | ❌ |
| Idioma (locale) e terminal | o WPrefs não tem esses painéis; as duas sessões usam o idioma do sistema e o `big-wm-open terminal` | ✅ |

**Contagem** (linhas das tabelas acima): ✅ 15 já funcionam no Wayland ·
🔄 40 convertidos pelo `big-wm-wprefs-sync` · 🩹 8 convertidos com patch no
wlmaker (feitos, 0075–0079) · 🔧 2 com o Dock do Window Maker (0084, 0086: D13, D17) · ❌ 34 sem equivalente. As linhas ❌ juntam cerca
de 160 chaves e itens do WPrefs, quase todos de recursos que o wlmaker não
tem: mini-ícones e área de ícones, balões fora do Dock/Clip, animações, regras por aplicativo,
meia tela e encaixe nas bordas, ações de cliques na área de trabalho.

## Dock, Clip e bandeja

| ID | Recurso | X11 | Wayland | Solução / arquivo | Teste | Status |
|---|---|---|---|---|---|---|
| D1 | Lançadores no Dock | ✅ | ✅ | `data/wlmaker/State.plist` | VM | ✅ |
| D2 | Indicador de aplicativo em execução no Dock | ✅ três pontinhos no canto do lançador cujo aplicativo não está aberto; pontilhado enquanto abre | ✅ o mesmo: os três pontinhos do Window Maker (make3Dots) quando o aplicativo do `AppID` não tem janela, o ladrilho pontilhado enquanto abre (até a janela aparecer, no máximo 10 s); lançadores sem `AppID` contam como abertos enquanto o processo lançado vive | wlmaker patch 0086 | teste unitário do wlmaker (`launcher`: pontinhos sem janela, nenhum com janela); captura headless | ✅ |
| D3 | Dockapps (relógio/calendário, CPU, memória, rede, bateria) | ✅ | ✅ nativos (`wlmcalclock`, `wlmcpugraph`, `wlmmemgraph`, `wlmnetgraph`; `wlmbattery` só quando existe bateria), ladrilhos abaixo do Clip; podem ir para o Dock (D11) e voltam para onde estavam ao abrir de novo (ordem "Order" e "Dock" no `~/.config/wlmaker/Dockapps.plist`) | `big-wm-dockapps` (lista em `~/.config/big-wm/dockapps`; pula os da lista `Removed`) | VM: sessão real (captura 26/10); CI (`wayland-dock.sh`) | ✅ |
| D4 | Dockapp de volume | ✅ wmix | ✅ `wlmmixer` (nativo, PulseAudio/PipeWire) | patch do wlmaker (big-wm-packages) | VM: sessão real (captura 26/10) | ✅ |
| D5 | Dockapps X11 (wmshutdown, wmusic, pclock...) | ✅ | ❌ não acoplam; o wmshutdown terminava com SIGSEGV aberto pelo menu | `NotShowIn=wlmaker;` nos .desktop (big-dockapps pkgrel 3); Aparência > Dockapps lista só os nativos | VM (coredump) | ✅ fora dos menus |
| D6 | Bandeja (StatusNotifierItem): rede, atualizações, Bluetooth, som (Sound Switcher Indicator) | ✅ wmsystemtray + snixembed | ✅ `big-wm-tray`: o mesmo ladrilho de 64×64 do wmsystemtray, logo depois dos lançadores do Dock (segue a borda e o canto do Dock no `State.plist`): 4 ícones de 24×24 (ou 9 de 16×16, Configurações... > Ícones pequenos, o `-s`), paginação `<- 1/2 ->` com as setas do XPM do wmsystemtray e a fonte 5x8 (clique nas setas, roda sobre a barra, SIGUSR1/SIGUSR2); fundo do Tile do tema do wlmaker, claro ou escuro na hora (`big-wm-theme`); balão do Window Maker com o ToolTip/Title/Id do item e, sobre o ladrilho, "Bandeja do sistema (big-wm-tray)"; esquerdo/meio/direito/rolagem vão para o item (menu dbusmenu), direito fora dos ícones abre o menu do ladrilho (Configurações..., Terminar com confirmação); ícone por IconName, IconThemePath, IconPixmap, atenção, sobreposição e, quando o tema não tem o nome (pamac-tray no início: `org.manjaro.pamac`), o ícone do .desktop. Diferenças: o menu do ladrilho não tem Dock position, Add a drawer, Executar, Trazer aqui nem Esconder (não se aplicam a um cliente layer-shell; mover o Dock fica no ladrilho principal do Dock); a bandeja não é arrastada nem explode (não é um dockapp do wlmdock: acompanha o Dock quando ele muda de lado) | `session/bin/big-wm-tray` | CI e contêiner: `tests/integration/wayland-tray.sh` (26 verificações: 1/2 com 5 itens falsos, setas e roda pelo ponteiro virtual do compositor, Activate/SecondaryActivate/ContextMenu/Scroll, balão, menu do ladrilho, Passive→Active, modo escuro na tela, instância única); `tests/run.sh` (paginação, áreas de clique, balão, temas); VM live: wlmaker aninhado no X11 ao lado do wmsystemtray real com nm-applet e pamac-tray (capturas 27/10); VM instalada: nm-applet, pamac-tray e Sound Switcher com ícone pelo tema (`--state`) | ✅ |
| D7 | Ícones XEmbed antigos (GtkStatusIcon) | ✅ | ❌ não existe XEmbed no Wayland | apps atuais usam SNI | — | ❌ P3 |
| D8 | Ícones de janelas minimizadas | ✅ (com ícone de reserva) | 🔧 miniaturizar esconde a janela (botão, menu da janela, pedido do app; Alt+Tab ou ativação a devolve), sem mini-ícone; o Big Tabbed Dock lista as janelas (foreign-toplevel) | wlmaker patches 0020, 0021 e 0042; a "Icon area" é roadmap upstream | teste unitário do wlmaker | 🔧 P2 |
| D9 | Clip no canto superior esquerdo com Navegador e Arquivos fixos | ✅ | ✅ `wlmdock --clip` (`Clip.Launchers` no `State.plist`) | autostart `big-wm-wayland-clip` com `WLR_RENDERER=pixman` (o wlmdock caía no nó DRM e ficava invisível) | VM: sessão real (captura 26/10); CI: Dock e Clip sobem e saem com status 0 junto com o compositor | ✅ |
| D10 | Menu do botão direito no Dock, Clip e dockapps (como no X11) | ✅ | ✅ o menu do Window Maker em todo ladrilho: no Dock, **Dock position** (Normal / Auto raise & lower / Manter sobre outros), **Add a drawer**, **Executar**, **Trazer aqui** (ou Mostrar Aqui), **Esconder** (ou Mostrar), **Configurações...**, **Terminar**; no Clip, **Opções do Clip** (Manter sobre outros, Autoraise) no lugar dos dois primeiros e **Remover Ícone** nos lançadores; desabilitado o que não se aplica, como no X11 (Trazer aqui/Esconder sem janela aberta, Terminar sem processo). Textos do `pt.po` do Window Maker (Dock position, Add a drawer, Normal e Auto raise & lower não são traduzidos lá). Sem duplicar: o "Preferências" dos dockapps virou o **Configurações...** deles (submenu com as opções, como antes), "Lançar/Encerrar/Remover" do wlmdock deram lugar a Executar/Terminar/arrastar para fora. Extras que já existiam e não têm par no X11 ficam depois: mover para cima/baixo, mover Dock/Clip de lado, Restaurar lançadores padrão | wlmaker patch 0084 (o compositor abre o menu também para os ladrilhos do `wlmdock`); `big-wm-dock-settings` (DockSettingsCommand) | headless: capturas dos menus; CI: `wayland-dock.sh` (captura) | ✅ (Add a drawer desabilitado: D15) |
| D11 | Arrastar dockapps e lançadores entre o Dock e o Clip, com o fantasma do lugar | ✅ | ✅ o ladrilho vai com o ponteiro e o fantasma (o ícone embranquecido, como o MakeGhostIcon) mostra onde vai ficar: no próprio dock até 3 ladrilhos de distância, no outro quando bem em cima dele (wDockSnapIcon); ao soltar, o ícone desliza até o lugar. Lançadores vão para os lançadores do Dock ou do Clip; dockapps para os dockapps do Dock (logo depois dos lançadores dele) ou do Clip. Dockapps se arrastam pela borda do ladrilho (ou com Alt), como no X11. A bandeja (`big-wm-tray`) fica depois dos lançadores e dos dockapps do Dock (soma o `DockappTiles` do `State.plist`, com a margem de 1 px entre os ladrilhos) e muda de lugar na hora | wlmaker patches 0080, 0082, 0084 | CI: `tests/integration/wayland-dock.sh` (Clip → Dock → Clip pelo ponteiro virtual, `Dockapps.plist` e `DockappTiles`; a bandeja vai para depois do dockapp e volta); teste unitário `tile_drag.snap`, `dockapps.two_docks`; capturas headless | ✅ |
| D12 | Soltar fora do Dock/Clip: "kaboom" | ✅ ícone de aplicativo fechado explode (DoKaboom) e sai do dock; aplicativo aberto ou travado volta para o dock (com Alt, um aberto sai sem explodir e vira ícone solto) | ✅ a mesma animação (pedaços de 4×4, metade sorteada, a cada 20 ms, até saírem da tela) e as mesmas regras para lançadores (Travado = `Lock = Yes`). Dockapp: no X11 um dockapp aberto voltaria ao Dock (ou, com Alt, ficaria solto); no Wayland não há ícones soltos, então — como pedido — ele explode, é encerrado e vai para `Removed` no `Dockapps.plist` (não inicia mais com a sessão até ser aberto de novo pelo menu Aparência > Dockapps) | wlmaker patches 0080, 0084; `big-wm-dockapps` | CI: `wayland-dock.sh` (processo encerrado, `Removed`); teste unitário `dock_fx.kaboom`; capturas headless | ✅ (diferença de regra para dockapps documentada) |
| D13 | Balão com o nome sobre os ícones do Dock/Clip (`AppIconBalloons`) | ✅ | ✅ o balão do Window Maker (caixa arredondada, contorno e ponta, Noto Sans Bold 12 px, atraso de 1 s e sem atraso ao passar de um ícone para o outro): nome e (comando) para lançadores e dockapps, o nome da área no Clip quando tem mais de 5 letras; cores do tema (`Balloon` no `Theme.plist`, padrão preto no branco como no X11 claro e escuro) | wlmaker patches 0080, 0084 | teste unitário `dock_fx.balloon` (posição, desenho, atraso); CI `wayland-dock.sh` (captura) | ✅ |
| D14 | Dock position: Normal, Auto raise & lower, Manter sobre outros (e Opções do Clip) | ✅ | ✅ Normal põe o Dock (e os lançadores, dockapps e a camada do wlmdock) abaixo das janelas, Manter sobre outros acima (padrão), Auto raise & lower sobe 600 ms depois de o ponteiro chegar e desce 1 s depois de sair (ClipAutoraiseDelay/ClipAutolowerDelay); guardado como `Lowered`/`AutoRaiseLower`, os nomes do WMState; a bandeja (`big-wm-tray`) fica na mesma camada (no Auto raise & lower, sobe com o ponteiro sobre ela) | wlmaker patch 0084 | captura headless do submenu | 🔧 sem teste automático do empilhamento |
| D15 | Gavetas (Add a drawer) | ✅ | ❌ o item aparece (como no X11) mas desabilitado: o wlmaker não tem gavetas | — | — | ❌ P3 |
| D16 | Configurações... do lançador ("Configurações de Aplicação na posição Dock") | ✅ | ✅ `big-wm-dock-settings`: caminho e argumentos, AppID, imagem do ícone (Navegar...), Iniciar com a sessão (`AutoLaunch`), Travado (`Lock`); dockapps: o submenu com as opções deles | `session/bin/big-wm-dock-settings`; `DockSettingsCommand` no `Config.plist` (o `big-wm-firstrun` acrescenta) | headless: menu → Configurações... → Travado → Ok gravou `Lock = Yes` e o lançador não saiu do Dock ao ser solto fora (capturas) | 🔧 sem "Comando do clique do meio" nem "Comando para arquivos soltos" (o wlmdock não tem) |
| D17 | Clique duplo para abrir (`SingleClickLaunch = NO`); clique duplo em aplicativo aberto o mostra | ✅ | ✅ igual; `SingleClickLaunch = Yes` no `Config.plist` volta ao clique simples; o tempo é o `Mouse.DoubleClickTime` | wlmaker patch 0086 | — | 🔧 sem teste automático |
| D18 | Clicar fora de um menu de aplicativo (popup) o fecha | ✅ | ✅ clicar na área de trabalho encerra o grab do xdg_popup (antes o menu do GTK ficava aberto) | wlmaker patch 0085 | headless: menu de um GtkMenuButton fecha com o clique na área de trabalho; sem o patch, continua aberto (contraprova) | ✅ |

## Janelas

| ID | Recurso | X11 | Wayland | Solução / arquivo | Teste | Status |
|---|---|---|---|---|---|---|
| W1 | Barra de título do Window Maker em apps GTK 3/4 e libadwaita | ✅ gtk-nocsd | ✅ gtk-nocsd + protocolo de decoração do KDE | wlmaker patch 0002; `big_wm_nocsd` em `big-wm-env.sh` (só com wlmaker que tem o protocolo) | VM, wlmaker headless: gnome-calculator e gedit (capturas) | ✅ |
| W2 | Barra de título em apps Qt | ✅ | ✅ (`xdg-decoration`) | — | CI | ✅ |
| W3 | Alt+Tab, maximizar, tela cheia, fechar, mover entre áreas | ✅ | ✅ | `Config.plist` | — | 🔧 |
| W4 | Ativação de janela (abrir app já aberto, links) | ✅ | ✅ `xdg-activation-v1` (na área de trabalho atual) | wlmaker patch 0001 | cliente de teste no wlmaker headless; CI | ✅ |
| W5 | Diálogos de arquivo do portal presos à janela | ✅ | 🔧 `xdg-foreign` v1/v2 anunciado; o wlmaker ainda não aplica `set_parent` | wlmaker patch 0008 | — | 🔧 P2 |
| W6 | Métodos de entrada (IBus/Fcitx5) | ✅ | ❌ sem `text-input`/`input-method` no wlmaker | — | CI (XFAIL) | ❌ P2 |

## Bloqueio, energia e serviços

| ID | Recurso | X11 | Wayland | Solução / arquivo | Teste | Status |
|---|---|---|---|---|---|---|
| S1 | Bloquear (menu, Super+L) com a imagem do Big WindowMaker | ✅ i3lock | ✅ swaylock (`ext-session-lock`) | `big-wm-lock` | VM: bloquear e desbloquear com a senha (capturas) | ✅ |
| S2 | `loginctl lock-session` | ✅ xss-lock `--session` | ✅ | `big-wm-lock --watch` | VM | ✅ |
| S3 | Bloquear antes de suspender | ✅ (`XSS_SLEEP_LOCK_FD`) | ✅ swayidle `-w before-sleep` (o wlmaker agora tem `ext-idle-notify`) | wlmaker patch 0009; `big-wm-lock --watch` | swayidle no wlmaker headless | 🔧 sem teste de suspensão real |
| S4 | Bloqueio com um menu (popup) aberto | ✅ | ✅ o bloqueio encerra as capturas (popup_done) e o teclado vai para o swaylock | wlmaker patches 0004 e 0005 | teste unitário; swaylock + PAM no wlmaker headless | ✅ (confirmar na VM) |
| S5 | Recuperar bloqueio se o locker morrer | — | ✅ outro locker assume o bloqueio abandonado | wlmaker patch 0003 | teste unitário; swaylock no wlmaker headless | ✅ (confirmar na VM) |
| S6 | Notificações | ✅ dunst | ✅ dunst | — | CI | ✅ |
| S7 | Clipboard e seleção primária | ✅ | ✅ | — | CI | ✅ |
| S8 | Rede (nm-applet) | ✅ | ✅ pela bandeja | — | VM | ✅ |
| S9 | Polkit (senha de administrador) | ✅ | ✅ | autostart `big-wm-polkit` | — | 🔧 |
| S10 | Monitores | ✅ arandr | 🔧 wdisplays (agora dependência) | `pkgbuild/PKGBUILD` | — | 🔧 |
| S11 | Captura de tela e área (menu Captura de tela, Print, Shift+Print) | ✅ scrot (menu) e teclas do Window Maker | ✅ grim/slurp; o slurp não herda mais a entrada padrão do wlmaker (pelo menu ele esperava para sempre) | `big-wm-screenshot` | `tests/run.sh`; wlmaker headless: Área pelo menu raiz | ✅ |
| S12 | Captura de janela (menu, Alt+Print) | ✅ clique na janela (scrot) | 🔧 arrastar em volta da janela (slurp): o wlmaker não oferece `ext-foreign-toplevel-image-capture-source` nem a posição das janelas para o `grim -T` | `big-wm-screenshot window` | wlmaker headless | 🔧 P2 |
| S14 | Forçar o fechamento (Ctrl+Alt+Esc) | ✅ caveira do `xkill` | 🔧 lista das janelas (wlr-foreign-toplevel) com a ativa escolhida e confirmação; sem "clique para matar": o wlmaker não diz qual janela está sob o ponteiro nem o PID do cliente | `big-wm-kill` | Xvfb: Ctrl+Alt+Esc + clique encerrou o xterm; wlmaker headless: calculadora encerrada | 🔧 P2 |
| S13 | Compartilhar tela (portal ScreenCast) | ✅ | ❓ o portal wlr exige `linux-dmabuf` (GPU) | `wlmaker-portals.conf` | CI: SKIP sem GPU | ❓ |

## Desempenho

Sessão logo após o login, cache descartado, PSS (método em
[memoria.md](memoria.md)):

| | X11 | Wayland |
|---|---|---|
| PSS dos processos do usuário | 191 MB (live) | 335 MB (instalado) |
| Gerenciador de janelas | wmaker 14 MB | wlmaker 82 MB (renderização por software na VM) |
| Bandeja | snixembed 6 MB | big-wm-tray 33 MB (Python + GTK 3; a paginação não mudou o consumo: no contêiner, 63 MB de RSS com 6 itens, antes 65 MB) |
| XWayland | — | sob demanda com `--lazy_xwayland` (wlmaker patch 0017): ~35 MB a menos até o primeiro app X11 |

As medições não são do mesmo sistema (live x instalado), então servem de
ordem de grandeza. Para baixar mais: uma bandeja nativa em C no lugar da de Python.

## ISO

| Cenário | Resultado | Evidência |
|---|---|---|
| Live BIOS e UEFI (smoke test), sessão X11 | ✅ | build-iso run 40385383947 |
| Instalação Calamares BIOS e UEFI, login X11 no instalado | ✅ | build-iso run 40385383947 (a falha da UEFI na 1ª tentativa foi download de artefato do GitHub; nova tentativa passou) |
| Login Wayland no sistema instalado (BIOS) | ✅ | VM, entrando pelo greeter |
| Login Wayland no sistema instalado (UEFI) | ❓ | pendente |
| Login Wayland no live | ❓ | o live inicia direto na sessão X11 |
