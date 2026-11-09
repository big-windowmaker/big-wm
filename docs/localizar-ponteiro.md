# Localizar o ponteiro ao sacudir o mouse

Sacudir o mouse aumenta o **próprio** ponteiro por um instante: cresce
suavemente, fica grande enquanto a sacudida continua e volta aos poucos ao
tamanho normal. Nas duas sessões, com as mesmas preferências.

## Problema

- **Wayland:** aparecia um *segundo* ponteiro ampliado, que não acompanhava o
  verdadeiro. O `big-wm-locate-pointer` (X11) rodava pelo XWayland (o menu
  "Localizar o ponteiro ao sacudir" o iniciava com `--toggle`): ele só via o
  movimento sobre janelas X11, não conseguia trocar o cursor do compositor e
  caía no caminho da janela por cima (override-redirect), posicionada com
  `XQueryPointer` a cada 16 ms — um desenho à parte, fora de sincronia.
- **X11, cursores próprios dos programas:** o daemon trocava, *pelo nome*, a
  imagem dos cursores pela imagem grande do *tema dele*. Um programa com
  outro tema de cursor (ou com imagem própria sob um nome do tema) via, na
  animação, a imagem errada — e a troca por nome mudava também o cursor de
  todos os outros programas, que no fim ficavam com a imagem do tema do
  daemon. A janela usada para cursores sem nome só andava a cada quadro e
  continuava com a imagem antiga quando o cursor mudava por baixo dela.

## Solução

### Wayland: no compositor (wlmaker)

- `src/input/cursor.c`: todo pedido de imagem do cursor (toolkit, protocolo
  cursor-shape, superfícies de cursor dos clientes, inclusive o XWayland)
  passa pelo cursor do wlmaker, que sabe o que está na tela e pode:
  - **ampliar** a imagem atual: para imagens do tema, o mesmo cursor
    carregado sob demanda no tamanho grande (`wlr_xcursor_theme_load` no
    tamanho × ampliação máxima × maior escala das saídas, guardado até o
    tema mudar); para superfícies de clientes, a textura da superfície
    (`wlr_texture_read_pixels`). Mostra com `wlr_cursor_set_buffer` e uma
    escala fracionária a cada quadro: o ponto ativo fica no lugar, o
    ponteiro segue o mouse como sempre (é o próprio cursor), e o wlroots
    usa o plano de cursor de hardware quando cabe e o cursor de software
    quando não (também durante o zoom de acessibilidade, que já desenha o
    cursor por software: os dois convivem, ver
    `evidencias/wl-ponteiro-com-zoom.png`);
  - **substituir** a imagem (usado pela caveira, ver `cursor-caveira.md`).
  Pedidos novos durante a animação (outra janela, outro cursor) são
  ampliados também; no fim, a imagem pedida volta exatamente como estava.
- `src/locate_pointer.c`: o detector (o mesmo do X11: inversões de direção
  de trechos longos e rápidos, proporcionais à altura da saída sob o
  ponteiro, em pixels lógicos), a animação (temporizador de 16 ms só
  enquanto o ponteiro está ampliado) e as preferências. Sem polling: o
  detector roda no evento de movimento do cursor; não roda com o cursor
  escondido (jogos, vídeos) nem com a janela ativa em tela cheia.
- Ação nova `LocatePointer` (mostra uma vez, para um atalho).

### Preferências (as mesmas nas duas sessões)

`~/.config/big-wm/locate-pointer.conf` (o padrão em
`/usr/share/big-wm/locate-pointer.conf`):

| chave | valores | padrão |
|---|---|---|
| `enabled` | true/false | true |
| `sensitivity` | low/medium/high (5, 4 ou 3 idas e voltas) | medium |
| `max-scale` | 1.5 a 8 (intensidade) | 3 |
| `grow-ms` | tempo para crescer | 160 |
| `duration-ms` | do início até começar a voltar (duração) | 700 |
| `return-ms` | tempo para voltar (retorno) | 260 |

O wlmaker lê o arquivo indicado em `Config.plist`:

```
LocatePointer = {
    SettingsFile = "~/.config/big-wm/locate-pointer.conf";
};
```

e o vigia com inotify: o menu "Localizar o ponteiro ao sacudir"
(`big-wm-locate-pointer --toggle`) grava o arquivo e o wlmaker liga ou
desliga na hora, sem SIGHUP. Sem `SettingsFile`, valem as chaves do próprio
bloco (`Enabled`, `Sensitivity = Low|Medium|High`, `MaxScale`, `GrowMsec`,
`DurationMsec`, `ReturnMsec`; desligado por padrão).

No Wayland, o `big-wm-locate-pointer` só grava a configuração e avisa; o
daemon não roda pelo XWayland, e um daemon antigo daquele display é
encerrado no `--toggle`. `--show` no Wayland avisa que é a ação
`LocatePointer` do wlmaker.

### X11 (`session/locate-pointer/big-wm-locate-pointer.c`)

- A troca por nome no servidor só acontece quando o tema do daemon tem, com
  aquele nome, **a mesma imagem que está na tela** (tamanho, ponto ativo e
  pixels); senão o programa fica com a janela por cima, com a imagem dele, e
  os cursores dos outros programas não mudam. A devolução confere os pixels
  (antes, só o tamanho).
- A janela segue cada movimento (`XI_RawMotion`), não só os quadros, e troca
  de imagem quando o cursor muda por baixo dela (`XFixesCursorNotify`).
- Vários monitores: o tamanho do gesto segue a altura do monitor sob o
  ponteiro (já era assim); a janela e o cursor do servidor andam por todos.
- Chaves novas `grow-ms`, `duration-ms`, `return-ms`.

## Arquivos

- wlmaker: `include/toolkit/input.h`, `src/toolkit/input.c`,
  `src/input/cursor.[ch]`, `src/input/manager.[ch]`,
  `src/locate_pointer.[ch]`, `src/server.[ch]`, `src/action.[ch]`,
  `tests/wlmaker_test.c`.
- big-wm: `session/locate-pointer/*`, `data/wlmaker/Config.plist.in`,
  `session/bin/big-wm-firstrun` (migração), `welcome/` (o botão também no
  Wayland), `tests/run.sh`.

## Testes

- wlmaker: `ctest` 29/29 (conjunto novo `locate_pointer`: detector com os
  mesmos casos do X11, leitura das preferências, curva da animação).
- big-wm: `tests/run.sh`, bloco do localizar o ponteiro (detector,
  liga/desliga, Xvfb: cursor do servidor, SIGTERM, SIGKILL, e agora o modo
  Wayland: o daemon sai, `--disable` grava, `--show` avisa).
- Laboratório Wayland (wlmaker aninhado, cursor de software para as
  capturas; também com cursor de hardware, sem erros), capturas em
  `~/.cache/bwm-wt/cursor/evidencias/`:
  - `wl-ponteiro-padrao-acompanha.png`: normal → ampliado → ampliado depois
    de um movimento rápido (+40,+30 px; acompanha, ponto ativo na ponta) →
    normal, sem rastros nem segundo ponteiro;
  - `wl-ponteiro-xwayland-xterm.png`: o cursor de texto do xterm (superfície
    do XWayland);
  - `wl-ponteiro-cliente-wayland-proprio.png`: cursor próprio de um app GTK
    Wayland (imagem, não do tema), ampliado em volta do ponto ativo;
  - `wl-ponteiro-com-zoom.png`: com o zoom de acessibilidade ligado;
  - `wl-ponteiro-saida-escala2.png` e `wl-ponteiro-troca-de-saida.png`: duas
    saídas (escala 1 e 2), nítido na escala 2 (imagem carregada a 144 px) e
    com o mesmo tamanho lógico ao passar de uma saída para a outra.
- Laboratório X11 (Xvfb com dois monitores): `x11-janela-cursor-proprio-acompanha.png`,
  `x11-janela-troca-de-cursor.png`, `x11-outro-tema-imagem-do-programa.png`
  (xterm com Bibata enquanto o daemon usa Adwaita: a imagem do programa, e
  os outros xterm continuam com a deles).
- CPU: wlmaker parado, 0 ticks em 10 s; 5 s de movimento contínuo (virtual
  pointer), 4 ticks ligado e 5 desligado (diferença dentro do ruído).

## Limitações e próximos passos

- Superfícies de cursor de clientes são ampliadas a partir da textura (o
  renderizador interpola): menos nítidas que as do tema, que são carregadas
  grandes.
- Durante a animação, cursores animados (relógio de espera) ficam no
  primeiro quadro.
- O laboratório aninhado (backend x11) não mostra o plano de cursor de
  hardware nas capturas; a validação visual com cursor de hardware real
  (DRM) fica para a máquina do usuário (`tests/tools/nested-gpu.sh` não
  cobre o plano de cursor).

Capturas no repositório (sem o papel de parede do laboratório):
`docs/screenshots/ponteiro-caveira/`. As demais ficaram na máquina de
desenvolvimento, em `~/.cache/bwm-wt/cursor/evidencias/`.
