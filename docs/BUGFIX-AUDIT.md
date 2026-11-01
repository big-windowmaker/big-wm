# Auditoria de correções — novembro de 2026

Registro de engenharia da rodada de correções de 01/11/2026: causa raiz,
correção, evidência e o que ficou pendente, para cada problema relatado.
Não é necessário em tempo de execução.

Repositórios e branches:

| Repositório | Branch | O que mudou |
|---|---|---|
| `big-windowmaker/big-wm` | `fix/desktop-consistency-2026-11` | sessão, dock com abas, padrões, testes |
| `big-windowmaker/wmaker` | `big-wm/fixes-2026-11` | AppIcons, monitor do Dock, painel Info (exportados como patches) |
| `big-windowmaker/packages` | `fix/desktop-consistency-2026-11` | `windowmaker` 6, `big-dockapps` 5 (patches do wmnet e do wmcliphist) |

`iso-profiles` e `build-iso` não precisaram de mudança: a lista de pacotes
da ISO já tem `wmcliphist`, `wmnet`, `thunar` e `ashyterm`, e as correções
chegam pelos pacotes.

## 1. Super+V abre o histórico da área de transferência

**Causa.** O wmcliphist tinha atalho próprio (Ctrl+Alt+V), mas: o parser de
teclas só aceitava Ctrl, Alt e Shift (e escrevia um byte além do buffer), e
o menu aberto pelo atalho nunca aparecia — `gtk_menu_popup()` sem evento de
origem não mostra nada no GTK 3.24 (`no trigger event for menu popup`). Um
grab global de tecla de um cliente XWayland também não funcionaria no
Wayland.

**Correção.** O wmcliphist abre o mesmo menu do clique no dockapp ao receber
`SIGUSR1`, no ponteiro (`gtk_menu_popup_at_rect` na raiz), tentando de novo
por até 1 s enquanto o gerenciador de janelas ainda segura o teclado. O
atalho é do gerenciador de janelas — `Mod4+v` no menu raiz do Window Maker,
`Logo+V` no wlmaker — e chama `big-wm-open clipboard`. Um wmcliphist antigo
ainda em execução depois da atualização (binário `(deleted)`) não recebe o
sinal: ele morreria; a sessão avisa para entrar de novo.

**Evidência.** No Xvfb: menu aberto pelo sinal e pelo Super+V, fechado pelo
Escape. Migração testada: o menu anterior ganha a linha e fica igual ao
padrão novo; um menu que já usa Mod4+v fica como está.

## 2. Kate abria pastas no lugar do Thunar

**Causa.** `/etc/xdg/mimeapps.list` (pacote `biglinux-mime`) define
`inode/directory=org.kde.dolphin.desktop`. Sem o Dolphin instalado, o XDG
escolhe o primeiro aplicativo registrado para pastas — o Kate
(`MimeType=text/plain;inode/directory;`).

**Correção.** `/etc/xdg/windowmaker-mimeapps.list` e
`/etc/xdg/wlmaker-mimeapps.list` (lidos só nessas sessões, antes do
`mimeapps.list` do sistema e depois do `~/.config/mimeapps.list` do usuário)
põem o Thunar como padrão; a escolha do usuário continua valendo.
`big-wm-open files` (Super+E, Clip, menus) só usa o padrão se ele tiver
`Categories=FileManager`; Kate, VSCodium ou Catfish como "padrão" caem no
Thunar.

**Evidência.** Teste com Kate, VSCodium, Catfish, Dolphin e Nautilus
instalados e o `mimeapps.list` da BigLinux: `gio` e `xdg-mime` respondem
Thunar nas duas sessões; sem os arquivos novos, Kate (o bug). Escolha do
usuário (Nautilus) respeitada.

## 3. Dock (WMDock) num monitor escolhido

**Causa.** O Dock só ia para a borda esquerda ou direita da tela inteira
(ou do monitor principal com `KeepDockOnPrimaryHead`); arrastar só trocava
entre as duas bordas externas — com 3 monitores, sempre no da ponta.

**Correção (wmaker, patch 0006).** O Dock tem um monitor, gravado como
`Dock.Monitor` no WMState: `primary`, o nome do conector RandR
(`HDMI-A-0`, estável quando a ordem muda) ou, sem RandR, o número do
monitor. Escolha pelo submenu "Monitor" do menu do Dock (gerado ao abrir,
com os monitores conectados) ou arrastando o Dock para outro monitor (vai
para o lado mais próximo do ponteiro). Monitor desconectado: o Dock vai para
o principal sem esquecer a escolha e volta na reconexão (`wRandR_Update`).
O primeiro ícone fica dentro da altura do monitor (monitores de alturas
diferentes). As gavetas acompanham.

**Evidência.** Xephyr com duas cabeças Xinerama: arrasto para o monitor 1,
escolha pelo menu, persistência após reiniciar, monitor ausente (`HDMI-9`)
caindo no principal e listado como "(desconectado)".

**Pendente.** Validação física com 3 monitores, hotplug real e RandR com
várias saídas (o Xephyr expõe uma saída RandR só). "Superior/inferior" não
existe: o Dock do Window Maker é vertical.

## 4. wmnet sem gráfico

**Causa.** Sem `-W`, o driver `devstats` olhava `eth0`, que não existe com
nomes previsíveis (`enp7s0`, `wlp2s0`). Também: a primeira amostra somava o
contador inteiro (pico falso), contador zerado virava um número enorme, e a
procura do `:` em `/proc/net/dev` podia passar do fim do nome.

**Correção (patch `wmnet-default-route.patch`).** Sem `-W`, a interface da
rota padrão (`/proc/net/route`, menor métrica), ou a primeira interface
ativa; reavaliada a cada 3 s ou quando some (cabo, Wi-Fi, VPN que assume a
rota). Uma interface só: tráfego de VPN não é somado duas vezes. Leitura a
10 Hz em vez de 40 Hz. `-W` continua escolhendo à mão.

**Evidência.** No Xvfb, com download real: o wmnet novo detecta `enp7s0` e
desenha o tráfego; o atual registra `monitoring eth0`. CPU das duas versões
≈0,1% (30 e 40 ms em 30 s; diferença na resolução do relógio).

## 5. Dock com abas: largura total

**Causa.** Reproduzida no Xephyr com o Window Maker (Centro → Largura
total): a janela X ficava na largura nova e o GTK continuava anunciando e
alocando a antiga (`WM_NORMAL_HINTS` 544 × janela 1600; na máquina do
usuário, 1544 × 3440). Redimensionar a janela mapeada faz o GTK esperar o
`ConfigureNotify` com o relógio de quadros congelado; sob o Window Maker
esse evento não chega ao GTK para a janela do dock (nem um sintético), e
ele fica parado até outro evento — "aplicar de novo" às vezes destravava.

**Correção.** No X11 a janela tem tamanho fixo no monitor (largura do
monitor × altura do maior dock possível com as fontes atuais) e a área de
desenho fica dentro dela (margens); o shape deixa visível e clicável só o
dock. Alinhamento, largura total, ícones, rótulos e aplicativos abrindo só
mudam a área, dentro do GTK. Monitor diferente: a janela é recriada. O
strut continua com a altura e a faixa do dock.

**Evidência.** Os cenários (dock antes do Window Maker, centro → total,
alternância rápida, ícones 48 → 72 → 32) renderizam certo; memória igual
(PSS ~15,6 MB nas duas versões).

**Pendente.** Modos "um dock por monitor", "espelhado" e "aplicativos do
próprio monitor" (seção 8 do pedido): exigem separar a vista (janela,
slots, monitor) do estado compartilhado (modelo, rastreador de janelas,
cache de ícones) no `btd-dock.c`, para várias vistas num processo só. A
escolha de um monitor específico já existe (`Monitor=` no `dock.conf`).

## 6. Painel Info

Créditos "Big WindowMaker Custom" — Rafael Ruscher e Barnabé Di Kartola —
no fim, depois de um divisor entalhado (`WRGroove`, o relevo dos grupos do
WPrefs). O símbolo de copyleft é desenhado (anel e "C" invertido, com
antialiasing), porque o glifo U+1F12F da fonte de reserva saía pequeno e
acinzentado. Os copyrights do Window Maker continuam acima (GPL, seção 2c).

## 7. Papel de parede serrilhado

**Causa.** O menu "Papéis de parede" chamava `wmsetbg -u -s`: sem `-S` a
escala é pelo vizinho mais próximo e o `wmsetbg` grava
`SmoothWorkspaceBack = NO` no perfil; `-s` estica (deforma imagens de outra
proporção). Além disso a imagem relatada tem 1938×811 — num monitor
3440×1440 ela é ampliada 1,77×, e nenhuma interpolação recria detalhe.

**Correção.** `big-wm-wallpaper` aplica com `wmsetbg -u -S` (Catmull-Rom do
wrlib, uma vez por aplicação) e o modo guardado: `fill` (padrão), `fit`,
`center` (tamanho original), `stretch`, `tile`, `span` (todos os monitores;
no Wayland vira `fill`). Submenu "Ajuste do papel de parede". Migração:
`SmoothWorkspaceBack` NO → YES uma vez.

**Evidência.** Capturas a 1920×1080, 2560×1080, 3440×1440 e 3840×2160:
bordas em degrau somem; em 16:9 a imagem 2,39:1 não é mais deformada.

**Pendente.** A imagem "Androide Cromada — Synthwave (Ultrawide)" não foi
empacotada: falta confirmar o direito de redistribuição, e 1938×811 é
pequena para ultrawide (o ideal é ≥ 3440×1440). Ela já está no menu do
usuário (`~/GNUstep/Library/WindowMaker/Backgrounds`). Papel de parede
diferente por monitor: o `wmsetbg` usa uma imagem para todos os monitores.

## 8. Abrir terminal aqui (Thunar)

**Causa.** No exo 4.20, sem o `xfce4-mime-helper` (pacote xfce4-settings,
ausente), o `exo-open --launch TerminalEmulator` só lê
`~/.config/xfce4/helpers.rc` e depois procura o `xfce4-terminal` — "Could
not find fallback TerminalEmulator application".

**Correção.** `big-wm-terminal` abre o terminal preferido
(`xdg-terminal-exec`; AshyTerm por padrão; reserva entre os instalados) na
pasta, sem shell no meio; o AshyTerm, de instância única, recebe
`--working-directory`. O firstrun grava `TerminalEmulator=big-wm-terminal`
no `helpers.rc` de quem não escolheu outro; um helper em
`/usr/share/xfce4/helpers` serve ao `xfce4-mime-helper` quando ele existir.
`big-wm-open terminal` (Ctrl+Alt+T, Dock, menus) usa o mesmo wrapper.

**Evidência.** AshyTerm real, com uma instância já aberta, abre o shell
novo na pasta `pasta teste $x 'q'`; argumentos com espaço, aspas e `$`
passam intactos.

## 9. AppIcons com um espaço de um ícone entre eles

**Causa (wmaker, patch 0005).** `PlaceIcon()` marcava os slots ocupados por
coordenada absoluta (`x / tamanho`), com arredondamento para cima, mas
colocava os ícones numa grade que começa no canto do monitor. No monitor do
meio (x = 3440, que não é múltiplo de 64) cada ícone marcava também o slot
seguinte. `wArrangeIcons()` não usa esse mapa — por isso reiniciar
"consertava".

**Evidência.** Xephyr com um painel à esquerda de 48 px (mesmo
deslocamento): antes x = 112, 240, 368, 496; depois 48, 112, 176, 240. A
coluna do Dock agora sai só do monitor onde o Dock está.

## 10. Ícone do Firefox (Netscape)

**Causa.** O Window Maker escolhe o ícone do aplicativo e reconhece o
lançador do Clip pelo líder do grupo de janelas, `firefox.Firefox`; o ícone
foi gravado em `Navigator.firefox` (a janela do navegador). Resultado: ícone
original no rodapé, num segundo ícone fora do Clip. O dock com abas usava
sempre o ícone do `.desktop`.

**Correção.** Padrões e migração gravam o ícone escolhido também em
`firefox.Firefox` e o lançador do Clip passa a `firefox.Firefox`. O dock com
abas lê o `WMWindowAttributes` (`btd-wmattrs`, só GLib, com teste): ícone
com `AlwaysUserIcon = YES` vale também no dock, pela classe das janelas ou,
fechado, pelo `StartupWMClass` do `.desktop` — só nomes inteiros. Prioridade:
ícone escolhido no próprio dock, forçado no Window Maker, tema/`.desktop`.
Mudanças no arquivo valem na hora (inotify).

## Matriz X11 / Wayland

| Funcionalidade | X11 | Wayland (wlmaker) |
|---|---|---|
| Super+V | atalho do Window Maker → SIGUSR1 | `Logo+V` → SIGUSR1; o wmcliphist roda no XWayland e só vê a área de transferência sincronizada pelo XWayland |
| Super+E / pastas no Thunar | sim | sim (`wlmaker-mimeapps.list`) |
| Abrir terminal aqui | sim | sim (mesmo wrapper) |
| Monitor do Dock | sim (patch do wmaker) | não implementado nesta rodada (Dock do wlmaker) |
| wmnet | sim | sim (dockapp pelo XWayland) |
| Dock com abas, largura total | corrigido (janela fixa + margens) | já usava layer-shell ancorado; sem mudança |
| Ícone forçado no dock com abas | sim | sim (pelo `.desktop`; o Wayland não expõe WM_CLASS) |
| Papel de parede suave e modos | sim (`wmsetbg -S`) | swaybg (`span` vira `fill`) |
| AppIcons | corrigido | não se aplica |

## Testes executados

- `big-wm/tests/run.sh` com shellcheck 0.11 e meson: todas as verificações
  passaram (inclui os 6 testes unitários do dock, um deles novo).
  Duas falhas antigas eram do ambiente e foram corrigidas no teste: o
  `LANGUAGE` da sessão traduzia o teste do índice de `.desktop`, e o
  `big-wm-wprefs-sync` instalado reescrevia o Config.plist do teste do tema.
- `wmaker`: `make check` do upstream passou; `makepkg` do `windowmaker`
  (com `check()`) e do `big-dockapps` concluídos.
- Testes visuais em Xephyr/Xvfb descritos em cada seção.

Não executados: validação física em 3 monitores, hotplug real, sessão
Wayland real, instalação dos pacotes (exige `sudo`), ISO.
