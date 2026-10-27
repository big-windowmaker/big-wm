# Arquitetura e decisões

## Visão geral

Um único executável, `big-wm-tabbed-dock`, processo cliente separado nas duas
sessões. Se ele cair, o Window Maker/wlmaker e a sessão continuam; o systemd
--user o reinicia (drop-in `Restart=on-failure`).

```
tabbed-dock/
  src/core/      só GLib/GIO, sem toolkit — testado por unidade
    btd-model      abas e itens (ids fixos, rótulos, ordem, mover/copiar)
    btd-config     preferências + persistência (GKeyFile, gravação atômica,
                   .bak, recuperação de arquivo corrompido)
    btd-desktop    índice de .desktop (XDG + exports do Flatpak), TryExec,
                   OnlyShowIn/NotShowIn, associação janela → aplicativo
    btd-defaults   abas padrão só com aplicativos instalados
    btd-coexist    escolha Dock clássico / com abas / ambos / nenhum
    btd-launch     lançamento sem shell (GDesktopAppInfo), AppImage validado,
                   escopo systemd por aplicativo
  src/backend/   rastreamento de janelas (interface + 3 backends)
    btd-x11        EWMH/ICCCM no root (Window Maker)
    btd-wayland    wlr-foreign-toplevel-management ou ext-foreign-toplevel-list;
                   nomes de saída por xdg-output
  src/ui/        GTK 3 + cairo
    btd-dock       janela, layout, desenho, entrada, posição (X11/layer-shell)
    btd-draw       abas NeXT, relevos, ladrilhos (implementação própria)
    btd-icons      cache LRU limitado de ícones já rasterizados
    btd-dock-dnd   arrasto dentro do dock + XDND/wl_data_device do GTK
    btd-dock-menu  menus de contexto
    btd-prefs      preferências (estilo WPrefs: quadros e caixas simples)
    btd-app        GtkApplication, linha de comando remota, sinais
```

## Por que C + GTK 3 (medido)

Mesmas condições (contêiner, sessão X11 Xvfb+Window Maker e Wayland wlmaker
headless), dock C contra um equivalente mínimo em Python + GTK 3
(`tabbed-dock/tests/fixtures/python-baseline.py`: janela dock/layer-shell,
área cairo, 6 ícones do tema):

| | C + GTK 3 (dock completo) | Python + GTK 3 (mínimo) |
|---|---|---|
| Primeiro quadro, X11 (3 execuções, cache quente) | 24,3 / 25,4 ms (85,5 ms a frio) | 140,7 / 145,2 / 147,9 ms |
| Primeiro quadro, Wayland | 27,8 / 27,9 ms (83,7 ms a frio) | 137,4 / 142,4 / 147,1 ms |
| PSS X11 / Wayland | 25,5 / 26,4 MB | 43,4 / 44,0 MB |
| USS (memória só dele) X11 / Wayland | 18,0 / 17,1 MB | 37,1 / 36,2 MB |
| CPU ociosa (60 s) | 0 ticks | — |

O PSS do dock cai na sessão real porque as bibliotecas GTK são divididas com
os outros aplicativos GTK 3 (no contêiner ele é praticamente o único). A
bandeja em Python do projeto (`big-wm-tray`) mede ~33 MB na VM.

**Por que não GTK 4**: no Wayland o dock precisa de layer-shell; o
gtk4-layer-shell existe, mas no X11 o GTK 4 não tem tipo de janela dock,
struts nem acesso a Xlib de forma suportada — seriam dois programas. GTK 3 dá
X11 e Wayland nativos com um código só, mais XDND/wl_data_device, temas de
ícones e gettext. **Por que não Python**: a tabela acima.

## Decisões

- **Dock desenhado por inteiro em cairo** (um `GtkDrawingArea`), não widgets
  GTK: o visual NeXT não depende do tema GTK do usuário, e uma única superfície
  é mais leve e permite a forma da janela exata (abas inclinadas).
- **Forma da janela**: sem compositor no X11, a máscara vira a forma da janela
  (XShape) — os cantos entre as abas não aparecem pretos; com compositor ou no
  Wayland, janela RGBA + região de entrada (cliques passam pela parte vazia).
- **Lançamento**: `GDesktopAppInfo` (códigos de campo do Exec, Terminal, Path,
  TryExec, DBusActivatable, Actions); argv direto, nunca `sh -c`. Documentos e
  pastas com o aplicativo padrão (`g_file_query_default_handler`, o mesmo do
  `gio open`). AppImage só se for arquivo regular, executável, do usuário ou do
  root e não gravável por outros. Cada processo lançado vai para
  `app-big\x2dwm\x2dtabbed\x2ddock-<app>-<pid>.scope` (systemd --user,
  `StartTransientUnit`), e GLib faz o fork duplo: parar ou reiniciar o dock
  nunca fecha aplicativos (o drop-in ainda põe `KillMode=process`).
- **Associação janela → aplicativo** (núcleo, testado): app_id/
  `_GTK_APPLICATION_ID` → `GIO_LAUNCHED_DESKTOP_FILE` (só se
  `GIO_LAUNCHED_DESKTOP_FILE_PID` é o próprio processo, para não herdar de
  um terminal) → `StartupWMClass` → nome do .desktop → id Flatpak → programa
  do Exec/`/proc/<pid>/exe`; programas ambíguos (sh, python, env, flatpak…)
  nunca identificam aplicativo.
- **Persistência**: `~/.config/big-tabbed-dock/dock.conf` (GKeyFile), gravado
  0,5 s depois da última mudança, de forma atômica (`g_file_set_contents_full`,
  0600); o arquivo bom anterior vira `dock.conf.bak`. Arquivo ilegível é
  guardado como `dock.conf.broken-AAAAMMDD-HHMMSS` (nunca apagado) e o `.bak`
  assume; valores inválidos voltam ao padrão um a um, itens inválidos são
  ignorados sem perder o resto.
- **Ícones**: PNG pelo cairo e SVG pelo librsvg, direto. O gdk-pixbuf 2.44 do
  Manjaro abre todo formato num processo glycin isolado (`bwrap` +
  `glycin-svg`/`glycin-image-rs`); medido, isso somava processos e memória à
  sessão para cada dock iniciado. Cache LRU de 128 superfícies, limpo quando
  o tema de ícones muda.
- **Sem polling**: X11 por `PropertyNotify`; Wayland pelos eventos do
  protocolo na fila do GDK; aplicativos instalados/removidos por
  `GAppInfoMonitor` (inotify), com reconstrução do índice 2 s depois da última
  mudança. Animações (troca de aba, lançamento) usam o relógio de quadros só
  enquanto duram: CPU ociosa medida em 0.
- **Uma instância**: `GtkApplication`; os comandos de controle (`--tab`,
  `--quit`, `--reload`, `--print-state`) nunca viram a instância principal
  (havia uma corrida com o registro no D-Bus durante a partida, achada nos
  testes).
- **Coexistência sem destruir nada**: X11 por `wdwrite WindowMaker
  DisableDock YES|NO` (ferramenta do próprio Window Maker, só essa chave;
  WMState intacto); Wayland por override `Hidden=true` do autostart do
  `wlmdock` em `~/.config/autostart`, marcado `X-Big-Tabbed-Dock-Managed`; um
  override do próprio usuário é renomeado para `.user-backup`, nunca apagado.

## Segurança

Sem privilégios, sem setuid, sem rede. Entrada não confiável (dock.conf,
arquivos importados, .desktop de terceiros, nomes de janela) é validada:
tamanho máximo, UTF-8, ids `[a-z0-9-]`, ids de .desktop sem `/` nem `..`,
caminhos absolutos, ícones por nome de tema `[A-Za-z0-9._+-]` ou caminho
absoluto sem `/../`. Importar valida num modelo descartável antes de aplicar.
Arrastar de fora sempre responde COPY/LINK, nunca MOVE (o gerenciador de
arquivos não apaga o original). `tests/run.sh` recusa `system()`, `popen()`,
`execl()` e `"sh", "-c"` no código.
