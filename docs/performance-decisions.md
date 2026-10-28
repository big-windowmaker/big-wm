# Decisões de desempenho

Registro das decisões da missão de memória: hipótese, medição, mudança,
nova medição e resultado. A linha de base está em
[performance-benchmarks.md](performance-benchmarks.md). Toda mudança passa
pelo mesmo harness (`tests/perf/`), com a mesma imagem e VM, e é comparada
pela mediana dos boots aos 5 e 15 min.

## Oportunidades (Etapa A)

Levantadas na linha de base de 2026-10-28: imagem dourada, big-wm main
`f65e26d`, 5 boots por sessão. **Nada aqui foi implementado ainda** (Gate
A). Ganho = estimativa a partir do PSS/USS medido do processo ou da função,
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
  `terminate:ctrl_alt_bksp`). Ver
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
