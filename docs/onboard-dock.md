# Onboard (teclado virtual) no Dock e no Clip

Investigação de novembro de 2026 (rodada 2). Relato do usuário: "arrastei o ícone
do teclado virtual (`onboard`) para o Dock e o ícone sumiu".

## Problema

1. O ícone de aplicativo do Onboard "sumiu" ao ser ancorado na sessão Wayland.
2. Pergunta derivada: o Onboard funciona na sessão Wayland (wlmaker)? Qual teclado
   virtual deve ir no Dock/Clip da sessão Wayland?

## Identificação do Onboard

| Item | Valor |
|---|---|
| `.desktop` | `/usr/share/applications/onboard.desktop`: `Exec=onboard`, `Icon=onboard`, **sem** `StartupWMClass` |
| Pacote | `onboard 1.4.1-14` (GTK3/Python, sem suporte próprio a Wayland) |
| X11 / XWayland | janela `WM_CLASS("onboard", "Onboard")`, tipo `_NET_WM_WINDOW_TYPE_NORMAL`, `_NET_WM_STATE_SKIP_TASKBAR`, `_NET_WM_STATE_ABOVE`, `STICKY`, `WM_HINTS.input = False`, propriedade `ONSCREEN_KEYBOARD = 1`; o líder de grupo tem `WM_CLASS("onboard", "")`. Não é override-redirect. |
| App ID que o wlmaker publica | XWayland: a **classe**, `Onboard` (`xwl_surface.c`, `wlmaker_toplevel_tracker_handle_set_app_id(..., s->class)`); Wayland nativo: `onboard` |
| Como roda na sessão real | `gtk-launch onboard.desktop` sob Wayland → GTK3 escolhe o backend **Wayland**: é um `xdg_toplevel` nativo, app_id `onboard` |
| Linha do `big-wm-appicons.plist` (Wayland nativo) | `Name = "Teclado virtual"; AppID = "onboard"; AppIDs = ("onboard"); CommandLine = "gtk-launch onboard.desktop"; Icon = "big-wm-app-onboard.desktop.png";` |
| Linha do `big-wm-appicons.plist` (XWayland, `GDK_BACKEND=x11`) | igual, mas `AppID = "Onboard"` |
| PNG | `~/.local/share/wlmaker/icons/big-wm-app-onboard.desktop.png` (gravado pelo `big-wm-appicons` ao criar o ícone, 03:49:07 na sessão real) |
| Lançador gravado (sessão real) | `Clip.Launchers[5] = { AppID = onboard; CommandLine = "gtk-launch onboard.desktop"; Icon = "big-wm-app-onboard.desktop.png"; }` |
| X11 (Window Maker) | `WMState`: `Name = onboard.Onboard; Command = "gtk-launch onboard.desktop"; DropCommand/PasteCommand` com `%d`/`%s` |

## Causa do "sumiu"

### O que a sessão real mostra

Log da sessão real (`~/.cache/wlmaker.log`, só leitura):

```
03:49:07.388 xdg_toplevel.c  Created XDG toplevel 0x55f8a9e65ba0          <- Onboard nativo
03:49:07.389 layer_panel.c   Created layer panel ... (fileira de ícones)
03:49:16.249 tile_drag.c:502 Docking "gtk-launch onboard.desktop" in column 0 at 4
03:49:16.250 clip.c:2242     Clip ...: Room for 5 launchers.
03:49:16.350 layer_panel.c   Destroying layer panel ... (fileira ficou vazia)
04:07:34.373 tile_drag.c:502 Docking "gtk-launch vscodium-insiders-bin.desktop" in column 0 at 4
04:07:34.374 clip.c:2242     Clip ...: Room for 6 launchers.
```

A coluna 0 é o **Clip** (`WLMAKER_COLUMN_CLIP`), não o Dock. A ancoragem deu certo:
o lançador está no `State.plist` (6º do Clip, abaixo do VSCodium, que foi inserido
*antes* dele às 04:07), o PNG existe e nenhuma edição posterior o removeu. Com o
`State.plist` real copiado para o laboratório (binários instalados e de base), o
Onboard aparece no Clip, com o indicador de "em execução"
([wayland-clip-onboard.png](screenshots/onboard-dock/wayland-clip-onboard.png)).

Conclusão: na sessão real **o ícone não se perdeu**. O que o usuário viu:

- o ícone some da fileira de ícones de aplicativo **por projeto** (um aplicativo
  ancorado é representado pelo lançador, como no Window Maker; `is_docked()` em
  `tabbed-dock/src/ui/btd-appicons.c`), e
- o lançador foi para o **Clip** (borda esquerda, entre os lançadores e as dock-apps
  do Clip), não para o Dock da direita. Se o Clip estiver em outra tela, ou o
  usuário estiver olhando para o Dock, parece que o ícone sumiu. Depois, o VSCodium
  ancorado no mesmo lugar empurrou o Onboard uma posição para baixo.

### Defeitos reais encontrados (podem fazer o ícone sumir de fato)

**D1 — Lançador perdido para sempre quando o PNG falta (wlmaker).**
`src/dock_launchers.c`, `wlmaker_dock_launchers_load()` →
`_wlmaker_dock_launchers_shown()` descarta os lançadores cujo ícone não é encontrado,
e *todas* as edições (`wlmaker_dock_launchers_edit`, `_move`, `_replace_drawer`,
`_set_drawer_flag`) partem dessa lista filtrada e a gravam de volta com
`wlmaker_dock_launchers_store()`. Reproduzido no laboratório: com o
`State.plist` real, renomeei `big-wm-app-onboard.desktop.png`, reiniciei e ancorei o
XTerm no Clip. Resultado (`diff` do `State.plist`):

```
<         AppID = onboard;
<         CommandLine = "gtk-launch onboard.desktop";
<         Icon = "big-wm-app-onboard.desktop.png";
---
>         AppID = XTerm;
>         CommandLine = "gtk-launch xterm.desktop";
>         Icon = "big-wm-app-xterm.desktop.png";
```

O Onboard sumiu do estado (`grep -c onboard` = 0) e não volta quando o PNG volta.
Efeito colateral: `_wlmaker_clip_reload_launchers()` (`src/clip.c`) conta os
lançadores **brutos** do estado ("Room for 6") enquanto o `wlmdock --clip` só
desenha os 5 com ícone: fica uma vaga vazia e os índices de arrastar ficam
deslocados. O `wlmdock` (`src/dock/launcher.c`, `wlmdock_launcher_create_from_plist`)
também descarta lançadores sem ícone, e também quando o PNG existe mas não carrega
(`wlmtk_image_create_scaled` falha), que o filtro do wlmaker (só existência) não vê.

**D2 — App ID com maiúsculas diferentes: sem indicador e sem ícone.**
Se o Onboard roda pelo XWayland (ex.: `GDK_BACKEND=x11`, ou as ações
`SoftwareRender onboard` do `.desktop`), o app_id é `Onboard`, mas o lançador
gravado a partir da sessão nativa tem `AppID = onboard`. O wlmaker compara com
`strcmp` (`src/toplevel_tracker.c:415`, `wlmaker_toplevel_tracker_for_each_app_window`;
`src/dock/toplevel.c:420`, `_wlmdock_toplevel_app_id_cmp`; `src/dock/launcher.c:768`),
então o lançador fica "parado" (com os três pontos), enquanto o `big-wm-appicons`
compara sem caixa (`is_docked()`, `g_ascii_strdown`, e ainda pelo nome do
`.desktop`) e **esconde** o ícone da fileira. Resultado: o Onboard em execução não
tem ícone em lugar nenhum, e um clique duplo no lançador abre um segundo Onboard
([wayland-xwayland-onboard-sem-running.png](screenshots/onboard-dock/wayland-xwayland-onboard-sem-running.png):
o Onboard aberto, o lançador do Clip com os pontos, a fileira só com XTerm e gedit).

## O Onboard funciona no Wayland?

**Não, hoje ele não digita em nada na sessão Wayland.** Testado no laboratório:

| Modo | Escolha do Onboard | Resultado |
|---|---|---|
| Nativo Wayland (como na sessão real, `gtk-launch`) | `XInput ... not an X display`; key-synth cai para **uinput** | Cada tecla dá `osk.error: write key event`; as teclas ficam "presas" e abre o popup de acentos; nada chega ao gedit. Além disso, clicar no Onboard o **ativa** (título escuro) e tira o foco do gedit ([wayland-onboard-nativo-foco.png](screenshots/onboard-dock/wayland-onboard-nativo-foco.png)). No Wayland ele também não fica acima das outras janelas. |
| XWayland (`GDK_BACKEND=x11`) | `Using key-synth 'KeySynthEnum.XTEST'` | Nada chega ao xterm nem ao gedit. Causa: o clique no Onboard ativa a janela dele; o wlmaker desativa o xterm e o foco X vira `PointerRoot` (`xdotool getwindowfocus` → janela 1). O XTEST do Xwayland em si funciona: `xdotool type abc` no xterm focado chegou (`abc` no arquivo). Mesmo com o foco certo, o XTEST só alcança clientes X, nunca os nativos Wayland. |
| X11 (Window Maker) | XTEST | Funciona: `qwe` + seta digitados no xterm; o Window Maker respeita `WM_HINTS.input = False` e não dá foco ao Onboard. |

Sobre uinput: o usuário tem ACL `rw` em `/dev/uinput` (regra udev de controle de
jogos), então na sessão real o Onboard nativo pode criar um teclado no kernel. No
laboratório isso falhou e **não tentei fazer funcionar de propósito**: um dispositivo
uinput é do host inteiro e injetaria teclas na sessão real do usuário. Mesmo que
funcionasse, as teclas iriam para a janela focada, que é o próprio Onboard (ver
acima). Não é uma solução para a ISO (depende de ACL no uinput).

### Teclados Wayland disponíveis

- **squeekboard** (`extra`, 1.43.1-5; dependências não instaladas: `feedbackd`,
  `gnome-desktop`). Baixado de `mirrors2.manjaro.org` e extraído no scratch (sem
  instalar), rodou com as bibliotecas do sistema. Usa `zwlr_layer_shell_v1` (painel
  na borda de baixo, não rouba foco) e `zwp_virtual_keyboard_v1`, que o wlmaker tem
  (`src/input/manager.c`). Digitou **nos dois mundos**: `hi` no gedit nativo Wayland
  e `ea⏎` no xterm do XWayland ([wayland-squeekboard.png](screenshots/onboard-dock/wayland-squeekboard.png)).
  O wlmaker **não** implementa `zwp_input_method_manager_v2` nem
  `zwp_text_input_manager_v3` (grep em `src/`): o squeekboard avisa
  `Wayland input method interface not available` e não aparece sozinho ao focar um
  campo; mostra/esconde pelo D-Bus `sm.puri.OSK0.SetVisible`. Layout padrão `us`;
  ele tem `br`/`pt`, escolhidos por `org.gnome.desktop.input-sources` (gsettings).
- **wvkbd**: não está nos repositórios.
- **Teclado da tela de bloqueio** (`session/lockscreen/gtklock-keyboard.c`,
  `big-wm-keyboard.so`): só funciona dentro do gtklock. Não foi alterado.

## Solução / recomendação

### Lançador do Dock/Clip na sessão Wayland

Recomendado: **squeekboard com um alternador**, não o Onboard.

```
{
  AppID = sm.puri.Squeekboard;
  CommandLine = "/usr/bin/big-wm-osk";
  Icon = "big-wm-app-onboard.desktop.png";   // ou um PNG próprio, ex. big-wm-osk-56x56.png
}
```

`big-wm-osk` ainda **não existe**: é um script a criar no big-wm. Protótipo testado
no laboratório (alternou `Visible` true/false; `shellcheck` limpo):

```sh
#!/bin/sh
v=$(gdbus call --session --dest sm.puri.OSK0 --object-path /sm/puri/OSK0 \
    --method org.freedesktop.DBus.Properties.Get sm.puri.OSK0 Visible) || exit 1
case $v in *true*) n=false ;; *) n=true ;; esac
exec gdbus call --session --dest sm.puri.OSK0 --object-path /sm/puri/OSK0 \
    --method sm.puri.OSK0.SetVisible "$n"
```

O script final deve iniciar o `squeekboard` se ele não estiver no barramento (ou
o pacote ganha um autostart só para `OnlyShowIn=wlmaker`). O squeekboard não tem
janela (`xdg_toplevel`), então o lançador nunca mostra "em execução" e nunca há ícone
de aplicativo duplicado. O `AppID` é só um rótulo.

Sem o squeekboard empacotado: **nenhum** teclado no Dock/Clip do Wayland. O Onboard
não deve ser oferecido ali enquanto F3 (abaixo) não estiver feito; e, mesmo com F3,
ele só serviria para aplicativos XWayland, com
`CommandLine = "env GDK_BACKEND=x11 onboard"`, `AppID = Onboard`,
`Icon = "big-wm-app-onboard.desktop.png"`.

Na sessão X11 (Window Maker) o Onboard continua a escolha certa.

### Correções propostas (para o agente do Dock aplicar)

- **F1 (D1, wlmaker)**: separar "mostrar" de "gravar". `wlmaker_dock_launchers_edit`,
  `_move`, `_replace_drawer` e `_set_drawer_flag` devem editar a lista **bruta** do
  estado (`bspl_dict_get_array(..., "Launchers")` sem `_wlmaker_dock_launchers_shown`)
  e mapear o índice visível → índice bruto (pular as entradas sem ícone ao contar).
  Alternativa mais simples e mais Window Maker: não filtrar; o `wlmdock` desenha um
  lançador sem ícone com um ícone genérico (ex. `big-wm-apps-56x56.png` /
  "application-x-executable") em vez de `wlmdock_launcher_destroy()` em
  `wlmdock_launcher_create_from_plist` (`src/dock/launcher.c` ~l.228-275), e o
  filtro `_wlmaker_dock_launchers_shown` some. Isso também elimina a vaga vazia
  (`_wlmaker_clip_reload_launchers` conta as entradas brutas). Teste unitário: estado
  com 3 lançadores, o do meio com ícone inexistente; `wlmaker_dock_launchers_edit`
  inserindo outro não pode remover o do meio.
- **F2 (D2)**: comparar app IDs sem diferenciar maiúsculas, nos três pontos:
  `src/toplevel_tracker.c:415` (`strcmp` → `strcasecmp`), `src/dock/toplevel.c:420`
  (`_wlmdock_toplevel_app_id_cmp`: `strcasecmp`, mantém a ordem da árvore AVL
  consistente) e `src/dock/launcher.c:768`. Assim o lançador `onboard` acende com a
  janela `Onboard`, como o `big-wm-appicons` já supõe. (Alternativa: deixar o
  `is_docked()` do big-wm exato, mas aí o ícone duplicaria.)
- **F3 (foco, wlmaker)**: não ativar janelas X com modelo de entrada ICCCM "No Input"
  (`WM_HINTS.input = False` sem `WM_TAKE_FOCUS`), como o Window Maker. Em
  `src/xwl_surface.c`, ao mapear/associar, se
  `wlr_xwayland_surface_icccm_input_model(xws) == WLR_ICCCM_INPUT_MODEL_NONE`,
  marcar a janela como "não focável" (nova flag no `wlmtk_window_t`), e em
  `src/toolkit/window.c` (`pointer_button`, ~l.1159
  `wlmtk_workspace_activate_window(...)`, e o foco-segue-ponteiro ~l.1486) só ativar
  se a janela for focável (o raise continua). Com isso o Onboard XWayland digitaria
  nos clientes X. Para clientes nativos não há dica equivalente; um atributo em
  `WindowAttributes` (chave `NoFocus` por app_id) seria a extensão.

## Decisões

- Não alterei código: o brief pede só investigação e documentação; as correções
  ficam para o agente do Dock (ele está mexendo em `dock_launchers.c`/`clip.c`).
- Não mexi nos menus padrão nem nas listas de hash do `big-wm-firstrun`.
- Não testei uinput de verdade: injetaria teclas na sessão real do usuário.
- squeekboard foi só baixado e extraído no scratch; nada instalado.

## Testes feitos (laboratório `dockosk`)

Wayland: wlmaker aninhado em Xvfb, `wlmdock`/`wlmdock --clip`/`big-wm-appicons`
iniciados à mão (o autostart XDG não roda no lab), com os binários de base
(`~/.cache/bwm-wt/dock/base-bin`) e, para comparar com a sessão real, os instalados
(`/usr/bin`). Ponteiro com `xdotool` no Xvfb.

1. Onboard nativo → ícone na fileira (AppID `onboard`) → arrastado para o Clip
   (1280x900, estado padrão): `Docking ... in column 0 at 2`, tile com indicador de
   execução.
2. Estado real copiado (1920x1080), Onboard removido do estado, `gtk-launch
   onboard.desktop`, arrastado para o **Dock**: `column 1 at 4`, visível
   ([wayland-dock-onboard.png](screenshots/onboard-dock/wayland-dock-onboard.png)).
3. Binários instalados, arrastado para baixo das dock-apps do Clip (y=700): encaixa
   depois dos lançadores (`column 0 at 5`), visível.
4. Clique duplo com o Onboard fechado: `App 'gtk-launch onboard.desktop' ...
   terminated, status code 0` e o Onboard abriu.
5. Reinício do wlmaker (wl-down/wl-up com o mesmo HOME): lançador continua no Clip.
6. Mover do Clip para o Dock: `State.plist` → Clip sem, Dock com o Onboard.
7. D1 reproduzido (PNG ausente + ancorar outro app = Onboard apagado do estado).
8. D2 reproduzido (Onboard XWayland: lançador sem indicador e sem ícone na fileira).
9. Digitação: tabela acima (nativo, XWayland, squeekboard).

X11 (`x-up.sh`, Window Maker instalado): ícone do Onboard arrastado para o Clip,
`WMState` com `Command = "gtk-launch onboard.desktop"`, sobreviveu ao reinício do
wmaker, clique duplo abriu o Onboard, digitou no xterm
([x11-clip-onboard.png](screenshots/onboard-dock/x11-clip-onboard.png)).

## Resultado

- Sessão real: o Onboard está no Clip (6º lançador). Nada se perdeu.
- Ancorar, mover, reiniciar e abrir funcionam nas duas sessões.
- Dois defeitos reais (D1 perde lançadores; D2 esconde o Onboard XWayland) e um de
  foco (F3) que impede qualquer teclado X no Wayland.
- O Onboard não digita na sessão Wayland; o squeekboard digita (Wayland e XWayland).

## Limitações

- Não vi a tela real do usuário; a conclusão sobre o "sumiu" vem do log, do estado e
  da reprodução.
- squeekboard sem input-method-v2: não aparece sozinho; layout precisa de
  `org.gnome.desktop.input-sources` (ou seleção manual); ocupa a largura da tela.
- uinput não testado (segurança da sessão real).

## Próximos passos

1. Aplicar F1 e F2 no wlmaker (com testes unitários) e F3 se o Onboard XWayland for
   desejado.
2. Decidir o empacotamento do squeekboard (+ `feedbackd`, `gnome-desktop`) na ISO;
   criar `big-wm-osk` (alternador, inicia o daemon) e um ícone 56x56 próprio; só
   então pôr o lançador no Dock/Clip do Wayland (e configurar o layout `br`).
3. Mais adiante: `zwp_input_method_manager_v2` + `zwp_text_input_manager_v3` no
   wlmaker (wlroots tem `wlr_input_method_v2`/`wlr_text_input_v3`) para o teclado
   aparecer sozinho ao focar um campo de texto.

## Correções aplicadas (ramo `feat/dock-free-placement-2026-11` do wlmaker)

- **F1 (D1)** feito: `wlmaker_dock_launchers_edit/_move/_replace_drawer/_set_drawer_flag`
  editam a lista gravada (`_wlmaker_dock_launchers_load_stored`) e convertem o
  índice mostrado no gravado (`_wlmaker_dock_launchers_stored_index`, teste
  `dock_launchers.stored_index`): um lançador com o PNG faltando não some mais ao
  arrastar outro. O Clip reserva lugar só para os lançadores mostrados (antes
  contava os brutos e sobrava uma vaga). O `wlmdock` mantém, como ladrilho vazio,
  o lançador cujo PNG existe mas não carrega.
- **F2 (D2)** feito: `strcasecmp` em `src/toplevel_tracker.c`,
  `src/dock/toplevel.c` (`_wlmdock_toplevel_app_id_cmp`) e `src/dock/launcher.c`.
- **F3** não feito: mexe em `src/xwl_surface.c` (área da sessão principal) e só
  faria o Onboard XWayland digitar em clientes X. Fica como proposta.
- Além disso, a posição livre no Dock e no Clip (ver
  `appicons-dock-clip.md`, rodada 2) deixa pôr o lançador do teclado em qualquer
  lugar, inclusive depois dos dockapps, e recusa com aviso quando a coluna está
  cheia em vez de deixá-lo sair da tela.
