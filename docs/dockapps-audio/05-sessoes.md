# 05 — Sessões X11 e Wayland: início, posição e migração

## Ordem padrão (contas novas)

| Sessão | Arquivo | Ordem |
|---|---|---|
| X11 | `data/GNUstep/Defaults/WMState` | Logo 0,0 · wmclock 0,1 · bandeja (`wmsystemtray`, pelo `big-wm-systemtray`) 0,2 · **wmpulsemixer 0,3** · **wmusic 0,4** · wmcpuload 0,5 · wmmemload 0,6 · wmnet 0,7 · wmake-zones 0,8 |
| Wayland | `data/wlmaker/State.plist` (`Dock.Dockapps`) | wmclock · `big-wm-tray` · **wmpulsemixer** · **wmusic** · wmcpuload · wmmemload · wmnet · wmake-zones-wl · wmcliphist · wmshutdown · wlmeyes |

- **Bandeja do X11.** No X11 a bandeja continua o `wmsystemtray`. Não foi criado um `big-wm-tray` no X11 só para ter o mesmo nome.
- **Entradas do X11.**
  - `Name` é o WM_CLASS conferido com `xprop`: `wmpulsemixer.WmPulseMixer` e `wmusic.DockApp`.
  - `Command` é `wmpulsemixer -w` e `wmusic -c "big-wm-open music"`.
  - As duas têm `AutoLaunch = YES` e `Lock = YES`.

## Início com a sessão

- **X11.**
  - O Window Maker inicia os acoplados pelo `AutoLaunch`.
  - O `startbigwm` roda antes, nesta ordem: `big-wm-firstrun`, `big-wm-dock-audio x11` (novo) e `big-wm-dockapps-autolaunch`.
  - Nada espera o servidor de áudio: o `wmpulsemixer` espera sozinho, mostrando `--`.
- **Wayland.**
  - O autostart `big-wm-wayland-dockapps.desktop` roda o `big-wm-dockapps`.
  - Grupo de volume: `wmpulsemixer -w | big-wm-mixer | wlmmixer`. Inicia o primeiro disponível, um só.
  - Grupo de música: `wmusic -c "big-wm-open music"`, pelo XWayland (`--lazy_xwayland`: o XWayland sobe com o primeiro dockapp do X11).
  - O `startbigwm-wayland` roda `big-wm-dock-audio wayland` antes de o wlmaker ler o `Dockapps.plist`.
- **Sem instâncias duplicadas.**
  - Novo no `big-wm-dockapps`: um dockapp que já roda nesta sessão (mesmo `DISPLAY` do XWayland, ou mesmo `WAYLAND_DISPLAY` para os nativos) não é iniciado de novo. O motivo vai para o `logger`.
  - Sem a variável não há sessão com que comparar, e o dockapp inicia.
  - No X11 quem garante é o Window Maker (uma entrada por ladrilho).

## Contas existentes: `session/lib/big-wm-dock-audio`

Roda uma vez por sessão (marca `~/.config/big-wm/migrations/dock-audio-2026-11-{x11,wayland}`).

- **O wmix dá lugar ao wmpulsemixer onde estava.**
  - A posição que o usuário escolheu fica.
  - `AutoLaunch` e `Lock` ficam como estavam.
  - No Wayland também o nome nas listas do `Dockapps.plist`, inclusive em `AfterLaunchers`.
- **O wmusic entra logo abaixo do volume.**
  - Na mesma coluna, os ladrilhos de baixo descem um lugar até o primeiro vago: a ordem do usuário fica e nada se sobrepõe.
  - O Window Maker grava uma lista por altura de tela (`Applications1440`…) além de `Applications`: cada uma muda sozinha.
  - O limite de altura de cada lista é o número dela; o de `Applications` vem do `xwininfo`.
- **Escolhas do usuário respeitadas.**
  - Quem tirou o volume (sem wmix no Dock; no Wayland, `wmix` em `Removed`) não ganha o wmpulsemixer nem o wmusic. No Wayland os dois vão para `Removed`, senão o `big-wm-dockapps` os iniciaria pelo padrão.
  - Quem já tem o wmpulsemixer ou o wmusic (Dock, Clip, gaveta), ou tirou um deles, fica como está.
  - Uma lista `~/.config/big-wm/dockapps` escrita à mão não é tocada.
- **Sem lugar na tela:** o volume é trocado, o wmusic fica de fora e a marca não é gravada. Tenta no próximo login.
- **`State.plist` do usuário.** A lista padrão anterior (com o wmix) vira a nova. Uma lista mudada recebe o volume e a música no lugar do wmix. Se o arquivo foi regravado (sem comentários), a edição usa o leitor de property list.

Exemplo, a conta real desta máquina (dry run em cópias):

- **X11**, `Applications` e `Applications1440`:
  - antes: …, wmcube 9, **wmix 10**, wmake-zones 11, bandeja 12;
  - depois: …, wmcube 9, **wmpulsemixer 10, wmusic 11**, wmake-zones 12, bandeja 13.
- **Wayland**, lista `Dock` do `Dockapps.plist`:
  - antes: `big-wm-tray, wmix, wmclock, …`;
  - depois: `big-wm-tray, wmpulsemixer, wmusic, wmclock, …`.

## Disposição recomendada

Menu **Aparência/Configurações > Dockapps > "Restaurar volume e música no Dock"** (`big-wm-dock-audio --restore`):

- pede a disposição recomendada para o próximo login das duas sessões, com uma notificação dizendo isso;
- o Window Maker regrava o `WMState` e o wlmaker o `Dockapps.plist` ao sair, então a mudança com a sessão aberta se perderia;
- no login, o volume e a música vão para logo abaixo da bandeja, o wmix sai, duplicatas no Clip saem e os ladrilhos de baixo seguem na ordem deles.

## Catálogo do menu Dockapps

- **Entradas novas:**
  - "Volume e microfone (wmpulsemixer)" no grupo Som e música, nas duas sessões (no Wayland, pelo XWayland);
  - "Controle de reprodução de música (wmusic)", que já estava.
- **wmix:** continua no catálogo.
- **Iniciados pelo menu:** acoplam como os outros (no X11, o `big-wm-dockapps-autolaunch` liga o `AutoLaunch` na primeira vez).

## Mudanças no wlmaker (branch `fix/dockapp-menu-overlay`, pacote 30)

Achadas nos testes na tela da sessão Wayland:

1. **O menu de dispositivos do wmpulsemixer não aparecia.**
   - O GTK põe o menu como transiente da janela que recebeu o clique, a janela de ícone. No wlmaker ela é desenhada num ladrilho, não é uma janela; o menu, filho dela, ficava invisível.
   - Agora uma janela override-redirect cujo pai é um dockapp vai para a camada de sobreposição, onde se pôs, como uma sem pai (`src/xwl_surface.c`).
2. **Preto em volta do wmusic no login.**
   - A forma (X Shape) recorta o nó da cena do conteúdo. Quando o ladrilho muda de lugar no login, o conteúdo ganha um nó novo, e a forma só voltava no próximo commit.
   - O wmusic sem player não redesenha, e ficava preto em volta até tocar algo.
   - Agora a destruição do nó recortado agenda a forma de novo pelo laço ocioso (`src/icon_manager.c`).

## Limitação

- **Teclado no menu do wmpulsemixer no Wayland:** a navegação pelas setas não funciona; o mouse, o Esc e o clique fora funcionam. No X11 funciona.
- O wlmaker dá o foco do teclado ao menu, mas o GTK no XWayland não recebe as teclas. Suspeita: a pegada (grab) de teclado do GTK através do XWayland. Fica para outra rodada.
