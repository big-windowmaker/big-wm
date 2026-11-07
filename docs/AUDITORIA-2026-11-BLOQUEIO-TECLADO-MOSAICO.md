# Auditoria — bloqueio, teclado na tela, mosaico GTK, dockapps e login da ISO (novembro de 2026)

Continuação de [AUDITORIA-2026-11-RODADA-2.md](AUDITORIA-2026-11-RODADA-2.md).
Cada item diz a causa encontrada, o que mudou, onde e como foi verificado.
"Verificado" quer dizer executado e observado (saída, captura, medição); o
que não foi executado está em **Pendências**, sem estimativa no lugar do
resultado.

Ambiente: a máquina do mantenedor (Manjaro, X11) e, para o que não pode
rodar na sessão real, Xvfb e Xephyr aninhados e wlmaker aninhado (backend
X11, pixman). A ISO conferida é a `big-windowmaker_TESTING_2026-11-06_k618`
(r35), extraída com 7z (`livefs.sfs`, `desktopfs.sfs`).

## Ramos (só locais, sem push)

| Repositório | Ramo | O que tem |
|---|---|---|
| big-wm | `fix/missao-lock-kbd-2026-11` | bloqueio sem o aviso, teclado na tela do X11 e preferências, ícone do dock com abas, dockapps padrão, testes, esta auditoria |
| wmake-zones | `fix/gtk-mosaico` | troca de lugar, `windows -d`, compatibilidade de aplicativos nas preferências; ladrilho arrasta para o Dock/Clip e abre o menu do Window Maker |
| wlmaker | `big-wm/missao-2026-11` | o mosaico confere o tamanho mínimo do cliente (patch 0121) |
| packages | `fix/missao-2026-11` | wlmaker 19 (0121), wmake-zones 0.3.1-3 (0002, 0003) e windowmaker 13 (0018) |
| wmaker | `big-wm/missao-2026-11` | arrastar e menu do Window Maker passados por um dockapp (patch 0018) |
| iso-profiles | `fix/login-live-2026-11` | login da sessão live com o visual do Big WindowMaker |

## 1. "INCOMPATIBLE COMPOSITOR, PLEASE FIX!" ao desbloquear (X11)

**Causa.** O xsecurelock cria, além das janelas na janela de sobreposição
do Composite (onde fica a tela de bloqueio), uma janela comum quase do
tamanho da tela, o *obscurer*, branca com o texto do aviso. Ela só deveria
aparecer se o compositor esconder a sobreposição. Com o picom ela fica no
quadro composto, embaixo da tela de bloqueio. Ao sair, o xsecurelock destrói
tudo de uma vez (`main.c` 1.9.0: o obscurer primeiro, depois a
sobreposição e as outras janelas, sem esperar o compositor): a tela de
bloqueio some na hora e o último quadro do picom, ainda com o obscurer,
fica à mostra até a próxima repintura. Com `fading = true` o picom ainda o
esmaece por ~250 ms. O `unredir-if-possible` não é a causa: o aviso
continua sem ele.

Reproduzido no Xvfb com o picom e o `picom.conf` padrão, gravando a tela a
cada ~20 ms: 29 de 30 desbloqueios mostraram o aviso.

**Alternativas medidas.**

| Variante | Aviso ao desbloquear | Sessão à mostra enquanto bloqueado |
|---|---|---|
| `picom.conf` da Alpha | sim (29/30) | não |
| `fading = false` | 1 quadro | não |
| `unredir-if-possible = false` | sim | não |
| `XSECURELOCK_COMPOSITE_OBSCURER=0` | não | **sim**: com uma janela de tela cheia por cima (o picom deixa de compor), a sessão apareceu |
| sem picom | não | não |
| **regra do obscurer (adotada)** | **não (0/30)** | **não** |

**Correção.** Regra no `data/picom.conf`:
`class_g = 'xsecurelock' && class_i = 'obscurer'` com `opacity = 0`,
`fade = false`, `shadow = false`. O obscurer fica invisível só na
composição do picom (onde nunca serve: a tela de bloqueio está acima). Quando
o picom deixa de compor e esconde a sobreposição, quem desenha o obscurer
é o X, e ele continua cobrindo a sessão. O `XSECURELOCK_COMPOSITE_OBSCURER=0`
foi descartado por isso e o `XSECURELOCK_NO_COMPOSITE` não foi usado.
Contas existentes ganham a regra uma vez (`big-wm-firstrun`,
`migrate_picom_obscurer`, com cópia de segurança; quem a tirar fica sem ela),
só quando o `picom.conf` da conta tem a linha exata `rules = (`: sem ela a
marca da migração é criada do mesmo jeito e a regra precisa ser posta à mão
(uma conta com o `picom.conf` mudado pode não recebê-la).

**Verificado.** `tests/integration/lock-compositor.sh` (no `make check`, 3
ciclos; `BIG_WM_LOCK_CYCLES=30` para a rodada completa). Precisa de Xvfb,
picom, xsecurelock (com o `saver_blank`) e xdotool; sem eles sai com 77, e o
`tests/run.sh` conta como pulado (o log some). Os 3 ciclos padrão não têm o
de ataque (tela cheia pedindo para o picom não compor, a cada 5 ciclos):
um `make check` verde não prova que ele rodou; use
`BIG_WM_LOCK_CYCLES=5` ou mais. No de ataque, o ciclo falha se a janela de
ataque não rodou ou se o obscurer não cobriu a sessão. xsecurelock +
`auth_big_wm` + authproto falso, picom com o `picom.conf` do repositório,
janela de "sessão" vermelha, gravação da tela inteira. Falha se a sessão
aparecer enquanto bloqueado ou o aviso branco depois da senha certa.

- 30 ciclos com a correção: 0 avisos, 0 exposições, saída 0 em todos,
  senha errada (uma ou duas) não desbloqueia, 6 ciclos com janela de tela
  cheia pedindo `_NET_WM_BYPASS_COMPOSITOR` por cima do bloqueio (o obscurer
  aparece, como deve, e a sessão não).
- O mesmo teste com o `picom.conf` da Alpha falha (3/3).
- 10 ciclos sem picom: 0 avisos, 0 exposições.

**Não coberto aqui.** DPMS (o Xvfb deste ambiente não tem a extensão: os
ciclos "dpms" não desligaram nada), suspensão real e dois monitores reais.
Ver Pendências.

## 2. Teclado na tela

**Wayland.** O módulo do gtklock funciona na instalação: com os binários
instalados (`/usr/bin/big-wm-lock`, `/usr/lib/gtklock/big-wm-keyboard.so`)
num wlmaker aninhado, o ⌨ do campo abre o teclado e as teclas digitam no
campo. O que faltava era o X11 (o bloqueio da sessão X11 não tinha teclado
nenhum) e as preferências.

**X11 (novo).** O `auth_big_wm` desenha o teclado (OPENSTEP, ABNT2 com
teclas mortas e US, Shift, Caps, apagar, Enter), um por monitor, na escala
do Xft.dpi. Como chega o clique: o xsecurelock prende o ponteiro e só
repassa teclas pela entrada padrão; o módulo pede ao servidor os eventos
*raw* do XInput2 na raiz (`XI_RawButtonPress/Release`, `XI_RawMotion`;
nunca teclas), que chegam mesmo com o ponteiro preso por outro cliente, e
lê a posição com `XQueryPointer`. Cada tecla da tela entra no mesmo buffer
travado (`mlock`) das teclas físicas, pelo mesmo caminho (`auth_byte`); a
tecla morta pendente é apagada com o resto ao sair. Sem xdotool, sem
injeção de eventos, sem outro processo. O ponteiro do xsecurelock é
transparente durante a senha (medido com `XFixesGetCursorImage`: 1×1, sem
pixel visível), então o módulo desenha uma seta. Cancelar e Desbloquear
passam a funcionar com mouse e toque. Com uma tela de toque (classe
`XITouchClass` direta) e `KeyboardTouch`, o teclado já abre.

Dois defeitos achados no caminho: o "Aguarde..." ficava no diálogo depois
do pedido de senha; e o laço do auth podia esperar até 1 s com eventos já
lidos pelo Xlib (o `poll` não acorda para a fila do Xlib).

**Preferências.** `big-wm-lock-settings` ganhou a seção *Teclado na tela*
(`VirtualKeyboard`, `VirtualKeyboardButton`, `KeyboardTouch`,
`KeyboardLayout`, `KeyboardSize`, `KeyboardTheme`) e foi para duas colunas
(cabe em 768 linhas). O módulo do Wayland lê as mesmas chaves; o tema
escuro e o tamanho grande também valem lá, e o diálogo sobe quando o
teclado abre.

**Verificado** (Xvfb + xsecurelock + authproto falso):

- `tests/integration/lock-keyboard.sh` (no `make check`): ⌨ abre; senha
  errada pela tela não desbloqueia; "certa" pela tela e Enter da tela
  desbloqueia (saída 0); Cancelar fecha o diálogo e não desbloqueia;
  `VirtualKeyboard=false` tira o botão. O mesmo teste com o `auth_big_wm`
  instalado (Alpha) falha.
- ABNT2: "Ação" com ⇧, a tecla morta ~ e ç, depois de uma senha errada,
  desbloqueia; parte pelo teclado físico e parte pela tela também.
- 192 dpi: diálogo e teclado em 2×, desbloqueio pela tela.
- Tema escuro e tamanho grande: capturas conferidas, nos dois lados.

**Não coberto.** Dois monitores reais (Xvfb e Xephyr deste ambiente não
expõem dois monitores RandR; o desenho por monitor é o mesmo laço que já
desenhava um diálogo por monitor) e toque real (sem tela de toque aqui).

## 3. Ícone do dock com abas no Clip

**Causa.** O PNG vem no big-wm-themes e está na ISO, mas o
`WMWindowAttributes` padrão tinha a entrada do dock com abas só com
`DontSaveSession` (desde 14a43d3), e o `big-wm-clip-add` pulava o ícone
quando a entrada existia; numa conta da ISO de novembro ele ainda saía antes
por achar o nome na lista de já adicionados. Resultado: o cubo genérico.

**Correção.** O padrão traz `Icon` e `AlwaysUserIcon`; o clip-add põe o
ícone numa entrada sem `Icon` antes de qualquer retorno antecipado, sem
mexer num ícone escolhido pelo usuário. Teste em `tests/run.sh`.

## 4. Mosaico com aplicativos GTK (Pamac, BigLinux Settings, Calculadora)

**Causa** (medida no Xvfb com Window Maker, picom e a configuração do Big
WindowMaker, e num wlmaker aninhado): aplicativos GTK4/libadwaita declaram
mínimos grandes.

| Aplicativo | Mínimo (cliente) | Onde não cabe |
|---|---|---|
| Pamac | 760×200 | qualquer coluna dividida em 1366 de largura |
| GNOME Calculadora | 360×616 | células empilhadas em 768 de altura |
| BigLinux Settings | 360×480 | três células empilhadas |

No X11, quem não cabia ficava flutuante sem explicação (e a linha do log
se repetia a cada rodada). No Wayland o wlmaker nem olhava o mínimo: as
janelas entravam em células menores e passavam por cima das vizinhas e da
saída (Pamac com 810 de largura numa célula de 402).

**Correção.**

- wmake-zones (X11): uma única janela que não cabe troca de lugar com outra
  quando assim as duas cabem (o Pamac vai para a principal em 1366×768); com
  mais de uma fora, sai primeiro a mais longe de caber, sem mexer na
  principal. Log uma vez por mudança, com a célula e o mínimo.
- wlmaker (patch 0121): a janela guarda o mínimo do cliente (`xdg_toplevel`
  e, no XWayland, `WM_NORMAL_HINTS`); o mosaico faz o mesmo que o X11.
- `wmake-zones windows -d` (X11 e Wayland): por que cada janela está ou não
  no mosaico ("não cabe: mínimo 760x200, célula 402x343", diálogo, tamanho
  fixo, regra, escolha do usuário...). O `windows -l` não mudou.
- Preferências > Regras: *Compatibilidade de aplicativos* (a lista acima e
  *Sempre flutuante este aplicativo*); o diagnóstico (Avançado) mostra a
  lista. As páginas rolam: a janela pedia 883 linhas e não cabia em 768.

**Verificado.** wmake-zones: `meson test` 5/5 (x11-functional 56/56 com a
troca, o `windows -d` e o log; x11-multimonitor 13/13 no Xephyr; dockapp
13/13); suíte Wayland 51/51 com o wlmaker do ramo (o teste novo pula num
wlmaker antigo e falha no da Alpha). wlmaker: testes do toolkit 30/30,
wlmaker 25/25. Os três aplicativos de verdade: 1366×768 (Pamac + xterm:
ambos no mosaico; os três: dois no mosaico e a Calculadora flutuante, com o
motivo), 1920×1080 (os três no mosaico), Wayland 1024×768 (Pamac flutuante
com o motivo, os outros no mosaico sem sobrepor).

## 5. Dockapps padrão: wmcliphist, wmshutdown, wmeyes

O firstrun põe os três no Dock do X11, quando instalados, uma vez cada
(`dock_add_once`, a mesma função do ladrilho do wmake-zones): sem cópia de
quem já está no Dock ou no Clip, sem voltar depois de tirado (marca em
`~/.config/big-wm`), fora quando a coluna passaria da altura da tela (sem
marca: entra num login com espaço). O nome é o WM_CLASS que o Window Maker
casa com o programa; com o nome errado (`wmshutdown.DockApp`) o wmshutdown
aparecia ao lado do Dock e o ladrilho ficava com o cubo.

- wmcliphist grava no banco do big-wm-clipboard (`setup_wmcliphist`): um
  histórico só, o mesmo do Super+V.
- wmshutdown pergunta antes (*Shutdown confirmation*: Halt / Reboot /
  Cancel), conferido com um clique no Xvfb.
- Wayland (atualizado em 07/11, wlmaker 20): com o XWayland, o
  `big-wm-dockapps` inicia o wmcliphist e o wmshutdown (os de GTK com
  `GDK_BACKEND=x11`) no Dock, como no X11; os olhos são o
  `wlmeyes --dockapp` (desenhado como o wmeyes, seguindo o ponteiro pela
  tela). Sem o XWayland (ou sem o programa) eles não entram: histórico
  pelo Super+V, desligar pelo menu do wlmaker. Ver
  `docs/wayland-dockapps.md`.

**Verificado.** wmaker num Xvfb com o WMState migrado: os três nos
ladrilhos 8, 9 e 10, sem ícone ao lado. `tests/run.sh`: uma vez cada,
`wmeyes -w`, nenhuma posição repetida, WMState válido, o tirado não volta,
quem está no Clip não duplica, tela de 600 linhas só com o primeiro.

## 6. Login da ISO com cara de GNOME

**Gerenciador de login.** Sessão live: LightDM (`displaymanager="lightdm"`
no `profile.conf`), entrada automática na sessão `livecd`
(`startbiglive` → assistente com o visual do Window Maker → `startbigwm`).
O greeter aparece ao sair da sessão live. Sistema instalado: wdm (o
Calamares habilita o wdm primeiro, `services.conf`).

**Causa.** Na ISO r35, `/etc/lightdm/slick-greeter.conf` aponta para
`/usr/share/backgrounds/community/lightdm-default.jpg` (que esta edição não
tem) e o nosso está em `slick-greeter.conf.bak`, com data de geração da
imagem: o manjaro-tools troca o fundo no desktopfs. Sem fundo e sem tema, o
slick-greeter vem preto, no tema escuro padrão do GTK. O `lightdm.conf` da
sessão live também não dizia qual greeter usar.

**Correção** (iso-profiles): o `live-overlay` (livefs, por cima do
desktopfs no boot) traz o `slick-greeter.conf` do Big WindowMaker (papel de
parede da edição, tema OneStepBack, logo PNG de 64) e o `lightdm.conf` da
sessão live diz o greeter e pré-escolhe a sessão `big-wm`. Autologin e nomes
das sessões (X11 e Wayland) iguais. Testes em
`tests/test_wmaker_login_live.py`; prévia conferida com o
`slick-greeter --test-mode` num Xvfb, com o arquivo montado num namespace
próprio.

## 7. Ladrilho do wmake-zones: arrastar para o Dock/Clip e o menu do Window Maker

**Causa.** O ladrilho (64×64) cobre o ícone inteiro e pede os cliques. O
servidor X só entrega ButtonPress a um cliente por janela, então o Window
Maker não recebia nada: o ladrilho não arrastava (só com Alt), e o clique
direito abria só o menu do wmake-zones, sem as opções do Dock ou do Clip.
Medido no Xvfb: com o binário da Alpha, um ladrilho solto arrastado até a
coluna do Dock fica onde estava.

**Correção.**

- wmake-zones: o clique esquerdo vale ao soltar; se o ponteiro anda mais de
  4 pontos antes, o clique vai para o Window Maker como evento sintético na
  janela do ícone (a forma que ele prevê para dockapps), depois de soltar a
  captura implícita. A borda de 3 pontos funciona como nos outros
  dockapps (arrastar move, clique direito abre o menu do Window Maker). O
  menu do ladrilho ganhou "Opções do Window Maker...", que abre o menu
  dele para aquele lugar (Dock: Posição do Dock, Monitor, Add a drawer,
  Executar, Trazer aqui, Esconder, Configurações, Terminar; Clip: o do
  Clip; solto: o do ícone).
- windowmaker (patch 0018): três falhas na passagem sintética. O arrastar
  dependia da captura implícita de um clique real (sem ela, o ícone parava
  de seguir o ponteiro fora dele): agora o Window Maker pega o ponteiro. O
  menu esperava a soltura do mesmo botão, que nunca vinha quando o botão
  já estava solto (clique rápido, ou o item do menu do ladrilho): agora o
  próximo clique fecha ou escolhe. E o ponteiro capturado para o evento
  sintético nunca era devolvido (os cliques seguintes iam para o Window
  Maker): agora volta no fim do menu.

Regra do próprio Window Maker, mantida: um aplicativo rodando, acoplado,
só sai do Dock com Alt; sem Alt, o arrastar o leva para outro Dock, o Clip
ou um gaveteiro, ou muda o lugar dele no mesmo Dock.

**Verificado** (Xvfb, Window Maker de verdade): Dock → Clip e de volta;
ladrilho solto → Dock; clique direito na borda no Dock (menu do Dock) e no
Clip (menu do Clip); "Opções do Window Maker..." abre o menu do Dock e, com
a 0018, um clique fora o fecha e o menu do ladrilho volta a abrir (sem a
0018 o menu fica aberto). `tests/x11-dockapp.sh` 17/17 (4 novas: menu pela
borda, fechar soltando fora, arrastar o ladrilho solto para o Dock, clique
sem arrastar troca o modo); com o binário da Alpha, as do menu e do
arrastar falham.

## Pendências

- **Publicação.** Nada foi enviado: push, PRs, CI dos pacotes e a ISO nova
  esperam autorização. A ISO r35 tem os quatro defeitos (conferido nos
  arquivos dela: `picom.conf` sem a regra, dock com abas sem `Icon`,
  `auth_big_wm` sem XInput2, Dock sem os três dockapps).
- **wmake-zones 0.3.1-2** leva a correção como patch 0002 sobre a tag
  v0.3.1; uma tag nova (v0.3.2) precisa de push.
- **DPMS, suspensão real, dois monitores reais e toque real** no bloqueio:
  não houve como testar neste ambiente; pedem a VM ou a máquina real.
- **Instalador e entrada automática:** o módulo `displaymanager` do
  Calamares só conhece sddm, gdm e lightdm; com o wdm (o padrão do sistema
  instalado), marcar "entrar automaticamente" não tem efeito, e o wdm não
  tem entrada automática. Precisa de decisão: trocar para o LightDM quando
  a entrada automática for marcada, ou tirar a opção nesta edição.
- **slick-greeter no sistema instalado:** o mesmo fundo da BigCommunity
  fica no desktopfs; só importa para quem trocar o wdm pelo LightDM.
- **wmshutdown** está em inglês (*Shutdown confirmation*), do upstream.
