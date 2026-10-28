# Benchmarks de memória e CPU

Linha de base ("Antes") da missão de memória (Etapa A): o sistema
**instalado** do Big WindowMaker em repouso, sem aplicativos do usuário,
medido nas sessões X11 e Wayland, ≥ 5 boots independentes por perfil.
Nenhuma otimização foi feita antes desta medição. As oportunidades estão em
[performance-decisions.md](performance-decisions.md); o histórico, em
[memoria.md](memoria.md).

Data: 2026-10-28. Tudo em **MiB** (1024 KiB). Mediana e, entre colchetes,
mínimo–máximo dos boots.

## Resultado principal (perfil padrão, 5 boots por sessão)

Perfil padrão = como a ISO instala: Dock clássico **e** Dock com abas
(`both`), tema claro, pt_BR, sem Bluetooth nem impressora.

| Sessão | t após o login | RAM usada do sistema (MemTotal − MemAvailable) | "usada" do `free` | PSS de todos os processos | PSS da sessão | PSS da sessão + Xorg |
|---|---:|---:|---:|---:|---:|---:|
| X11 | 30 s | 914 [908–919] | 451 [445–455] | 427 [421–433] | 258 [250–264] | 327 [321–333] |
| X11 | 90 s | 963 [942–979] | 499 [478–514] | 431 [420–431] | 265 [251–265] | 333 [323–334] |
| X11 | 5 min | 950 [941–968] | 485 [476–503] | 416 [405–416] | 249 [235–249] | 318 [307–318] |
| X11 | 15 min (tela bloqueada) | 947 [926–971] | 482 [460–505] | 443 [435–445] | 260 [247–261] | 345 [337–348] |
| Wayland | 30 s | 923 [905–926] | 447 [429–450] | 459 [459–460] | 362 [361–362] | — |
| Wayland | 90 s | 984 [958–995] | 507 [481–518] | 465 [464–465] | 368 [367–368] | — |
| Wayland | 5 min | 978 [962–994] | 500 [484–516] | 450 [449–459] | 353 [352–353] | — |
| Wayland | 15 min (tela bloqueada) | 969 [948–971] | 491 [470–493] | 476 [475–477] | 379 [379–379] | — |

No Wayland o compositor (wlmaker) já está na sessão; não há Xorg.

**Leitura honesta.** A meta da missão (150–200 MiB) fica muito longe em
qualquer das três métricas. O número que o usuário vê no `free`/`htop`
("usada") é ~485 MiB no X11 e ~500 MiB no Wayland; MemTotal − MemAvailable
é ~950–980 MiB porque inclui ~465 MiB que **não são de nenhum processo**
(ver [A diferença entre as métricas](#a-diferença-entre-as-métricas)).
Os 191 MB de PSS da sessão de [memoria.md](memoria.md) eram de outra
medição (live, `drop_caches`, sem o indicator-sound-switcher). Não servem de
comparação.

### Tabela da missão

Amostra de 5 min (repouso, tela desbloqueada), mesmo harness, mesma imagem
base e mesma VM. Entre parênteses na linha "Depois", o controle medido no
mesmo dia (ver [Etapa B/F](#etapa-bf-serviços-do-sistema)). As etapas C, D
e E acrescentam as suas linhas.

| Perfil | Sessão | RAM usada do sistema (MiB) | PSS todos processos (MiB) | PSS sessão (MiB) | Boot/login (s) | Estado dos testes |
|---|---|---:|---:|---:|---:|---|
| Antes | X11 | 950 [941–968] | 416 [405–416] | 249 [235–249] (318 com Xorg) | 8.1 / 11.7 | medido: 5 boots, imagem dourada, big-wm f65e26d |
| Depois (Etapa B/F) | X11 | 951 [949–959] (controle do mesmo dia: 974) | 404 [394–404] (416) | 246 [235–247] (249) (315 com Xorg) | 7.9 / 2.7 | medido: 5 boots, imagem 02-sistema; controle: 3 boots |
| Antes | Wayland | 978 [962–994] | 450 [449–459] | 353 [352–353] | 8.0 / 12.1 | medido: 5 boots, imagem dourada, big-wm f65e26d |
| Depois (Etapa B/F) | Wayland | 962 [953–980] (controle: 991) | 434 [433–435] (450) | 346 [345–347] (353) | 8.1 / 2.9 | medido: 5 boots, imagem 02-sistema; controle: 3 boots |
| Depois (só Etapa D) | Wayland | 937 [919–951] | 345 [344–353] | 247 [246–248] | 7.5 / 11.8 | medido: 5 boots alternados com 5 de "antes" na imagem 02 (main be71855 + wlmaker -13: 986 / 450 / 353); ver [Depois da Etapa D](#depois-da-etapa-d-wayland) |

Boot/login = boot total do `systemd-analyze` / login → área de trabalho
pronta.

### Boot e login (s)

| Sessão | Boot total (`systemd-analyze`) | `graphical.target` | Login → compositor | Login → `big-wm-session.target` | Login → área de trabalho pronta |
|---|---:|---:|---:|---:|---:|
| X11 | 8.1 [7.7–8.6] | 6.9 [6.5–7.2] | 1.1 [1.0–1.6] | 1.3 [1.2–1.7] | 11.7 [11.2–12.0] |
| Wayland | 8.0 [7.6–8.9] | 6.7 [6.2–7.4] | 1.1 [0.7–1.2] | 2.1 [1.6–2.2] | 12.1 [11.9–12.5] |

"Área de trabalho pronta" = compositor rodando, `big-wm-session.target`
ativo e **fila de jobs do `systemd --user` vazia**. A área de trabalho
aparece em ~1 s. Nos ~10 s seguintes a fila fica com o
`render-menu-options-user.service` da BigLinux ("Add rendering type in menu"), que roda
a cada login (identificado pelo harness na sessão live; ver
Oportunidades, A9). Na VM do usuário, depois do perfil "Mínima" da BigLinux,
o mesmo marco cai para ~2 s.

### CPU em repouso (janela de 30 s depois de cada amostra)

| Sessão | t | CPU ocupada (% das 2 vCPUs) | Trocas de contexto/s | Interrupções/s |
|---|---:|---:|---:|---:|
| X11 | 30 s | 2.7 [2.5–2.7] | 1887 [1570–2898] | 1110 [964–1613] |
| X11 | 5 min | 0.5 [0.4–0.6] | 467 [462–471] | 345 [334–371] |
| X11 | 15 min (bloqueada) | 0.2 [0.2–0.3] | 170 [168–174] | 180 [173–209] |
| Wayland | 30 s | 1.6 [1.3–2.1] | 395 [308–1433] | 266 [223–783] |
| Wayland | 5 min | 0.2 [0.2–0.3] | 110 [107–112] | 95 [91–96] |
| Wayland | 15 min (bloqueada) | 0.1 [0.0–0.1] | 51 [50–52] | 56 [55–57] |

Em repouso o X11 acorda ~4× mais que o Wayland. Por processo (5 min, X11):
Xorg ~81 trocas de contexto/s e picom ~75/s (2,3 ms de CPU por segundo
cada), wmix e wmnet ~10/s, wmcpuload ~7/s. No Wayland: wlmaker ~10/s,
dockapps `wlm*` ~2/s cada.

### Memória do kernel e outros indicadores (5 min, mediana)

| Sessão | AnonPages | Shmem | Cached | SReclaimable | SUnreclaim | KernelStack | PageTables | Swap usada | Processos (sem threads do kernel) | Processos da sessão | XWayland | PSI memory some avg60 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|
| X11 | 242 | 19 | 1482 | 35 | 57 | 4.9 | 10.5 | 0 | 85 | 48 | — | 0.00 |
| Wayland | 258 | 32 | 1539 | 35 | 57 | 5.1 | 10.1 | 0 | 91 | 57 | não (sob demanda) | 0.00 |

Swap: o sistema instalado tem zram (`/dev/zram1`, 1,8 GiB, do
`biglinux-systemd-swap`) e um swapfile de 512 MiB. Nenhum foi usado em
nenhuma amostra. O cache de páginas cresce ~250 MiB entre 90 s e 5 min:
é a atualização do banco do pamac e os timers diários. Esse cache é
devolvido sob pressão.

## Custo por função (perfil padrão, 5 min, PSS mediana dos 5 boots)

Soma do PSS dos processos de cada função. A soma das linhas é o "PSS de
todos os processos" (416 e 450 MiB). Classificação em
`tests/perf/bigwm-perf-groups.py`; por processo em
`tests/perf/resultados/2026-10-28-antes/golden/componentes.tsv`.

| Função | X11 | Wayland |
|---|---:|---:|
| Servidor gráfico/compositor (Xorg + wmaker / wlmaker) | 82.3 (68.8 + 13.5) | 108.6 |
| Rede (NetworkManager, nm-applet, ModemManager, dnsmasq) | 49.2 | 36.6 |
| Sistema básico (systemd, journald, udevd, logind, cron, grub-btrfsd, getty...) | 38.3 | 37.7 |
| Som: indicator-sound-switcher (Python + GTK) | 34.5 | 32.0 |
| Som: PipeWire + WirePlumber + pipewire-pulse | 30.2 | 31.5 |
| Dock com abas | 25.0 | 14.5 |
| Samba (smbd, nmbd e auxiliares) | 23.6 | 23.6 |
| Bandeja: snixembed (X11) / big-wm-tray (Wayland, Python + GTK) | 21.8 | 33.0 |
| Autenticação (polkit-gnome + polkitd) | 19.2 | 10.6 |
| Som: redução de ruído do microfone (biglinux-microphone, 3 processos) | 15.8 | 15.5 |
| Portais (xdg-desktop-portal, -gtk, -wlr, documentos, permissões) | 13.8 | 16.1 |
| Compositor X11 (picom + big-wm-x11-repaint) | 10.5 | — |
| Sincronia WPrefs → wlmaker (`big-wm-wprefs-sync --watch`, Python) | — | 16.2 |
| Dockapps (X11: wmclock, wmix, wmcpuload, wmmemload, wmnet, wmsystemtray; Wayland: wlm*) | 4.3 | 13.4 |
| Dock/Clip Wayland (wlmdock ×2) + papel de parede (swaybg) | — | 11.7 |
| Notificações (dunst) | 8.2 | 7.2 |
| `systemd --user` + D-Bus da sessão | 7.7 | 7.7 |
| Discos (udisksd) | 6.1 | 6.1 |
| GVfs | 5.1 | 5.1 |
| wdm (tela de login, continua residente) | 4.9 | 5.6 |
| Atualizações (pamac-tray residente) | 4.1 | 4.1 |
| Impressão (cupsd), acessibilidade (at-spi), uresourced, bloqueio, outros | 11.2 | 13.3 |
| **Total (PSS de todos os processos)** | **416** | **450** |

Maiores processos (5 min, PSS / RSS / USS em MiB):

| Processo | X11 | Wayland |
|---|---|---|
| Xorg | 68.8 / 96.9 / 61.9 | — |
| wlmaker | — | 108.6 / 124.4 / 105.8 |
| indicator-sound-switcher | 34.4 / 59.5 / 29.3 | 31.9 / 60.2 / 25.7 |
| big-wm-tray | — | 33.0 / 65.4 / 25.7 |
| nm-applet | 27.6 / 78.8 / 16.2 | 16.0 / 47.2 / 10.3 |
| big-wm-tabbed-dock | 24.9 / 68.7 / 15.2 | 14.4 / 40.0 / 10.0 |
| snixembed | 21.7 / 67.6 / 12.0 | — |
| wireplumber | 20.9 / 38.9 / 18.3 | 21.7 / 39.0 / 19.3 |
| big-wm-wprefs-sync | — | 16.2 / 25.3 / 13.9 |
| polkit-gnome-authentication-agent-1 | 15.8 / 52.1 / 9.4 | 7.1 / 27.2 / 5.1 |
| NetworkManager | 13.6 / 27.2 / 11.7 | 12.6 / 27.0 / 10.2 |
| smbd | 12.2 / 28.2 / 6.2 | 12.2 / 28.1 / 6.2 |
| biglinux-microphone-pwloader (×2) | 11.6 / 22.0 / 10.1 | 11.5 / 22.2 / 10.2 |
| wmaker (`--for-real` + pai) | 13.5 | — |
| big-wm-x11-repaint (Python) | 8.2 / 11.2 / 7.2 | — |
| dunst | 8.2 / 25.7 / 5.3 | 7.2 / 23.9 / 4.9 |

Os mesmos aplicativos GTK 3 custam mais no X11: nm-applet +11,6 MiB,
polkit-gnome +8,7 e o Dock com abas +10,5. Ver Oportunidades, A6.

### Transientes do login (não aparecem a 5 min)

- **pamac (verificação de atualizações):** `pamac-session` (usuário) sobe
  ~30 s depois do login e sai antes de 5 min: 16–17 MiB a 90 s, 104 MiB no
  pico registrado (validação, 60 s). Quando o banco de pacotes é
  atualizado, o `pamac-broker` (root) soma mais **61 MiB** a 90 s. Isso
  aconteceu em todos os boots dos perfis medidos depois das 02:40 e em
  nenhum do perfil padrão (01:20–02:40). Depende da idade do banco no
  relógio da VM, não do perfil. Por isso a amostra de 90 s varia entre os
  perfis e as comparações usam 5 e 15 min.
- **Carregadores de imagem glycin** (gdk-pixbuf 2.44, em sandbox `bwrap`)
  do snixembed e do dunst: ~20 MiB por ~1 min no login.
- **Timers diários** (logrotate, systemd-tmpfiles-clean,
  pamac-snapshot-maintenance) rodaram no início dos boots (a imagem é sempre
  "do dia anterior" para eles). Entram no cache, não na RAM usada.

### Tela bloqueada (15 min)

Aos ~10 min de inatividade o bloqueio automático liga (i3lock no X11 via
xss-lock, swaylock no Wayland via swayidle). Por isso a amostra de 15 min é
de **tela bloqueada**: Xorg +16 MiB, wlmaker +15 MiB, mais o próprio
bloqueador (5–10 MiB). É o estado real de um desktop parado e foi mantido.

## Perfis adicionais (mesma imagem e VM)

Diferença para o perfil padrão (mediana; 5 min e 15 min). A amostra de
90 s não entra, por causa do transiente do pamac.

| Perfil | Boots | Sessão | Δ RAM usada (5 min / 15 min) | Δ PSS todos | Δ PSS sessão |
|---|---:|---|---:|---:|---:|
| Só Dock clássico (`--dock-mode=classic`: Dock com abas desligado) | 5 | X11 | −14 / +8 | −20 / −18 | −18 / −18 |
| Só Dock clássico | 5 | Wayland | −12 / −12 | −12 / −12 | −12 / −13 |
| Tema escuro (`big-wm-theme dark`) | 3 | X11 | +14 / +15 | 0 / +7 | 0 / 0 |
| Tema escuro | 3 | Wayland | −13 / +11 | +1 / +1 | +1 / 0 |

- **Dock com abas:** custa 12–18 MiB de PSS da sessão. Em RAM usada a
  diferença fica perto do ruído entre boots (±15 MiB). Continua opcional e
  ligado por padrão; a Etapa E trata do consumo dele.
- **Tema escuro:** sem diferença no PSS. As diferenças em RAM usada estão
  dentro da faixa entre boots, que vem do cache e da reserva do kernel.

Tabelas completas: `tests/perf/resultados/2026-10-28-antes/golden/resumo.md`.

## Depois da Etapa D (Wayland)

Mudanças em [performance-decisions.md](performance-decisions.md#etapa-d-sessão-wayland-wlmaker-e-residentes):
wlmaker 1:0.8.r80.g7481afe-14 (patches 0100–0103: pixman sem aceleração
3D, buffers de menu só quando visíveis, atalho Quit) e big-wm
`feature/desempenho-wayland` (vigia do WPrefs por unidade `.path`,
bandeja sem `ssl`).

Mesma VM e harness. A linha de base da Etapa A era de big-wm `f65e26d` com
wlmaker -12; o main andou (bandeja como dockapp, wlmaker -13), então o
"antes" foi medido de novo: imagem `02-main-be71855` (overlay da
`01-main-f65e26d` com os pacotes do main `be71855` e o wlmaker -13).
"Depois" = imagem `03-wl-53bf9f9-wlm14` (overlay da 02 com os pacotes
desta etapa). Boots alternados (antes, depois, antes...), 5 de cada, só
Wayland, uma VM por vez. Imagens: sha256 em
`tests/perf/resultados/2026-10-28-wayland/*.sha256`.

| t após o login | Perfil | RAM usada | "usada" do `free` | PSS todos | PSS sessão | Shmem | AnonPages |
|---:|---|---:|---:|---:|---:|---:|---:|
| 30 s | antes | 929 [914–937] | 453 | 460 [460–460] | 362 [362–363] | 32,1 | 261,0 |
| 30 s | depois | 875 [869–894] | 391 | 354 [353–355] | 256 [256–257] | 39,9 | 204,4 |
| 90 s | antes | 973 [960–995] | 496 | 465 [465–466] | 368 [368–369] | 32,2 | 262,3 |
| 90 s | depois | 923 [910–939] | 438 | 360 [359–360] | 263 [262–263] | 40,0 | 205,6 |
| 5 min | antes | 986 [966–998] | 508 | 450 [450–450] | 353 [353–354] | 32,2 | 258,2 |
| 5 min | depois | 937 [919–951] | 451 | 345 [344–353] | 247 [246–248] | 40,0 | 201,8 |
| 15 min (bloqueada) | antes | 973 [952–1004] | 495 | 476 [476–477] | 380 [380–380] | 32,2 | 279,8 |
| 15 min (bloqueada) | depois | 925 [905–935] | 431 | 363 [363–364] | 266 [265–267] | 48,0 | 212,1 |

Por processo (5 min, PSS mediana): wlmaker 108,6 → 18,3 MiB (USS 105,8 →
15,8); `big-wm-wprefs-sync` 16,2 → 0 (sem processo); big-wm-tray 33,3 →
32,1. Tela bloqueada (15 min): wlmaker 124,3 → 26,3. Os demais ficam iguais
(±0,9 MiB). Por função: `funcoes-{antes,depois}-wayland-{300,900}.md`.

- **PSS e RAM usada não caem igual.** O wlmaker com pixman desenha em
  *dumb buffers* do DRM, que o PSS não conta (são GEM shmem): Shmem +7,8 MiB
  (+15,8 com a tela bloqueada). Por isso o PSS de todos cai 105 MiB e a RAM
  usada cai ~49 MiB (os intervalos não se sobrepõem aos 5 min: antes ≥ 966,
  depois ≤ 951). O resto da diferença é a variação do cache e da reserva do
  kernel entre boots.
- **Boot/login:** iguais (boot 7,5 s; login → área de trabalho pronta 11,8
  s; login → compositor 0,7 s): o teste do renderizador num processo filho
  não aparece.
- **CPU em repouso (5 min):** 0,2 → 0,1 % das 2 vCPUs; 111 → 81 trocas de
  contexto/s no sistema; o wlmaker 1,67 → 0,67 ms de CPU por segundo e 9,8 →
  5,7 trocas de contexto/s (o pixman compõe os dockapps que se atualizam a
  cada segundo sem o llvmpipe).
- **XWayland:** parado em todas as amostras, antes e depois. Com o primeiro
  app X11 (WPrefs), 36,6 MiB de PSS (investigação, 1 medida); sai ~10 s
  depois do último cliente e volta sob demanda.
- **Login/logout:** 4 ciclos na imagem 04, saindo pelo atalho Quit
  (Ctrl+Alt+Backspace, agora funcionando): 57 processos na sessão em todos,
  PSS da sessão 262,8–263,9 MiB (90 s depois do login), 0 processos do
  usuário 20 s depois de cada logout.
- **Testes de integração** (`tests/integration/session-test.sh` num
  contêiner Manjaro, pacotes desta etapa + os do main de
  big-windowmaker/packages): 152 PASS, 2 SKIP, 3 XFAIL conhecidos
  (`integracao-results.tsv`), inclusive todos os da bandeja.

Uma terceira série ("depois-final", imagem 04, 5 boots) repetiu os números
de "depois" (5 min: 918 [906–942] / 345 / 247) e serviu para descartar a
mudança D6 (ver decisões).

## A diferença entre as métricas

Exemplo X11, 5 min, mediana:

| | MiB |
|---|---:|
| PSS de todos os processos | 416 |
| + kernel não recuperável (SUnreclaim 57 + pilhas 5 + tabelas de páginas 10,5) | ≈ 72 |
| ≈ "usada" do `free` (MemTotal − MemFree − Buffers − Cached − SReclaimable) | 485 |
| + reserva que o kernel desconta do MemAvailable (reservas das zonas + marca d'água *low*) | ≈ 465 |
| = MemTotal − MemAvailable | 950 |

O MemAvailable desconta da memória livre as reservas das zonas
(`totalreserve`, 252 MiB aqui) e a marca d'água *low* (176 MiB). Essa
parte não é de nenhum processo e não sai fechando programas. Na imagem
medida, os valores vêm de:

- `vm.watermark_scale_factor = 150` e `vm.watermark_boost_factor = 15000`,
  de `/etc/sysctl.d/91-memory.conf`, do pacote **auto-rules-io**
  26.02.23-2022 (o padrão do kernel é 10);
- `vm.min_free_kbytes = 120078`, que nenhum arquivo do `sysctl.d` define;
- marcas d'água resultantes (soma das zonas): min 117, low 176, high
  235 MiB.

Com esses números a fórmula do kernel (`si_mem_available`) dá 3196,4 MiB de
MemAvailable na tela de login, exatamente o medido (3196,4 MiB). Na VM do usuário
(8 GiB, depois da "Mínima", sem o auto-rules-io, fator 10) a reserva é de
~220 MiB. Oportunidade A1. Dados completos (`sysctl vm`, `zoneinfo`, donos
dos arquivos): `tests/perf/resultados/2026-10-28-antes/reservas.txt`.

## Verificação cruzada: VM do usuário depois da "Mínima"

VM libvirt "Big WindowMaker" do usuário: 8 GiB, 8 vCPUs, q35,
virtio-vga 1280x800, tema claro. Big WindowMaker 26.10.27-2337 (Dock com
abas de 5556e3d), wlmaker pkgrel 13, wdm 14. Em 2026-10-27 23:11 alguém
removeu ali 104 pacotes pelo pamac (a opção "Mínima" da BigLinux:
ashyterm, tts-biglinux, uresourced, system-config-printer-jobs,
sysctl-network, regras do udev...). **Não é o conjunto padrão de pacotes**
e serve só de verificação cruzada. 3 boots por sessão com
`tests/perf/bigwm-perf-libvirt.sh`. A amostra de 15 min de x11-1 foi
descartada: alguém abriu uma sessão Wayland na VM durante a medição.

| Sessão | t | RAM usada | PSS todos | PSS sessão | PSS sessão + Xorg |
|---|---:|---:|---:|---:|---:|
| X11 | 5 min | 614 [607–621] | 340 [340–345] | 199 [193–199] | 249 [247–250] |
| X11 | 15 min | 620 [618–623] (N=2) | 361 [358–363] | 206 [206–206] | 268 [268–268] |
| Wayland | 5 min | 679 [667–679] | 417 [415–423] | 327 [325–329] | 327 [325–329] |
| Wayland | 15 min | 699 [698–703] | 480 [477–481] | 391 [384–391] | 391 [384–391] |

Login → área de trabalho pronta: 1.9 s (X11) e 2.1 s (Wayland). Boot total
5.0 s. No X11 o PSS de todos os processos fica ~75 MiB abaixo da imagem
dourada e o da sessão ~50 MiB abaixo, mas ainda acima de 200 MiB. Parte da
diferença vem da resolução menor (Xorg 62 contra 69 MiB) e parte dos pacotes
removidos (uresourced, applets). Os números não são comparáveis um a um com
a imagem dourada: RAM, CPUs, resolução e pacotes diferem.

## Sessão live (ISO, não instalada)

Mesma VM (4 GiB, 2 vCPUs, 1920x1080) com a **mesma ISO**, sem disco:
`tests/perf/bigwm-perf-vm.py live`. Pacotes da ISO, não os do main:
big-wm 26.10.27-2115, biglinux-livecd 26.10.26-5, LightDM com login
automático. O usuário live (`community`) entra pelo console hvc0. O
coletor roda numa unidade `systemd-run` fora da sessão e o assistente é
concluído pelo teclado (Enter, Enter, Enter: idioma e teclado padrão, tema
claro). Locale pt_BR.UTF-8. Como o login automático abre a sessão antes do
assistente, **t = 0 é o início do wmaker**, ~34 s depois da abertura da
sessão no logind. 5 boots. **Live ≠ instalado:** a raiz é um overlay em
RAM, então tudo o que é gravado vira Shmem.

| t após o wmaker | RAM usada | "usada" do `free` | PSS todos | PSS sessão | PSS sessão + Xorg | Shmem |
|---:|---:|---:|---:|---:|---:|---:|
| 30 s | 1135 [1119–1151] | 418 [402–434] | 390 [389–394] | 222 [221–225] | 329 [328–333] | 272 |
| 90 s | 1304 [1285–1340] | 499 [480–534] | 460 [420–461] | 228 [228–231] | 335 [334–339] | 357 |
| 5 min | 1285 [1278–1301] | 480 [473–496] | 385 [384–389] | 212 [211–215] | 319 [318–323] | 357 |
| 15 min | 1283 [1278–1298] | 477 [473–492] | 381 [380–385] | 213 [212–216] | 320 [319–324] | 357 |

- **RAM usada ~330 MiB acima do instalado:** Shmem de 357 MiB (overlay em
  RAM, inclusive o `/run/initramfs` que o motor da ISO guarda para
  desligar). Não é custo do desktop.
- **Xorg 106,6 MiB** (contra 68,8 no instalado com o wdm). O live usa o
  LightDM, que inicia o Xorg com outra configuração. A causa não foi
  investigada.
- **Aplicativos GTK 3 mais leves que no X11 instalado:** snixembed 11,8
  contra 21,7 MiB, Dock com abas 15,1 contra 25,0, polkit (agente +
  polkitd) 9,3 contra 19,2. É a mesma sessão X11 com quase os mesmos
  pacotes, uma pista para a oportunidade A6.
- Não há Samba, ModemManager, cupsd nem dnsmasq rodando no live.
- Boot total 22,9 s [22,5–24,0] (`graphical.target` 19,8 s). A área de
  trabalho fica pronta 10,8 s depois do wmaker, e o que segura a fila é o
  mesmo `render-menu-options-user.service` do instalado.
- CPU em repouso: 0,4 % e ~480 trocas de contexto/s, como no X11 instalado.

## Login/logout repetidos

`tests/perf/bigwm-perf-vm.py cycles`: um boot e 4 logins e logouts
seguidos pelo wdm. Lista de processos 90 s depois de cada login e 20 s
depois de cada logout. Análise: `tests/perf/bigwm-perf-cycles.py`.

| Sessão | Logout | Processos na sessão (ciclos 1–4) | PSS da sessão (MiB) | Processos do usuário 20 s após o logout |
|---|---|---|---|---|
| X11 | SIGTERM ao wmaker (o mesmo caminho do "Sair" do menu) | 49, 49, 49, 49 | 265.9, 258.7, 265.6, 265.1 | 0, 0, 0, 0 |
| Wayland | SIGTERM ao wlmaker (ver abaixo) | 58, 57, 58, 58 | 368.8, 368.0, 368.5, 369.0 | 0, 0, 0, 0 |

- **Sem duplicados nem crescimento**: os mesmos componentes em todo login,
  com PSS estável (±4 MiB).
- **Sem órfãos**: nenhum processo do usuário sobra depois do logout. O
  `systemd --user` sai ao fim da última sessão.
- A única variação no Wayland (57 em vez de 58) são os processos
  auxiliares do gtk-nocsd: `ld-linux-x86-64.so.2 /usr/lib/libgtk-nocsd.so.0
  <app>`, um filho dormindo por aplicativo GTK (nm-applet, Dock com abas,
  indicator-sound-switcher, snixembed), com 0,12 MiB de PSS cada, que não
  usaram CPU em 15 min. Às vezes um deles não aparece.
- **Atalho Quit do wlmaker:** a primeira tentativa usou
  Ctrl+Alt+Backspace (`"Ctrl+Alt+Backspace" = Quit` no `Config.plist`)
  enviado por QMP `send-key`. A sessão não terminou, e o ciclo parou
  esperando o greeter (captura em
  `resultados/2026-10-28-antes/ciclos/wayland-atalho.txt`). Como o wlmaker
  não trata SIGTERM, ele foi encerrado por SIGTERM. O `startbigwm-wayland`
  segue para o `big-wm-session-ctl stop` como num "Sair" normal, mas os
  clientes perdem o compositor sem aviso. Hipótese a verificar com teclado
  físico: a opção XKB `terminate:ctrl_alt_bksp`, que a live mostra no
  `localectl`, troca a tecla por `Terminate_Server` e o atalho não casa.
  **Confirmada na Etapa D** (VM, QMP): o `Config.plist` gerado tem
  `Options = "terminate:ctrl_alt_bksp"` (kbd-model-map do br-abnt2); sem
  a opção o atalho saía. O wlmaker -14 (patch 0103) casa os atalhos também
  com o keysym do primeiro nível da tecla, e os ciclos Wayland agora saem
  pelo atalho ([Depois da Etapa D](#depois-da-etapa-d-wayland)).

## Etapa B/F: serviços do sistema

Medição de 2026-10-28 das mudanças de sistema aceitas na Etapa B/F
([decisões](performance-decisions.md#decisões-da-etapa-bf-serviços-do-sistema)):

- ModemManager só com modem;
- primeira verificação do pamac 10 min depois do login;
- render-menu-options e biglinux-snapshot-detect sem segurar o login.

Samba, CUPS, a reserva do kernel (A1), os filtros do microfone e o gpg-agent
foram medidos e rejeitados.

### Imagem medida

`02-sistema.qcow2` = `01-main-f65e26d.qcow2` + o tar de
`tests/perf/bigwm-perf-stage.sh`, com os arquivos do desktop-overlay da
edição (`big-windowmaker/iso-profiles`, branch
`feature/desempenho-sistema`) e do `make install-session` do big-wm (branch
`feature/desempenho-sistema`). O `big-wm-wayland-pamac-tray.desktop`
foi removido à mão, como o pacote novo faria. Não deu para gerar uma ISO
nova: os runners do GitHub estão bloqueados pelo Cloudflare nos
repositórios da BigLinux/BigCommunity.

```sh
tests/perf/bigwm-perf-stage.sh ../iso-profiles . stage.tar
qemu-img create -f qcow2 -F qcow2 -b 01-main-f65e26d.qcow2 02-sistema.qcow2
printf '%s\n' '@push stage.tar /var/tmp/stage.tar' \
  'tar --no-same-owner --no-overwrite-dir -xf /var/tmp/stage.tar -C / && rm /var/tmp/stage.tar && rm -f /etc/xdg/autostart/big-wm-wayland-pamac-tray.desktop && systemctl daemon-reload && udevadm control --reload && sync' |
  tests/perf/bigwm-perf-vm.py shell --image 02-sistema.qcow2 --out prep-02 --writable
tests/perf/bigwm-perf-run.sh 02-sistema.qcow2 resultados sistema 5
```

sha256 da imagem: `c3b67456…`.

### Controle do mesmo dia

O pamac só atualiza o banco de pacotes quando ele tem mais de 6 h
(`RefreshPeriod = 6`). Nas medições da linha de base (01:20–02:40) o banco
da imagem ainda era recente: o `pamac-broker` (61 MiB) não apareceu. Nas de
hoje à tarde ele aparece em todo boot. Por isso a imagem `01`, sem
nenhuma mudança, foi medida de novo na mesma tarde (3 rodadas): esse é o
"antes" justo. Nessa hora havia VMs de outros agentes no host, e o boot do
controle saiu mais lento (10,2–10,5 s contra ~8 s). A memória dentro do
convidado não depende disso.

**RAM usada (MemTotal − MemAvailable)** (MiB, mediana [mín–máx])

| Sessão | t | Antes (linha de base, N=5) | Controle do mesmo dia (imagem 01, N=3) | Depois (imagem 02, N=5) | Depois − controle |
|---|---:|---:|---:|---:|---:|
| X11 | 30 s | 914 [908–919] | 917 [916–917] | 887 [880–892] | -30 |
| X11 | 90 s | 963 [942–979] | 997 [997–1037] | 886 [879–891] | -111 |
| X11 | 5 min | 950 [941–968] | 974 [965–992] | 951 [949–959] | -23 |
| X11 | 15 min | 947 [926–971] | 989 [950–996] | 1014 [968–1025] | +25 |
| Wayland | 30 s | 923 [905–926] | 931 [919–937] | 885 [874–899] | -46 |
| Wayland | 90 s | 984 [958–995] | 1063 [1013–1072] | 885 [873–899] | -179 |
| Wayland | 5 min | 978 [962–994] | 991 [985–1008] | 962 [953–980] | -30 |
| Wayland | 15 min | 969 [948–971] | 968 [968–973] | 1020 [978–1029] | +52 |

**"Usada" do free** (MiB, mediana [mín–máx])

| Sessão | t | Antes (linha de base, N=5) | Controle do mesmo dia (imagem 01, N=3) | Depois (imagem 02, N=5) | Depois − controle |
|---|---:|---:|---:|---:|---:|
| X11 | 30 s | 451 [445–455] | 454 [453–454] | 424 [417–429] | -30 |
| X11 | 90 s | 499 [478–514] | 533 [532–573] | 423 [416–427] | -110 |
| X11 | 5 min | 485 [476–503] | 509 [500–527] | 487 [485–495] | -22 |
| X11 | 15 min | 482 [460–505] | 524 [485–531] | 549 [503–560] | +25 |
| Wayland | 30 s | 447 [429–450] | 455 [443–461] | 409 [398–423] | -45 |
| Wayland | 90 s | 507 [481–518] | 586 [536–595] | 408 [396–422] | -178 |
| Wayland | 5 min | 500 [484–516] | 514 [507–530] | 485 [476–503] | -29 |
| Wayland | 15 min | 491 [470–493] | 490 [490–495] | 542 [500–551] | +52 |

**PSS de todos os processos** (MiB, mediana [mín–máx])

| Sessão | t | Antes (linha de base, N=5) | Controle do mesmo dia (imagem 01, N=3) | Depois (imagem 02, N=5) | Depois − controle |
|---|---:|---:|---:|---:|---:|
| X11 | 30 s | 427 [421–433] | 432 [426–432] | 420 [410–421] | -11 |
| X11 | 90 s | 431 [420–431] | 491 [446–496] | 404 [396–405] | -87 |
| X11 | 5 min | 416 [405–416] | 416 [415–421] | 404 [394–404] | -12 |
| X11 | 15 min | 443 [435–445] | 448 [442–450] | 443 [435–459] | -5 |
| Wayland | 30 s | 459 [459–460] | 460 [459–460] | 447 [446–448] | -13 |
| Wayland | 90 s | 465 [464–465] | 525 [525–526] | 434 [434–436] | -91 |
| Wayland | 5 min | 450 [449–459] | 450 [450–460] | 434 [433–435] | -16 |
| Wayland | 15 min | 476 [475–477] | 476 [476–477] | 468 [467–469] | -8 |

**PSS da sessão** (MiB, mediana [mín–máx])

| Sessão | t | Antes (linha de base, N=5) | Controle do mesmo dia (imagem 01, N=3) | Depois (imagem 02, N=5) | Depois − controle |
|---|---:|---:|---:|---:|---:|
| X11 | 30 s | 258 [250–264] | 264 [257–264] | 261 [247–261] | -3 |
| X11 | 90 s | 265 [251–265] | 265 [265–271] | 246 [235–246] | -20 |
| X11 | 5 min | 249 [235–249] | 249 [249–255] | 246 [235–247] | -3 |
| X11 | 15 min | 260 [247–261] | 261 [261–267] | 261 [254–262] | +1 |
| Wayland | 30 s | 362 [361–362] | 362 [362–362] | 359 [358–359] | -4 |
| Wayland | 90 s | 368 [367–368] | 368 [368–369] | 346 [345–346] | -23 |
| Wayland | 5 min | 353 [352–353] | 353 [353–353] | 346 [345–347] | -7 |
| Wayland | 15 min | 379 [379–379] | 379 [379–380] | 380 [380–380] | +1 |

### Login (s, mediana [mín–máx])

| Sessão | Antes | Controle | Depois |
|---|---:|---:|---:|
| X11: login → área de trabalho pronta (fila do `systemd --user` vazia) | 11.7 [11.2–12.0] | 12.8 [12.1–13.6] | **2.7 [2.1–2.9]** |
| Wayland: login → área de trabalho pronta | 12.1 [11.9–12.5] | 13.1 [12.5–13.2] | **2.9 [2.1–2.9]** |
| X11: CPU ocupada na janela depois da amostra de 30 s | 2.7 % | — | 0.5 % |
| Wayland: CPU ocupada na janela depois da amostra de 30 s | 1.6 % | — | 0.2 % |

### Leitura

- **Começo da sessão (30–90 s): o maior ganho.** Sem o pamac atualizando o
  banco no login, sem o Python + GTK 4 do biglinux-snapshot-detect e sem o
  ModemManager, a RAM usada aos 90 s cai 111 MiB no X11 e 179 MiB no
  Wayland, e o PSS de todos os processos cai 87 e 91 MiB, contra o
  controle.
- **Repouso (5 min):** −12 MiB (X11) e −16 MiB (Wayland) de PSS de todos os
  processos. Os ganhos permanentes são o ModemManager (−6,3 MiB) e o
  journald com menos registros (−2,6 MiB). O pamac-tray (4,1 MiB) só não
  aparece porque ainda está esperando: não conta como economia.
- **15 min:** a RAM usada fica +25 MiB (X11) e +52 MiB (Wayland) acima do
  controle, com o PSS 5–8 MiB abaixo. A diferença está na memória do kernel
  fora das categorias do meminfo (140–195 MiB nesta VM). A verificação do
  pamac agora acontece ~4,5 min antes dessa amostra. No controle ela foi no
  primeiro minuto, e aos 15 min essa memória já tinha voltado. **Não medido:**
  se ela volta ao nível do controle mais tarde (30 min ou mais). Fica para
  a Etapa H.
- Nenhuma unidade com falha (sistema e usuário) nos 10 boots. O ModemManager
  não aparece em nenhuma das 40 amostras; no controle, em todas.

### Limitação da imagem dourada

O `/etc/pacman.conf` instalado pela ISO do run 40585944672 não tem os
repositórios `community-*` e `biglinux-*`. O perfil wmaker tem uma cópia
própria do pacman.conf, e o acréscimo do motor ao `shared/pacman.conf` não
chegava a ela; corrigido no build-iso `87df1ce`. A memória medida não muda
por isso, mas a atualização do banco pelo pamac (o transiente acima) lê
menos repositórios do que num sistema corrigido.

## Método

### VM de benchmark (imagem dourada)

1. **ISO:** `bigcommunity_wmaker_2026-10-27_k618.iso`, artefato
   `iso-40585944672` do run
   [40585944672](https://github.com/big-windowmaker/build-iso/actions/runs/40585944672)
   do build-iso (`263a576`; perfis `big-windowmaker/iso-profiles@e0534a4`;
   motor `biglinux/iso-profiles@8ec5ad1`). sha256 do zip conferido com o
   digest da API.
2. **Instalação real**, pelo próprio teste do projeto:
   `build-iso/scripts/install-test.sh --plan bios --keep-disk` (openQA +
   Calamares, "Apagar disco", usuário `openqa` com senha aleatória, login
   no wdm, atualização `pacman -Syu`, sessão Wayland). APROVADO, 12 de 14
   módulos. A "falha leve" da sessão Wayland era só `xwayland=0`, esperado
   com `--lazy_xwayland`. A nova tentativa do teste gravou
   `WLR_RENDERER=pixman` em `/etc/environment`. A linha foi removida na
   preparação, para ficar o padrão.
3. **`00-instalado.qcow2`**: o disco instalado, convertido
   (sha256 `356ba2cf…`, 5,9 GiB).
4. **`01-main-f65e26d.qcow2`** (overlay sobre a 00, sha256 `1cace3ac…`):
   `tests/perf/bigwm-perf-vm.py prepare`. Instala o `qemu-guest-agent`
   (11.1.1-1) pelo console hvc0. Aplica os pacotes do big-wm **main
   f65e26d** (artefato `big-wm-packages` do run 40596971284:
   big-wm-session/-wayland/-themes/-tabbed-dock 26.10.27-2246) com
   `pacman -U`. A ISO já tinha os demais pacotes do main (windowmaker
   0.96.0.r130.g558e67f-5, wlmaker 1:0.8.r80.g7481afe-12, wdm 1.28-14,
   big-dockapps 20260412-4, gtk-nocsd 4.8-1). Faz um login de aquecimento em
   cada sessão (primeiro login depois da atualização: `big-wm-firstrun`,
   caches) e fica somente leitura. Lista completa:
   `resultados/2026-10-28-antes/pacman-Q.txt` (1480 pacotes; kernel
   linux618 6.18.49-1, systemd 261.2, xorg-server 21.1.24, pipewire 1.6.8,
   mesa 26.2.2).
5. **Cada boot** cria um overlay novo sobre a 01 e o apaga no fim. Todo boot
   parte do mesmo estado.

VM: QEMU 11.1.1 + KVM, `-machine pc` (a do teste de instalação BIOS),
`-cpu host`, **2 vCPUs, 4096 MiB**, virtio-blk, **virtio-vga 1920x1080 sem
3D** (Mesa llvmpipe no Xorg, pixman/llvmpipe no wlmaker), intel-hda sem
saída de áudio, virtio-net (slirp), virtio-rng, qemu-xhci + usb-tablet,
sem console serial nos boots medidos. Host: AMD Ryzen 7 5700G (16 threads),
kernel 7.2.12-1-big. Duas VMs por vez (uma X11 e uma Wayland). Locale
pt_BR.UTF-8, teclado br-abnt2.

### Login e amostras

`tests/perf/bigwm-perf-vm.py boot`:

1. sobe a VM e espera o `qemu-guest-agent`;
2. copia o coletor e grava a sessão lembrada do wdm
   (`/var/lib/wdm/sessions/openqa`: `x11/big-wm` ou
   `wayland/big-wm-wayland`);
3. inicia o coletor (`bigwm-perf-collect.py run`, root, pelo agente);
4. espera o greeter (`wdmLogin`) e entra **pelo teclado** (QMP `send-key`:
   Enter, senha, Enter, igual ao openQA);
5. o coletor mede o login e tira as amostras a **30 s, 90 s, 5 min e 15 min
   depois da abertura da sessão no logind**. Cada amostra é seguida de uma
   janela de CPU de 30 s;
6. ao fim, o inventário. O host não consulta o convidado entre o login e o
   fim das amostras.

### O que cada amostra guarda (`sample-<t>.json`)

- `/proc/meminfo` inteiro, lido antes de qualquer outra coisa;
  `/proc/pressure/{cpu,memory,io}`; `/proc/swaps`; zram (`mm_stat`).
- Por processo: comm, cmdline, UID, PPID, cgroup v2,
  `/proc/<pid>/smaps_rollup` (Rss, Pss, Pss_Anon/File/Shmem, Private/Shared,
  Swap, SwapPss; USS = Private_Clean + Private_Dirty).
- Na janela de 30 s: deltas de `/proc/stat` (CPU, `ctxt`, `intr`), de
  `/proc/interrupts` e, por processo, CPU (utime + stime) e trocas de
  contexto (voluntárias + involuntárias, o indicador de *wakeups* usado
  aqui; `perf`/powertop não estão instalados), mais os processos criados
  durante a janela.
- Se o XWayland está rodando e o tipo de sessão no logind.

### Atribuição dos processos

- **sessão**: cgroup em `/user.slice/user-<UID>.slice/`. Inclui
  `user@UID.service` inteiro (systemd --user, D-Bus, autostart, portais,
  PipeWire) e as `session-N.scope` (startbigwm, wmaker/wlmaker, filhos).
- **servidor gráfico**: Xorg, que o wdm inicia como root fora da sessão.
  Aparece à parte e somado em "sessão + Xorg".
- **ferramenta**: cgroup `qemu-guest-agent.service` (o agente e o coletor)
  ou unidades `bigwm-perf*`. Fica fora de todos os PSS e aparece à parte
  (10–11 MiB). O kernel não separa: ela **está** dentro da RAM usada.
- **sistema**: todo o resto com espaço de usuário (inclui o wdm).
- **kernel**: threads do kernel (sem memória própria em `smaps_rollup`).

Diferenças para a medição antiga de [memoria.md](memoria.md): lá a sessão
era "processos com UID 1000" numa sessão live, depois de `drop_caches`.
Aqui é o cgroup, no sistema instalado, sem `drop_caches`.

### Reproduzir

```sh
# 1. instalação (no build-iso; ~15 min com KVM)
scripts/install-test.sh --iso bigcommunity_wmaker_2026-10-27_k618.iso --plan bios \
    --out ~/.cache/bigwm-perf/install-bios --keep-disk
qemu-img convert -O qcow2 ~/.cache/bigwm-perf/install-bios/results/raid/hd0 00-instalado.qcow2
cp ~/.cache/bigwm-perf/install-bios/secret/test-password ~/.cache/bigwm-perf/test-password

# 2. imagem medida (pacotes do main)
tests/perf/bigwm-perf-vm.py prepare --base 00-instalado.qcow2 --image 01-main.qcow2 \
    --out prep --password-file ~/.cache/bigwm-perf/test-password \
    --packages big-wm-packages/*.pkg.tar.zst \
    --setup "sed -i '/^WLR_RENDERER=pixman\$/d' /etc/environment"

# 3. N rodadas (cada uma: um boot X11 e um Wayland em paralelo)
tests/perf/bigwm-perf-run.sh 01-main.qcow2 resultados padrao 5
tests/perf/bigwm-perf-run.sh 01-main.qcow2 resultados dock-classico 5 \
    --setup "setpriv --reuid=openqa --regid=openqa --init-groups env -i HOME=/home/openqa PATH=/usr/bin big-wm-tabbed-dock --dock-mode=classic"

# 4. resumo, custo por função, ciclos
tests/perf/bigwm-perf-summary.py resultados --out sumario
tests/perf/bigwm-perf-groups.py resultados padrao x11 300
tests/perf/bigwm-perf-vm.py cycles --image 01-main.qcow2 --session x11 --out ciclos-x11 \
    --password-file ~/.cache/bigwm-perf/test-password --cycles 4
tests/perf/bigwm-perf-cycles.py ciclos-x11
```

Ajustes de perfil vão em `--setup`, como root antes do login. Comandos do
usuário rodam por `setpriv`, sem PAM: nenhum `systemd --user` sobe antes do
login medido.

## Não medido (e por quê)

- **GPU real / virgl:** a VM não tem 3D. O Xorg (glamor + llvmpipe) e o
  wlmaker usam renderização por software, que pesa no PSS deles. Com GPU
  real os números mudam: medir em hardware na Etapa H.
- **Wakeups do kernel por `perf`/powertop:** não instalados na imagem, e
  instalá-los mudaria o sistema medido. Usei trocas de contexto por processo
  e `/proc/interrupts`.
- **Hardware opcional (Bluetooth, impressora):** a VM não tem. O
  blueman-applet e o applet de impressão ficaram cortados pelas condições
  do big-wm, como esperado.
- **Outras resoluções, HiDPI, 2 monitores, 1/2/8 GiB, UEFI:** Etapa H.
- **Dock com abas sozinho (`tabbed`) e nenhum dock (`none`):** só foram
  medidos `both` (padrão) e `classic`.
