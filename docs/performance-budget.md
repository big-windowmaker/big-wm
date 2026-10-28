# Orçamento de memória

Teto de memória por subsistema do Big WindowMaker em repouso, para guiar as
próximas etapas e, mais tarde, um teste de regressão. Os números vêm do
harness de [performance-benchmarks.md](performance-benchmarks.md): sistema
instalado, VM de 4 GiB, 2 vCPUs e 1920x1080, **5 min depois do login**
(repouso, tela desbloqueada), **PSS em MiB**, mediana de ≥ 5 boots,
classificação de `tests/perf/bigwm-perf-groups.py`.

- **Antes**: linha de base de 2026-10-28 (big-wm `f65e26d`).
- **Etapa B/F**: imagem `02-sistema`, com os serviços do sistema já
  ajustados (ModemManager só com modem, pamac depois de 10 min,
  render-menu-options e biglinux-snapshot-detect sem segurar o login).
- **Orçamento**: o teto proposto. Coluna "Etapa" = quem cuida (ver
  [performance-decisions.md](performance-decisions.md)).

O orçamento é uma meta, não uma garantia. Onde o teto pede uma
mudança ainda não feita, a coluna "Como chegar" diz qual.

## Por subsistema

| Subsistema | X11 antes | X11 B/F | Wayland antes | Wayland B/F | Orçamento X11 / Wayland | Etapa | Como chegar |
|---|---:|---:|---:|---:|---:|---|---|
| Servidor gráfico e WM (Xorg + wmaker / wlmaker) | 82.3 | 82.4 | 108.6 | 108.9 | 80 / 85 | C / D | wlmaker: buffers do renderizador por software (A2) |
| Compositor X11 (picom + big-wm-x11-repaint) | 10.5 | 10.5 | — | — | 10 / — | C | manter |
| Dock e Clip Wayland (wlmdock) + papel de parede (swaybg) | — | — | 11.7 | 11.9 | — / 12 | D | manter |
| Dock com abas | 25.0 | 24.9 | 14.5 | 14.7 | 15 / 15 | E | GTK 3 mais leve no X11 (A6), cache de ícones (A10) |
| Dockapps | 4.3 | 4.3 | 13.4 | 13.7 | 5 / 12 | C / D | — |
| Bandeja (snixembed / big-wm-tray) | 21.8 | 21.8 | 33.0 | 32.8 | 15 / 20 | C / D | A6, A11 |
| Sincronia WPrefs → wlmaker (`--watch`) | — | — | 16.2 | 16.2 | — / 0 | D | vigia por unidade `.path` (A3) |
| Notificações (dunst) | 8.2 | 8.2 | 7.2 | 2.2 | 8 / 8 | — | manter |
| Autenticação (polkit-gnome + polkitd) | 19.2 | 19.2 | 10.6 | 10.6 | 12 / 11 | C | A6 |
| Rede (NetworkManager, nm-applet, ModemManager, dnsmasq) | 49.2 | 42.9 | 36.6 | 30.9 | 36 / 31 | B/F ✔, C | ModemManager sob demanda (feito, −6 MiB); nm-applet no X11 (A6) |
| Som (indicator-sound-switcher, PipeWire, WirePlumber, filtros do microfone) | 80.5 | 80.5 | 79.0 | 79.0 | 80 / 79 | B | manter; filtros sob demanda depende da BigLinux (A5) |
| Portais (xdg-desktop-portal*, documentos, permissões) | 13.8 | 13.8 | 16.1 | 16.2 | 16 / 16 | — | manter |
| Samba | 23.6 | 23.5 | 23.6 | 23.7 | 24 / 24 | BigLinux | só sob demanda com mudança no compartilhamento do Thunar (proposta) |
| Atualizações (pamac-tray; pamac-session e pamac-broker transitórios) | 4.1 | 0.7¹ | 4.1 | 0.5¹ | 5 / 5, sem pico nos primeiros 10 min | B/F ✔ | feito |
| Sistema básico (systemd, journald, udevd, logind, cron, grub-btrfsd...) | 38.3 | 35.6 | 37.7 | 35.2 | 38 / 38 | — | manter |
| Discos, GVfs, wdm, systemd --user, cupsd, at-spi, uresourced, bloqueio, outros | 35.0 | 35.2 | 37.1 | 37.4 | 35 / 37 | — | manter |
| **PSS de todos os processos** | **416** | **404** | **450** | **434** | **≤ 380 / ≤ 395** (soma dos tetos) | | |
| **PSS da sessão** (cgroup do usuário) | **249** | **246** | **353** | **346** | **≤ 215 / ≤ 300** | | |

¹ Aos 5 min só o `big-wm-pamac-tray` (sh + sleep, 0,5–0,7 MiB) está
esperando. O pamac-tray (4,1 MiB) sobe aos 10 min. É deslocamento no tempo,
não economia.

## Totais do sistema (métrica 1)

| | X11 antes | X11 B/F | Wayland antes | Wayland B/F | Orçamento |
|---|---:|---:|---:|---:|---:|
| RAM usada (MemTotal − MemAvailable), 5 min | 950 | 951 | 978 | 962 | ≤ 900 |
| "Usada" do `free` (sem cache e sem a reserva do kernel), 5 min | 485 | 487 | 500 | 485 | ≤ 440 |
| RAM usada no 1º minuto e meio (30 e 90 s) | 914 / 963 | 887 / 886 | 923 / 984 | 885 / 885 | sem pico acima do repouso |

A RAM usada inclui ~465 MiB de reserva do kernel. O
`vm.watermark_scale_factor = 150` do auto-rules-io e o `min_free_kbytes` de
3 % do biglinux-systemd-swap ficam (A1 rejeitado com teste de pressão). Por
isso o orçamento dessa métrica acompanha o PSS e não promete os 150–200 MiB
da missão: com os serviços completos da edição, nem o PSS de todos os
processos chega perto disso.

## Como conferir

```sh
tests/perf/bigwm-perf-run.sh IMAGEM.qcow2 resultados perfil 5
tests/perf/bigwm-perf-summary.py resultados --out sumario
tests/perf/bigwm-perf-groups.py resultados perfil x11 300
tests/perf/bigwm-perf-groups.py resultados perfil wayland 300
```

Uma mudança cabe no orçamento quando a mediana dos 5 boots fica no teto ou
abaixo dele. A margem para comparar duas medianas é a maior entre 5 MiB e
a faixa (mín–máx) dos boots da linha de base. Para um teste automático no
CI, 3 boots por sessão bastam para pegar regressões acima de ~15 MiB nesta
VM: a faixa do PSS de todos os processos foi de 1 a 11 MiB entre boots.
