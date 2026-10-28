# Decisões de desempenho

Registro das decisões da missão de memória: hipótese, medição, mudança,
nova medição e resultado. A linha de base está em
[performance-benchmarks.md](performance-benchmarks.md). Toda mudança passa
pelo mesmo harness (`tests/perf/`), com a mesma imagem e VM, e é comparada
pela mediana dos boots aos 5 e 15 min.

## Oportunidades (Etapa A)

Levantadas na linha de base de 2026-10-28: imagem dourada, big-wm main
`f65e26d`, 5 boots por sessão. Na Etapa A nada foi implementado (Gate
A); o que veio depois está nas seções de cada etapa (A2, A3 e A11: [Etapa
D](#etapa-d-sessão-wayland-wlmaker-e-residentes)). Ganho = estimativa a partir do PSS/USS medido do processo ou da função,
a validar com o harness. A ordem combina ganho, risco e quanto depende só do
Big WindowMaker.

| # | Oportunidade | Ganho estimado | Risco | Impacto visual/funcional | Onde | Como validar |
|---|---|---|---|---|---|---|
| A1 | **Reserva do kernel em MemTotal − MemAvailable** (~465 MiB numa VM de 4 GiB; ~220 MiB na VM de 8 GiB pós-"Mínima"). Origem medida: `/etc/sysctl.d/91-memory.conf` do pacote **auto-rules-io** 26.02.23-2022 (sobrepõe o arquivo de mesmo nome do biglinux-systemd-swap) põe `vm.watermark_scale_factor = 150` (padrão do kernel: 10) e `watermark_boost_factor = 15000`. Com `vm.min_free_kbytes = 120078` (nenhum arquivo do sysctl.d define esse valor; origem a confirmar), as marcas d'água somam min 117 / low 176 / high 235 MiB, e o MemAvailable desconta a reserva das zonas (252 MiB) + a marca low (176 MiB); a fórmula do kernel reproduz o MemAvailable medido ao 0,1 MiB. Na VM pós-"Mínima" (sem o auto-rules-io) o fator é 10 | métrica 1: ~90 MiB a mais de MemAvailable voltando o `watermark_scale_factor` para 10 (low 176→~146, high 235→~176 MiB), e até ~100 MiB a mais se o `min_free_kbytes` vier de um ajuste evitável; nenhum processo muda | médio: a reserva existe para evitar travadas sob pressão; mexer nela é decisão de sistema, não de desktop | nenhum | sysctl da BigLinux (pacote que a "Mínima" remove) — fora do big-wm; levar à BigLinux ou ao perfil `bigcommunity/wmaker` com justificativa | harness (RAM usada) + teste de pressão (stress-ng/abrir muitos apps) olhando PSI e travamentos, antes/depois |
| A2 | **wlmaker com 108,6 MiB de PSS (USS 105,8)**, o maior processo do Wayland (no X11, Xorg + wmaker = 82). Hipótese: buffers do renderizador por software em 1920x1080 (cada quadro ARGB = 7,9 MiB), texturas do fundo/ladrilhos e cópias por saída | 30–60 MiB | médio: código do compositor (fork wlmaker) | nenhum, se mantiver as texturas | wlmaker (Etapa D) | `smaps` do wlmaker por mapeamento (heap, memfd, dmabuf) antes de mudar; harness Wayland; testar 1366x768 para ver a escala com a resolução |
| A3 | **`big-wm-wprefs-sync --watch` residente (Python, 16,2 MiB)** só para vigiar `~/GNUstep/Defaults` por inotify | ~16 MiB na sessão Wayland | baixo | nenhum (o efeito continua: mudança no WPrefs chega ao wlmaker) | big-wm: trocar o vigia residente por uma unidade `.path` do systemd --user que roda o conversor uma vez por mudança | `tests/wprefs` + mudança no WPrefs chegando ao wlmaker em < 1 s; harness Wayland |
| A4 | **Samba ativo sem compartilhamento** (smbd, nmbd, smbd-notifyd, smbd-cleanupd: 23,6 MiB) | ~24 MiB (sistema, as duas sessões) | médio: o compartilhamento de pastas da BigLinux precisa voltar a ligar o serviço quando o usuário compartilhar | nenhum até o usuário compartilhar uma pasta | perfil `bigcommunity/wmaker` (habilitação dos serviços) ou BigLinux | `systemctl is-enabled smb nmb`; compartilhar pasta pelo gerenciador de arquivos liga o serviço; harness |
| A5 | **Redução de ruído do microfone sempre carregada** (`biglinux-microphone-*`: 3 processos, 15,6 MiB, mais os filtros no PipeWire) mesmo sem microfone | ~15 MiB | médio: é recurso da BigLinux; ligar sob demanda (microfone presente/ativado) | nenhum se subir quando houver microfone | BigLinux (`biglinux-noise-reduction-pipewire`) | `pactl list sources`; gravação com e sem o filtro; harness |
| A6 | **GTK 3 mais caro no X11 que no Wayland**: nm-applet 27,6 contra 16,0 MiB, polkit-gnome 15,8 contra 7,1, Dock com abas 24,9 contra 14,4, mais o snixembed (21,7). Hipóteses: carregadores de imagem (glycin/gdk-pixbuf) e temas de ícones carregados por processo no X11, `LD_PRELOAD` do gtk-nocsd, bibliotecas X11/Cairo-xlib | 15–30 MiB somando os quatro | baixo a médio | nenhum (mesma aparência) | big-wm (ambiente da sessão X11), gtk-nocsd | `smaps` por mapeamento do nm-applet nas duas sessões; testar sem `LD_PRELOAD` e com `GDK_PIXBUF`/glycin em cache; harness X11 |
| A7 | **Pico do pamac no login**: `pamac-session` 16–104 MiB + `pamac-broker` (root) 61 MiB por ~1–2 min, logo depois do login | transiente: −70 a −160 MiB no pico do login (não muda o repouso) | baixo | atualizações continuam avisadas; só mais tarde | pamac-tray (atraso do primeiro check) ou config do pamac no perfil | harness com amostras a 30/60/90/120 s; notificação de atualização ainda aparece |
| A8 | **CPU/wakeups em repouso no X11**: Xorg ~81 e picom ~75 trocas de contexto/s (2,3 ms/s cada) puxados pelos dockapps (wmix e wmnet ~10/s, wmcpuload ~7/s); total X11 ~467/s contra ~110/s no Wayland | energia/CPU, não RAM | baixo | dockapps atualizando com menos frequência (ex.: wmix 10 Hz → evento ou 1–2 Hz) | dockapps (big-dockapps), picom.conf (Etapa C) | janela de CPU do harness; `ctxt` por processo |
| A9 | **Login "pronto" só depois de ~10 s**: a área de trabalho aparece em ~1 s, mas a fila de jobs do `systemd --user` fica ocupada ~10 s. Na VM pós-"Mínima" cai para ~2 s. O harness gravou a fila: é o **`render-menu-options-user.service`** (pacote `render-menu-options` 24.11.26-2343 da BigLinux, disparado no login pelo `render-menu-options-user.path`), que roda ~10 s a cada login (descrição da unidade: "Add rendering type in menu", acrescenta opções de renderização às entradas de menu). O mesmo pacote tem `render-menu-options.service` no boot (10 s no primeiro boot, segundo o openQA) | ~10 s até o login assentar | baixo | nenhum | BigLinux (`render-menu-options`): só trabalhar quando um `.desktop` mudou (carimbo/lista de hashes), ou condição no `.path`; no big-wm, no máximo um drop-in de prioridade baixa (`Nice`/`CPUWeight`) na sessão Big WindowMaker | campo `jobs_pendentes` do `login.json` do harness; `systemd-analyze --user blame` |
| A10 | **Dock com abas**: 12–18 MiB de PSS da sessão (medido com ele desligado). Fica: a missão pede para mantê-lo, opcional e medido com e sem | 5–10 MiB otimizando (não removendo) | baixo a médio | nenhum (Etapa E: cache de ícones limitado, sem varreduras contínuas) | big-wm (`tabbed-dock/`) | perfil `dock-classico` contra `padrao`; mesmo perfil antes/depois |
| A11 | **`big-wm-tray` (Python + GTK, 33 MiB)** na sessão Wayland, contra 21,7 MiB do snixembed no X11 | 10–20 MiB se reescrito em C/sem GTK | alto: o agente E está mudando a bandeja agora (dockapp do wlmaker) | nenhum se houver equivalência completa | big-wm | harness Wayland + testes da bandeja (`tests/integration/wayland-tray.sh`) |
| A12 | **Serviços sem hardware/uso** sempre ativos: ModemManager (6,4 MiB, sem modem), cupsd (2,8 MiB, sem impressora; pode ser por socket), `gpg-agent@etc-pacman.d-gnupg` (resto do `pacman-key`), `grub-btrfsd` (bash + inotifywait) | ~10–12 MiB | baixo a médio (hardware que aparece depois precisa funcionar: ativação por udev/socket) | nenhum | perfil `bigcommunity/wmaker` / BigLinux | modem USB e impressora conectados depois do boot continuam funcionando; harness |
| A13 | **Bloqueio de tela**: Xorg +16 MiB e wlmaker +15 MiB enquanto a tela está bloqueada, mais o bloqueador (5–10 MiB) | 15–25 MiB só com a tela bloqueada | baixo | nenhum | big-wm-lock / i3lock / swaylock | amostra de 15 min do harness |
| A14 | **indicator-sound-switcher (Python + GTK, 32–34 MiB)**: **mantido**. Só sai com substituto completo (troca de saída e entrada pela bandeja nas duas sessões) e mais leve | 20–25 MiB se houver substituto em C | alto (funcionalidade pedida pelos usuários) | nenhum se o substituto for equivalente | Etapa B | lista de funções do indicator + harness |

### Achados da auditoria

- **Duplicados:** nenhum. Um processo por componente nas duas sessões e nos
  4 ciclos de login/logout (X11). O `pamac-tray` do pamac e o
  `big-wm-wayland-pamac-tray` não sobem juntos. O
  `app-org.manjaro.pamac.tray@autostart` é cortado pelo drop-in do big-wm.
- **Órfãos depois do logout:** nenhum processo do usuário 20 s depois de
  sair (X11, 4 ciclos). Wayland: ver
  [Login/logout repetidos](performance-benchmarks.md#loginlogout-repetidos).
- **Serviços de outros desktops na sessão:** nenhum processo do Plasma,
  GNOME Shell ou XFCE. Os cortes do big-wm funcionam: big-preload-user,
  sync-gnome-theme-to-qt, blueman sem Bluetooth, applet de impressão sem
  impressora. Ficam, por serem usados: at-spi (acessibilidade),
  xdg-desktop-portal-gtk (backend de portal do GNOME, usado pelo GTK),
  GVfs (lixeira, montagens no Thunar), uresourced (BigLinux). Autostarts
  sem `OnlyShowIn` que não sobem aqui: `xapp-sn-watcher`, `vmware-user`,
  `spice-vdagent` (na VM do usuário, com canal SPICE, o spice-vdagent sobe
  e ocupa 13–50 MiB; o vmware-user tenta subir e falha).
- **Python/GTK residentes:** indicator-sound-switcher (as duas sessões),
  big-wm-tray e big-wm-wprefs-sync (Wayland), big-wm-x11-repaint (X11, só
  libX11, 8,2 MiB, enquanto o picom roda), biglinux-microphone-cli.
- **Laços de polling:** dockapps (A8). O `big-btrfs-balance` (script com
  `sleep`) roda só no começo do boot e terminou antes da amostra de 5 min.
  O `pw-dump --monitor` do biglinux-microphone e o big-wm-x11-repaint não
  acordaram nas janelas medidas (0 trocas de contexto/s).
- **Unidades com falha:** 0 (sistema e usuário) em todos os boots.
- **gtk-nocsd:** cada aplicativo GTK 3 da sessão (nm-applet, Dock com abas,
  indicator-sound-switcher, snixembed) tem um filho dormindo
  `ld-linux-x86-64.so.2 /usr/lib/libgtk-nocsd.so.0 <app>` de 0,12 MiB. É
  inofensivo para a memória, mas é um processo a mais por aplicativo, e o
  número varia entre logins. Vale entender no gtk-nocsd.
- **Atalho Quit do wlmaker** (Ctrl+Alt+Backspace) não encerrou a sessão
  quando enviado por QMP `send-key`. Funcional, fora do escopo de
  desempenho: verificar com teclado físico (hipótese: opção XKB
  `terminate:ctrl_alt_bksp`; confirmada e corrigida na Etapa D, D7). Ver
  [Login/logout repetidos](performance-benchmarks.md#loginlogout-repetidos).

### Ordem proposta para as próximas etapas

1. A3, A9 e A7: só big-wm ou configuração, baixo risco, ganho medido rápido.
2. A6 e A2: investigar com `smaps` por mapeamento antes de qualquer
   mudança. São os maiores processos de cada sessão.
3. A4, A5 e A12: dependem do perfil/BigLinux. Propor com os números e manter
   ativação sob demanda para hardware e compartilhamento.
4. A1: apresentar a medição à BigLinux. Não mexer na reserva sem um teste de
   pressão de memória.
5. A10, A11 e A14: nas etapas E e B, com equivalência funcional provada.

## Decisões da Etapa B/F: serviços do sistema (2026-10-28)

Itens de sistema da lista acima: A1, A4, A5, A7, A9 e A12, mais um achado
durante a medição (A9b). Para cada um:
hipótese, medida antes, menor mudança, medida depois e verificação de que o
recurso continua funcionando. A imagem medida "depois" é a `02-sistema`
(overlay sobre a `01-main-f65e26d`, montada com
`tests/perf/bigwm-perf-stage.sh`, ver
[performance-benchmarks.md](performance-benchmarks.md#etapa-bf-serviços-do-sistema)).
Mudanças da edição ficam em `big-windowmaker/iso-profiles`
(`bigcommunity/wmaker/desktop-overlay`, nada no `shared/`). As do big-wm
valem só nas sessões Big WindowMaker.

| # | Decisão | Onde | Efeito medido | Funciona? |
|---|---|---|---|---|
| A12a | **ModemManager só com modem** — aceito | iso-profiles: `ModemManager.service.d/bigwm-sob-demanda.conf` (condições de dispositivo) + `udev/rules.d/90-bigwm-modemmanager.rules` | −6,3 MiB de PSS (sistema) nas duas sessões; o ModemManager não aparece em nenhuma das 40 amostras (no controle, em todas) | sim: modem CDC-ACM conectado depois do boot (gadget `g_serial` sobre `dummy_hcd`) → udev → ModemManager sobe em 4 s e o NetworkManager registra "ModemManager now available"; sem modem, o boot só registra "skipped" (o NetworkManager não insiste) |
| A7 | **Primeira verificação do pamac 10 min depois do login** — aceito | big-wm: `pamac-tray.service.d/big-wm.conf` (`Type=exec`, `ExecStart=/usr/lib/big-wm/big-wm-pamac-tray`), autostarts XDG do pamac pulados nas sessões Big WindowMaker, `big-wm-wayland-pamac-tray.desktop` removido (era uma segunda cópia) | contra o controle do mesmo dia (em que o pamac atualiza o banco no login): aos 90 s, RAM usada −111 MiB (X11) e −179 MiB (Wayland), PSS de todos os processos −87 e −91; aos 5 min, sem diferença do pamac (4,1 MiB do pamac-tray ainda não subiram); aos 15 min, RAM usada +25 e +52 MiB (a cauda da verificação, que agora acontece ~4,5 min antes da amostra), com PSS −5 e −8 | sim: `pamac-tray` rodando aos 600 s, um só (o autostart do Wayland iniciava outro sem atraso) |
| A9 | **render-menu-options sem segurar o login** — aceito | iso-profiles: `/etc/systemd/user/render-menu-options-user.service.d/bigwm-nao-bloqueia.conf` (`Type=exec`, a espera de 10 s dentro do processo, `Nice=19`, `IOSchedulingClass=idle`) | junto com o A9b: login → área de trabalho pronta de 11,7 → 2,7 s (X11) e 12,1 → 2,9 s (Wayland); CPU nos 30 s seguintes à amostra de 30 s 2,7 % → 0,5 % (X11) e 1,6 % → 0,2 % (Wayland) | sim: roda uma vez por login, 10,0 s, código 0, 30 ms de CPU, e a fila de jobs fica vazia |
| A9b | **biglinux-snapshot-detect só no boot por snapshot** — aceito (achado ao medir o A9: com o render-menu-options fora, era ele que segurava a fila) | iso-profiles: `/etc/systemd/user/biglinux-snapshot-detect.service.d/bigwm-so-em-snapshot.conf` (`ExecCondition` com o mesmo teste do aplicativo: `timeshift-btrfs/snapshots` no `/proc/cmdline`) | ver A9: com só o drop-in do render-menu-options, a fila ficou 10,6–11,2 s presa no biglinux-snapshot-detect (um boot de cada sessão); com os dois, 2,7/2,9 s | sim: num boot normal a unidade é pulada na hora (sem `sleep 10` e sem abrir Python + GTK 4); num boot por snapshot o comportamento é o do pacote |
| A1 | **Reserva do kernel (auto-rules-io)** — rejeitado, fica como a BigLinux definiu | — | ver o teste de pressão abaixo | — |
| A4 | **Samba sob demanda** — rejeitado | — | ~24 MiB ficam | não: sem o smbd, `net usershare add … Everyone:R` (o que o plugin de compartilhamento do Thunar faz) falha com "cannot convert name Everyone to a SID… Maybe smbd is not running"; a primeira pasta compartilhada nunca seria criada |
| A12b | **CUPS pelo socket** — rejeitado | — | ~3 MiB ficam | o socket e a impressão em PDF funcionaram, mas com o `cupsd.conf` da BigLinux (`Browsing Yes`, `BrowseLocalProtocols dnssd`) o cupsd não sai ocioso (ativo 150 s depois, com `IdleExitTimeout 60` e sem impressora) e o arquivo `/var/cache/cups/org.cups.cupsd` que ele deixa dispara o `cups.path` no boot seguinte: residente de novo depois do primeiro uso ou do primeiro reboot |
| A12c | **gpg-agent do chaveiro do pacman** — rejeitado | — | ~1 MiB fica | encerrá-lo por tempo (`RuntimeMaxSec`) poderia matá-lo no meio de uma atualização longa do pacman |
| A5 | **Filtros do microfone sob demanda** — rejeitado aqui, proposto à BigLinux | — | ~16 MiB ficam | uma condição "existe dispositivo de captura ALSA" cortaria os filtros na VM, mas deixaria sem redução de ruído o microfone de um fone Bluetooth (nó do PipeWire, não `/dev/snd`) |

### A1: teste de pressão de memória

Hipótese: o `vm.watermark_scale_factor = 150` do auto-rules-io (padrão do
kernel: 10) e o `vm.min_free_kbytes` de 3 % da RAM do pre-systemd-swap
(biglinux-systemd-swap; padrão do kernel com THP nesta VM: 67584) reservam
~465 MiB que só aparecem como "RAM usada". Se valores menores mantivessem
a resposta do desktop sob pressão, valeria um override só da edição.

Teste (`tests/perf/bigwm-perf-pressure.py`, `bigwm-perf-vm.py pressure`,
sessão X11, imagem `01`, ajustes aplicados com `sysctl -w` depois do boot):

- um processo aloca 2600 MiB de memória anônima incompressível (o zram
  não a absorve) e continua tocando nela;
- outro lê um arquivo de 1 GiB em laço;
- por 120 s, o gnome-calculator abre a cada 15 s (tempo até a janela
  entrar na `_NET_CLIENT_LIST`) e `python3 -c pass` roda a cada 2 s;
- deltas do `/proc/vmstat` e do PSI.

| Mediana [mín–máx] | A: fator 150, min_free 120078 (como vem) (N=6) | B: fator 10, min_free 120078 (N=4) | C: fator 10, min_free 67584 (N=4) |
|---|---:|---:|---:|
| MemAvailable em repouso, antes da pressão (MiB) | 2907.2 [2877.8–2926.3] | 3008.6 [2997.4–3027.8] | 3214.2 [3197.1–3220.5] |
| 1ª abertura do aplicativo sob pressão (s) | 1.9 [1.7–2.2] | 2.0 [1.6–2.3] | 1.7 [1.6–2.1] |
| aberturas sob pressão, mediana do boot (s) | 0.5 [0.4–0.8] | 0.5 [0.5–1.5] | 0.6 [0.5–1.2] |
| pior abertura (s) | 1.9 [1.7–2.2] | 2.0 [1.6–2.3] | 1.7 [1.6–2.1] |
| `python3 -c pass`, p90 (ms) | 46.9 [41.3–63.8] | 43.7 [34.6–52.3] | 44.9 [30.0–52.0] |
| PSI memory some (s em ~130 s) | 7.8 [6.5–40.6] | 32.3 [6.7–35.7] | 26.9 [19.8–41.3] |
| PSI memory full (s) | 6.1 [5.5–31.6] | 25.8 [5.1–28.5] | 22.0 [15.9–31.6] |
| travadas em reclaim direto (`allocstall`) | 180.5 [145.0–296.0] | 600.0 [423.0–719.0] | 660.5 [343.0–815.0] |
| páginas varridas em reclaim direto (mil) | 312.2 [191.7–636.8] | 1411.7 [417.8–4207.7] | 1627.8 [645.9–2184.8] |
| refaults de arquivo (mil) | 488.1 [89.1–49870.0] | 53039.7 [210.5–61841.4] | 43970.6 [27246.2–60183.2] |
| processos mortos pelo OOM | 0 (nenhum dos 6 boots) | 1 (num dos 4 boots) | 4 (num dos 4 boots) |

Resultado:

- A abertura do aplicativo e o comando curto não mudam (1,7–2,0 s na
  primeira abertura sob pressão, ~0,5 s nas seguintes).
- Com o fator 150, o kswapd recupera cedo: ~3,5× menos travadas em reclaim
  direto (`allocstall` 145–296 contra 343–815, faixas sem sobreposição) e
  ~4× menos tempo de memória "full" no PSI (mediana 6 s contra 22–26 s).
- OOM não separa as configurações com segurança. Na série, o fator 150
  teve 0 de 6 boots com OOM, contra 1 de 4 nas outras duas. Mas a
  calibração com o fator 150 e o mesmo alvo também teve 3 processos mortos.
  Com o `oom_kill_allocating_task = 1` da BigLinux, morre quem estiver
  alocando: Dock com abas, bandeja, indicator-sound-switcher.

O ganho de MemAvailable seria de +101 MiB (fator 10) ou +307 MiB (fator
10 e `min_free_kbytes` padrão), em troca de um comportamento pior sob
pressão numa máquina de 4 GiB (reclaim direto e PSI). **Decisão: manter o
ajuste da BigLinux.**
A "RAM usada" (MemTotal − MemAvailable) do Big WindowMaker passa a vir
sempre acompanhada do "usada" do `free`, que não inclui essa reserva.

### Propostas para a BigLinux (texto para issue/PR)

1. **render-menu-options** (`render-menu-options-user.service`): "O serviço
   de usuário é `Type=oneshot` com `ExecStartPre=/bin/sleep 10` e é
   disparado no login pelo `render-menu-options-user.path`
   (`PathModified=%h/.local/share/applications`). Com isso o
   `systemd --user` fica 10 s com um job pendente a cada login. Medimos
   11,7 s até a sessão ficar sem jobs; com este drop-in e o do
   biglinux-snapshot-detect (abaixo), 2,7 s. Proposta:
   `Type=exec` com a espera dentro do `ExecStart` (ou
   `TriggerLimitIntervalSec` no `.path` no lugar do `sleep`), `Nice=19` e
   `IOSchedulingClass=idle`; e só chamar o `update-desktop-database`
   quando algum `.desktop` mudou, porque ele reescreve o `mimeinfo.cache`
   no próprio diretório vigiado. No Big WindowMaker isso está num drop-in
   da edição (`bigwm-nao-bloqueia.conf`)."
2. **grub-btrfs-timeshift** (`biglinux-snapshot-detect.service`): "O
   serviço de usuário é `Type=oneshot` com `ExecStartPre=/bin/sleep 10` e
   abre o `biglinux-snapshot-restore` (Python + GTK 4 + libadwaita) em todo
   login só para ele conferir o `/proc/cmdline` e sair. Proposta: fazer o
   teste no próprio unit (`ExecCondition=/bin/sh -c 'grep -q
   timeshift-btrfs/snapshots /proc/cmdline'`), que pula a espera e o Python
   num boot normal. No Big WindowMaker isso está num drop-in da edição
   (`bigwm-so-em-snapshot.conf`)."
3. **biglinux-noise-reduction-pipewire**: "Os filtros (eco e cadeia do
   microfone, ~16 MiB em três processos) sobem mesmo sem nenhuma fonte de
   captura (VM sem microfone). Como o `biglinux-microphone-cli watch-route`
   já acompanha o PipeWire, ele poderia carregar os filtros só quando
   existir um nó de captura real, inclusive Bluetooth, e descarregá-los
   quando ele sumir."
4. **Samba e o compartilhamento do Thunar**: "smbd + nmbd somam ~24 MiB
   residentes sem nenhum compartilhamento. Não dá para iniciá-los só quando
   aparece o primeiro usershare, porque `net usershare add … Everyone:R`
   precisa do smbd para resolver o SID. Proposta: o plugin (ou um
   auxiliar com polkit) inicia o smb/nmb na hora de compartilhar, ou grava
   a ACL com o SID `S-1-1-0`; aí o serviço pode ficar sob demanda (`.path`
   em `/var/lib/samba/usershare`)."
5. **CUPS**: "Com `Browsing Yes` e `BrowseLocalProtocols dnssd` o cupsd não
   sai quando ocioso, mesmo iniciado pelo socket e sem impressoras. Se a
   BigLinux quiser o CUPS sob demanda, precisa rever esse par (o avahi nem
   está habilitado) antes de trocar o `cups.service` pelo `cups.socket`."
6. **auto-rules-io**: "Mantivemos o `watermark_scale_factor = 150`: no
   teste de pressão numa VM de 4 GiB ele reduziu o reclaim direto (~3,5×)
   e o PSI de memória (~4×). Sugestão só de documentação: a reserva aparece como
   'memória usada' em ferramentas que usam MemAvailable (~465 MiB em
   4 GiB)."

## Etapa D: sessão Wayland (wlmaker e residentes)

Branches `feature/desempenho-wayland` em big-wm, packages e no fork do
wlmaker (patches 0100–0103, wlmaker 1:0.8.r80.g7481afe-14). Medição com o
mesmo harness e a mesma VM da linha de base, só a sessão Wayland, overlays
novos a cada boot:

- **antes** = imagem `02-main-be71855` (`01-main-f65e26d` + wlmaker -13 e
  big-wm `be71855` do main, onde a bandeja já é dockapp). Os números batem
  com a linha de base de f65e26d (sessão 353 MiB aos 5 min).
- **depois** = imagem `03` (antes + wlmaker -14 + big-wm `53bf9f9`: D1–D4).
- **depois-final** = imagem `04` (antes + wlmaker -14 + big-wm `ed62eb3`:
  D1–D6), para medir D6, que acabou revertida; no resto o código é o da
  imagem 03 e os números repetem os de depois.

Boots alternados antes/depois (5 + 5), mais 5 boots de depois-final e 4
ciclos de login/logout na imagem 04.
Resultados brutos em `tests/perf/resultados/2026-10-28-wayland/`.

| # | Mudança (onde) | Hipótese e diagnóstico | Antes → depois (mediana, 5 min) | Verificação funcional | Decisão |
|---|---|---|---|---|---|
| D1 | wlmaker 0100: sem aceleração 3D, renderizador **pixman** em vez de OpenGL ES no llvmpipe (fork, `src/backend/backend.c`) | A2. `smaps` do wlmaker na VM: libLLVM 46,3 MiB + libgallium 11,3 MiB (código do llvmpipe, privado) + heap 31,5 MiB + anônima 12,9 MiB (cópias das texturas: uma de 7,9 MiB = o papel de parede 1920×1080). O wlroots escolhia GLES2 via `kms_swrast` (virtio-gpu sem virgl tem `renderD128`, então "sem nó de renderização" não serve de critério). A escolha do wlroots agora é testada num processo filho (`GL_RENDERER` contém llvmpipe/softpipe → pixman); o libLLVM nunca entra no compositor. `WLR_RENDERER` continua mandando; com GPU de verdade nada muda | wlmaker 108,6 → 28,1 MiB (investigação, 1 boot, só D1) | menus, Dock/Clip, bandeja, X11 (WPrefs) pelo XWayland, capturas, `ctest` | **mantida** |
| D2 | wlmaker 0101: item de menu guarda só o buffer do estado atual | massif do wlmaker (D1 aplicada): 9,7 MB dos 15 MB do heap eram buffers de itens de menu, 3 por item (normal, realçado, desabilitado) para ~180 itens do menu raiz com os submenus gerados | heap 14,5 → 8,3 MiB; wlmaker 28,1 → 22,0 MiB (investigação, 1 boot) | menu raiz, submenus, realce, radio/check (capturas) | **mantida** |
| D3 | wlmaker 0102: itens de menus fechados sem buffer; dimensões vêm do estilo | o buffer do estado atual continuava alocado com o menu fechado (~3,2 MB) | heap 8,3 → 5,2 MiB; wlmaker 22,0 → 18,9 MiB (investigação, 1 boot); abrir e fechar menus volta ao mesmo heap | idem, menu do Dock (wlmdock), menu de dockapp, reabrir menus | **mantida** |
| D1–D3 juntas | — | — | **wlmaker 108,6 [108,5–108,9] → 18,3 [18,3–18,3] MiB**; CPU 1,67 → 0,67 ms/s; trocas de contexto 9,8 → 5,7/s (5+5 boots) | — | — |
| D4 | big-wm: vigia do WPrefs por `big-wm-wprefs-sync.path` (systemd --user, `PathChanged` nos diretórios) + `--triggered` | A3. Processo Python residente só para o inotify | `big-wm-wprefs-sync` 16,2 → 0 MiB (sem processo) | latência WPrefs (wdwrite) → `Config.plist` do wlmaker: 0,51–0,52 s (6×) contra 0,46–0,48 s do vigia residente; SIGHUP chega ao wlmaker; 36 testes de `tests/wprefs` | **mantida** |
| D5 | big-wm-tray: sem o `ssl` que o asyncio do PyGObject importava | A11. libssl + libcrypto + `_ssl` carregados sem uso | bandeja 33,3 → 32,1 MiB (5+5 boots); 34,7 → 33,0 MiB numa bandeja nova (2×) | `wayland-tray.sh` inteiro (paginação, setas, balões, menus, modo escuro, arrastar Dock↔Clip): PASS | **mantida** |
| D6 | big-wm-lock: trava para um só swaylock | a linha de base tem 2 processos swaylock aos 15 min em 5 de 5 boots; suspeita de dois bloqueios simultâneos (wlmaker + logind) | sem diferença: 2 swaylocks, 11,1 MiB (5 boots, imagem 04) | VM com `IdleSeconds = 30` e o big-wm-lock registrando as chamadas: **uma** chamada (do wlmaker) e dois processos swaylock, o primeiro sem sockets (o auxiliar do PAM, criado antes do `-f`), o segundo com a conexão Wayland. Não é duplicado | **revertida** (39bdc17) |
| D7 | wlmaker 0103: atalhos casam também com o keysym do 1º nível da tecla | Quit (Ctrl+Alt+Backspace) não saía. Causa verificada: o `Config.plist` gerado tem `Options = "terminate:ctrl_alt_bksp"` (kbd-model-map do br-abnt2); com Ctrl+Alt, BackSpace vira `Terminate_Server`. Sem a opção, o atalho saía (testado) | funcional | QMP Ctrl+Alt+Backspace: a sessão termina e o wdm volta (antes: nada); os ciclos de login/logout do harness agora saem pelo atalho | **mantida** |

### Rejeitado ou sem ganho (com dados)

- **`XWAYLAND_NO_GLAMOR=1`**: o XWayland (36,6 MiB de PSS com o WPrefs, 18,9
  de libLLVM + 5,2 de libgallium) continua carregando o llvmpipe pelo GLX;
  com a variável, 36,4 MiB. Sem ganho; o wlroots não deixa passar `-shm`.
- **Criar o `DbusmenuGtk3.Menu` da bandeja só no primeiro clique**: −0,4 MiB
  (32,6 contra 33,0, 2×) e mudaria o primeiro clique (o menu chega
  assíncrono). Não feito.
- **`malloc_trim(0)`**: −1,1 MiB no wlmaker e −0,2 no big-wm-tray (gdb na VM).
  Pouco para um patch só da glibc. Não feito.
- **Reescrever a bandeja em C**: o Python responde por ~9 MiB rastreados
  (tracemalloc: bytecode dos módulos do gi/asyncio/logging) + libpython 2,4
  MiB; o resto (~20 MiB) é GTK, temas e ícones. Sem uma reescrita
  equivalente (SNI, dbusmenu, paginação, balões, modo escuro, adoção pelo
  wlmaker), fica proposto.
- **wlmdock, swaybg e dockapps `wlm*`**: sem buffers duplicados que valham
  mudança. wlmdock já usa pixman (4,2–4,3 MiB cada; heap 1,8); swaybg 3,0 MiB
  (o buffer do papel de parede, 7,9 MiB, fica mapeado só no wlmaker); dockapps
  1,3–3,4 MiB cada, ~2 trocas de contexto/s (um quadro por segundo do relógio
  e dos gráficos). Menos atualizações mudariam a função; não feito.

### Custo honesto do pixman

O PSS não vê os *dumb buffers* do DRM (a cadeia de quadros do pixman é GEM
shmem): **Shmem +7,8 MiB** (32,2 → 40,0) aos 30 s–5 min e +15,8 MiB com a
tela bloqueada. Eles entram na RAM usada (MemTotal − MemAvailable), que
mesmo assim cai ~49 MiB.

### Resultado (sessão Wayland, perfil padrão, 5 boots cada, mediana [mín–máx])

| t | Perfil | RAM usada (MemTotal − MemAvailable) | PSS todos | PSS sessão | Shmem |
|---:|---|---:|---:|---:|---:|
| 5 min | antes | 986 [966–998] | 450 [450–450] | 353 [353–354] | 32,2 |
| 5 min | depois | 937 [919–951] | 345 [344–353] | 247 [246–248] | 40,0 |
| 15 min (bloqueada) | antes | 973 [952–1004] | 476 [476–477] | 380 [380–380] | 32,2 |
| 15 min (bloqueada) | depois | 925 [905–935] | 363 [363–364] | 266 [265–267] | 48,0 |

Login → área de trabalho pronta: 11,8 s antes e depois (o teste do
renderizador no processo filho não aparece). CPU em repouso aos 5 min: 0,2
→ 0,1 % e 111 → 81 trocas de contexto/s no sistema. Ciclos de
login/logout (imagem 04, 4 ciclos, saída pelo atalho Quit): 57 processos na
sessão em todos, PSS da sessão 262,8–263,9 MiB, 0 processos do usuário 20 s
depois de cada logout.

### Pendências da Etapa D

- **Suspensão na VM**: QEMU `-machine pc` suspende (S3 `deep`) e acorda
  (`system_wakeup`); o XWayland continua parado depois de retomar
  (verificado nas imagens 02 e 03). Mas a virtio-vga não volta ("Display
  output is not active") **também na imagem antes**, e o login seguinte não
  abre; uma vez (imagem 03) o convidado travou depois de retomar e sair. Fica
  a validar em hardware real.
- **Zumbis do Autostart do wlmaker**: `big-wm-session-ctl start` e
  `big-wm-wallpaper --restore` ficam `<defunct>` a sessão inteira (o wlmaker
  guarda os processos do `Autostart` numa pilha e só os espera ao sair). Sem
  memória; corrigir no wlmaker exige decidir se o Autostart continua sendo
  encerrado com o compositor.
- **Esc não fecha o menu (dbusmenu) de um ícone da bandeja**; clicar fora
  fecha. Não comparado com a imagem antes.
- **XWayland**: com o primeiro app X11 (WPrefs), 36,6 MiB de PSS (GLX com o
  llvmpipe); sai ~10 s depois do último cliente (`-terminate 10` do
  wlroots) e volta sob demanda.
