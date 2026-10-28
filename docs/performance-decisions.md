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
