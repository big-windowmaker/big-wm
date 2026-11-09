# Ampliação da tela (acessibilidade) — Ctrl+Alt+rolagem

Missão de novembro de 2026. Ampliação global da tela nas duas sessões do Big
WindowMaker, como a lupa de tela inteira do GNOME e do macOS.

| Atalho | Efeito |
| --- | --- |
| Ctrl+Alt+rolagem para cima | amplia (×1,25 por entalhe, até ×8) |
| Ctrl+Alt+rolagem para baixo | reduz |
| Ctrl+Alt+0 | volta ao normal |

Sem os modificadores a rolagem continua indo para os programas. A ampliação
vale para tudo o que está na tela (janelas, menus, painéis, Dock, a tela de
bloqueio do Wayland), em todos os monitores, desliza até o novo nível
(120 ms) e segue o ponteiro.

## Problema

Não havia ampliação de tela utilizável:

- Wayland: o wlmaker só tinha `OutputMagnify`/`OutputReduce`, que mudam a
  escala da saída (o tamanho lógico da tela muda, as janelas se rearranjam —
  é mudar a resolução, não ampliar).
- X11: só o `wmagnify` (Lupa), uma janela separada que mostra uma cópia
  ampliada de um pedaço da tela (capturas repetidas, sem entrada).

## Causa / restrições de cada sessão

- Wayland: quem desenha a saída é o compositor (wlmaker + wlroots 0.20,
  `wlr_scene`). A ampliação tem de estar ali: só ele vê o quadro inteiro e
  controla o cursor.
- X11: o Xorg não redireciona entrada para conteúdo ampliado. Uma janela de
  lupa por cima precisaria capturar a tela embaixo dela a cada quadro (e
  com o picom compondo, nem existe o que capturar fora do compositor).

## Arquivos

wlmaker (branch `feat/zoom-a11y-2026-11`):

- `src/backend/zoom.c`, `include/backend/zoom.h` — a ampliação: estado,
  configuração (`Zoom` no Config.plist), contas (área vista, animação,
  rolagem) e o quadro ampliado; testes unitários (`backend_test`, conjunto
  `zoom`).
- `src/backend/output.c` — o handler `frame` da saída passa o quadro à
  ampliação quando há zoom.
- `src/backend/backend.c` — cria a ampliação (`wlmbe_backend_zoom`).
- `src/action.c`, `src/action.h` — ações `ZoomIn`, `ZoomOut`, `ZoomReset` e
  atalhos de roda nos KeyBindings (`WheelUp`, `WheelDown`, `WheelLeft`,
  `WheelRight`); testes `wheel_bindings` e `parse`.
- `src/input/manager.[ch]`, `src/input/cursor.c` — filtro de eventos de eixo
  (roda) antes dos clientes, com os modificadores do teclado.
- `src/server.c` — liga o filtro aos atalhos, passa o cursor à ampliação e
  relê `Zoom` no ReloadConfiguration.
- `etc/Config.plist` — padrões do wlmaker.

big-wm (branch `feat/zoom-a11y-2026-11`):

- `session/zoom/big-wm-picom-zoom.c` — plugin do picom (sessão X11).
- `session/zoom/picom/{api,backend,types}.h` — cabeçalhos da API de
  plugins do picom 13 (MPL-2.0; o pacote do picom não instala o `types.h`).
- `session/zoom/big-wm-picom` — o picom com o plugin, quando instalado.
- `session/zoom/app-picom@autostart.service` — drop-in: o picom do autostart
  passa a ser o `big-wm-picom`.
- `session/zoom/zoom.conf` — configuração padrão do X11.
- `data/wlmaker/Config.plist.in` — atalhos e o dicionário `Zoom`.
- `session/bin/big-wm-firstrun` — os três atalhos entram num Config.plist
  existente (só se o wlmaker instalado conhece a ampliação).
- `Makefile`, `pkgbuild/PKGBUILD` — `make zoom`/`install-zoom`, pacote
  `big-wm-zoom`.
- `tests/run.sh`, `tests/zoom-probe.c` — testes (abaixo).

## Solução no Wayland (wlmaker)

Pipeline de um quadro com ampliação (`wlmbe_zoom_output_frame`):

1. `wlr_scene_output_build_state` com uma **swapchain própria**
   (`wlr_scene_output_state_options.swapchain`) do tamanho e formato da
   saída: a cena é desenhada como sempre, sem ampliação e com o controle de
   dano dela (o anel de dano reconhece os buffers dessa swapchain).
2. Uma passada de render (`wlr_renderer_begin_buffer_pass`) no buffer da
   swapchain da saída: um único quad com a textura do quadro da cena,
   `src_box` = área vista (em coordenadas de buffer, respeitando transformação
   e escala da saída), `dst_box` = saída inteira, filtro bilinear (ou
   `Smooth = False`: nearest).
3. Commit com dano total. Com sincronização explícita (timeline), a passada
   sinaliza uma timeline própria e o commit espera por ela.

Decisões:

- **Ponteiro:** enquanto há zoom, `wlr_output_lock_software_cursors`: o
  cursor é desenhado dentro do quadro da cena e ampliado com ele. Assim ele
  aparece sempre sobre o conteúdo que aponta, em qualquer modo de
  acompanhamento, e as coordenadas de entrada (as do layout) não precisam
  de tradução: o cliente recebe a posição real.
- **Acompanhamento** (`Tracking`): `Proportional` (padrão) — origem =
  ponteiro × (1 − 1/nível): o ponto sob o ponteiro não sai do lugar ao
  ampliar e o ponteiro alcança as bordas junto com a área vista; `Push` — a
  área só anda quando o ponteiro encosta na borda; `Centered`.
- **Scanout direto** desligado só nas saídas ampliadas
  (`wlr_output_lock_attach_render`), senão um cliente em tela cheia
  passaria por cima da ampliação.
- **Animação:** exponencial no logaritmo do nível (cada entalhe parece
  igual), ~95% em `AnimationTime`, encosta no alvo a 0,5%. Quadros só
  enquanto anima, o ponteiro anda ou a cena muda: **CPU em repouso zero**.
- **Vários monitores:** o nível é global; cada saída guarda a sua área vista
  (segue o ponteiro enquanto ele está nela).
- **Saída do zoom:** ao voltar a 1, destrava cursor e scanout, libera a
  swapchain própria e danifica a saída inteira (os buffers dela têm quadros
  ampliados, que o anel de dano da cena não conhece).
- **Atalhos de roda:** pseudo-teclas nos KeyBindings; o filtro do input
  manager roda antes dos clientes e só fica com a combinação ligada (Caps
  Lock e Num Lock não contam). Rodas de alta resolução e touchpads dão
  frações de entalhe (amplia suave); outras ações ligadas à roda rodam uma
  vez por entalhe. Ctrl+Alt+0 não conflita com nenhum atalho do big-wm
  (Alt+0 é a área de trabalho 10; Logo+Ctrl+0 é do wmake-zones).

Configuração (opcional, `~/.config/wlmaker/Config.plist`):

```
KeyBindings = {
    "Ctrl+Alt+WheelUp" = ZoomIn;
    "Ctrl+Alt+WheelDown" = ZoomOut;
    "Ctrl+Alt+0" = ZoomReset;
};
Zoom = {
    MaxLevel = 8.0;        // ampliação máxima
    Step = 1.25;           // fator por entalhe (1 < Step <= 4)
    AnimationTime = 120;   // ms; 0 = sem animação
    Tracking = Proportional; // Proportional | Push | Centered
    Smooth = True;         // filtro bilinear; False = blocos nítidos
};
```

Desligar o atalho = tirar as linhas dos KeyBindings. `ZoomIn`/`ZoomOut`
também podem ir para teclas (ex.: `"Logo+Alt+equal" = ZoomIn;`).

## Solução no X11 (plugin do picom)

Abordagens avaliadas:

| Abordagem | Resultado |
| --- | --- |
| Janela de lupa em tela cheia (wmagnify, xzoom) | Captura a tela a cada quadro (e a si mesma); o X11 não passa a entrada adiante para o conteúdo ampliado. **Descartada.** |
| XRandR `--transform` (escala 1/nível + translação) com panning | Amplia na varredura sem captura e a entrada fica certa, mas: cada movimento é um `RRSetCrtcConfig` (modeset por movimento do mouse, bloqueando o servidor ~1 vblank); o panning do servidor limita a origem pela largura do modo e não funciona com escala; e o pior — o CRTC transformado passa a medir largura/nível: o servidor manda `RRScreenChangeNotify` e o Window Maker, o GTK e o Qt veem o monitor "encolher" a cada passo (o WM rearranja cabeças, Dock e janelas maximizadas). Não testável no Xvfb (sem transformações). **Descartada.** |
| picom 13 nativo | Sem zoom (`picom --help`, v13). |
| **Plugin do picom 13** | **Escolhida.** Amplia no compositor, com a GPU/XRender dele, sem capturas, sem mudar a geometria dos monitores, entrada coerente pelo mapeamento proporcional. |

Como funciona (`big-wm-picom-zoom.so`, carregado com `picom --plugins`):

- O picom 13 desenha cada quadro numa imagem própria (inteira e com o
  controle de dano dele) e copia a parte danificada para o back buffer com
  `copy_area_quantize`. O plugin troca essa cópia, enquanto há zoom, por
  duas operações do backend do próprio picom em cada monitor (RandR 1.5):
  `blit` da área vista para uma imagem do tamanho do monitor e `blit`
  ampliado (`scale`) dela para o back buffer, recortado ao monitor. As
  coordenadas ficam pequenas (o XRender usa 16 bits) e a imagem do picom não
  é alterada.
- **Entrada:** área vista proporcional ao ponteiro, como no Wayland: o que
  está sob o ponteiro aparece exatamente onde o cursor do servidor está, e
  os cliques chegam ao lugar certo sem tradução.
- **Atalhos:** grabs passivos na raiz de Button4/Button5 e da tecla 0 só com
  Control+Mod1 (também com Lock e Mod2): a rolagem sem os modificadores não
  é tocada. O Window Maker não suporta botões da roda em atalhos; o grab
  passivo na raiz tem prioridade sobre os grabs dele nas janelas.
- **Quadros:** o picom redesenha a cada evento X que recebe; o plugin pede
  um quadro trocando a propriedade `_BIG_WM_ZOOM` da raiz (um pedido por
  quadro). Enquanto há zoom assina `XI_RawMotion` (XInput2) para acompanhar o
  ponteiro e lê a posição no próprio quadro (sem atraso). Sem zoom, nada é
  assinado: **o plugin não acorda com o mouse**.
- **Ao voltar a 1:** os próximos quadros copiam a tela inteira (os back
  buffers tinham quadros ampliados).
- **Monitores:** um monitor que contém outro (o "screen" de um Xvfb dividido,
  divisões virtuais) e cópias exatas (espelhamento) são ignorados.
- **Robustez:** `picom_api_get_interfaces` é símbolo fraco (picom sem a API
  de plugins: o plugin não faz nada); a versão da API é conferida pelo
  picom (outra major: o picom recusa o plugin e segue sem ampliação); o
  `big-wm-picom` só passa `--plugins` se o arquivo existe (o picom aborta
  com plugin ausente).

Configuração (`~/.config/big-wm/zoom.conf`, padrão em
`/usr/share/big-wm/zoom.conf`, lida quando o picom começa):

```
enabled=true
max-level=8
step=1.25
animation-ms=120
```

## Testes feitos

### Unitários

- wlmaker `ctest` (29/29): conjunto `zoom` do `backend_test` (7 casos:
  invariante proporcional — o ponto sob o ponteiro é o mesmo em 5 níveis ×
  5 pontos —, centrado, push, limites com entradas absurdas, rolagem e
  limites, animação chega exata em ≤16 quadros, configuração/rejeição) e
  `action` (`parse` com WheelUp/WheelDown/Ctrl+Alt+0; `wheel_bindings`:
  com e sem modificadores, Shift a mais, Num/Caps Lock, direção não ligada,
  eixo horizontal, fim de rolagem de touchpad, quantidades de roda comum,
  alta resolução e touchpad).
- big-wm `tests/run.sh`: contas e configuração do plugin
  (`-DZOOM_SELFTEST`), compilação do plugin com `-Werror`, shellcheck do
  `big-wm-picom`, e um teste de verdade com Xvfb + picom + xdotool
  (`tests/zoom-probe.c` cobre a tela com quadrados cuja cor diz a posição e
  lê pixels da tela): rolagem sem modificadores não amplia; Ctrl+Alt+roda ×4
  mantém sob o ponteiro o mesmo pixel e 200 px à direita mostra o conteúdo
  de x≈482 (= 400 + 200/1,25⁴); parado, nenhum quadro é pedido; o ponteiro
  anda e a área o segue; Ctrl+Alt+0 volta ao quadro original.

### Laboratório Wayland (wlmaker aninhado, `mklab.sh zoom`)

Capturas em `~/.cache/big-wm-lab-zoom/shots/wayland/`:

- `00-antes.png` → `01-zoom-calc.png`: Ctrl+Alt+roda ×4 sobre o "5" da
  Calculadora (GTK): ampliada ×2,44 com o ponteiro (ampliado) sobre o "5".
- `02-zoom-click.png`, `03-reset.png`: clique ampliado digitou "5" (entrada
  coerente), Ctrl+Alt+0 voltou ao normal.
- `04-zoom-kwrite.png`: KWrite (Qt) ampliado ×3,8.
- `06-07.png`: com zoom, a roda sem modificadores rolou o KWrite (linhas
  2→17) e o nível não mudou.
- `20-22.png`: dois monitores (`WLR_X11_OUTPUTS=2`): cada saída com sua
  área, a do ponteiro acompanha.
- `30-33.png`: renderizador **gles2** (backend headless, render node da GPU,
  capturas com grim/screencopy, entrada por ponteiro e teclado virtuais):
  amplia, rola sem modificadores, Ctrl+Alt+0.
- `34-36.png`: saída **girada 90° e escala 1,5** (gles2): ampliado sobre o
  "9", o clique digitou "9".
- `40-41.png`: com a tela de bloqueio (gtklock) ampliada; Ctrl+Alt+0 com
  Num Lock ligado.

CPU do wlmaker (ticks de 10 ms em 10 s):

| Situação | pixman (X11 aninhado) | gles2 (headless) |
| --- | --- | --- |
| repouso sem zoom | 1 | 0 |
| repouso com zoom | 2 | 0 |
| ponteiro andando 20 Hz, com zoom | 25 (2,5%) | 5 em 150 movimentos |
| ponteiro andando 20 Hz, sem zoom | 4 | 2 em 150 movimentos |

### Laboratório X11 (Xvfb, sessão do big-wm, picom 13 xrender + plugin)

Capturas em `~/.cache/big-wm-lab-zoom/shots/x11/` (o cursor do X não sai
nas capturas; a posição do ponteiro é a dita):

- `00-x11-antes.png` → `01-x11-zoom.png`: ponteiro em (571,522) sobre o "5":
  ampliado ×2,44, o "5" continua em (571,522).
- `02-03.png`: cliques ampliados digitaram "59"; Ctrl+Alt+0 voltou ao normal
  sem restos ampliados.
- `05-06.png`: KWrite (Qt) ampliado; a roda sem modificadores rolou o texto
  (4→19) mantendo o zoom.
- `07-08.png`: dois monitores (RandR `--setmonitor`): cada um com a sua área.

CPU do picom (ticks de 10 ms em 10 s; o Dock com dockapps redesenha sempre):
repouso sem zoom 8, repouso com zoom 7; ponteiro a 20 Hz: 19 com zoom, 10
sem.

## Resultado

- Wayland: ampliação suave sobre tudo, cursor ampliado e coerente, vários
  monitores, transformações e escalas, sem custo em repouso.
- X11: ampliação no compositor sobre tudo o que ele compõe, entrada coerente,
  vários monitores, sem custo em repouso.

## Limitações

- X11: precisa do picom compondo a tela (o padrão da sessão). Com
  `unredir-if-possible`, uma janela em tela cheia sem nada por cima é
  mostrada sem compositor — e sem ampliação — até algo aparecer por cima.
- X11: o cursor do servidor não é ampliado (fica no tamanho normal, no
  lugar certo); `Push`/`Centered` não existem no X11, porque o cursor não
  pode ser desenhado fora da posição real.
- X11, backend xrender: a ampliação é sem filtro (pixels em blocos); nos
  backends glx/egl o picom amplia com filtro linear.
- X11: o plugin é da API de plugins do picom 13; outra major exige revisão.
- Wayland: o cursor volta a ser de hardware só na próxima troca de imagem
  do cursor depois do Ctrl+Alt+0 (comportamento do wlroots).
- Wayland: a captura de tela (screencopy) recebe o quadro ampliado, como é
  mostrado.
- Laboratório: com duas saídas X11 aninhadas, o wlmaker mapeia o ponteiro de
  cada janela do host para o layout inteiro (não para a saída daquela
  janela); é do laboratório aninhado (nada a ver com a ampliação), por isso
  o cursor aparece noutro lugar que o do host nas capturas de dois monitores.
- Não testado em hardware real (não se mexe nos monitores do usuário) nem
  com Vulkan; o caminho de timeline explícita do Wayland foi escrito para o
  DRM com gles2/Vulkan mas o laboratório (headless) não usa timeline.

## Próximos passos

- Testar na sessão real (DRM, várias telas) e na VM.
- X11: cursor ampliado (XFixes: imagem do cursor desenhada ampliada pelo
  plugin e cursor real escondido).
- Itens de menu "Ampliação da tela" no WMRootMenu/RootMenu do wlmaker, e um
  controle no big-wm-welcome/WPrefs para nível máximo e passo.
- Atalhos de teclado opcionais para ampliar sem roda (ex.: Logo+Alt+=/−),
  hoje só configuráveis à mão no Wayland.
