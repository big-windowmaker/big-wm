# 04 — Validação no Virt-Manager

VM do Virt-Manager (libvirt `qemu:///system`, `bwm-clip-grade`): r57
instalada pelo Calamares, 4 vCPUs, virtio-vga, com o wlmaker -8 e os
pacotes `big-wm-session`/`big-wm-wayland` desta rodada (mic-tray e
kwin-screenshot em C). Perfil de memória da glibc.

`tests/perf/bigwm-perf-virt-matrix.sh`: por célula, desliga a VM, muda
memória (`virsh setmaxmem/setmem --config`) e resolução preferida do
virtio-vga (`<resolution>` no XML), liga, entra pelo wdm com o teclado do
virsh (`bigwm-perf-libvirt.sh`), coleta em 30, 90 e 300 s e roda
`bigwm-perf-funcional.sh` 150 s depois do login. O XML original foi
guardado antes e devolvido no fim (conferido: idêntico).

## Resultados (amostra de 300 s, 1 boot por célula)

| RAM | Resolução | Sessão | RAM usada | MemAvailable | PSS sessão | compositor | Xorg / Xwayland | swap | funcional |
|---|---|---|---:|---:|---:|---:|---:|---:|---|
| 2 GiB | 1920x1080 | X11 | 741 | 1219 | 298 | 10,1 | 78,0 | 0 | 21/21* |
| 2 GiB | 1920x1080 | Wayland | 721 | 1239 | 338 | 25,0 | 23,6 | 0 | 21/21 |
| 4 GiB | 1024x768 | X11 | 931 | 2978 | 298 | 10,1 | 56,1 | 0 | 21/21 |
| 4 GiB | 1024x768 | Wayland | 933 | 2975 | 330 | 19,9 | 23,7 | 0 | 21/21 |
| 4 GiB | 1366x768 | X11 | 923 | 2985 | 279 | 10,1 | 63,2 | 0 | 21/21 |
| 4 GiB | 1366x768 | Wayland | 956 | 2952 | 331 | 21,1 | 23,7 | 0 | 21/21 |
| 4 GiB | 1920x1080 | X11 | 971 | 2937 | 295 | 10,1 | 78,7 | 0 | 21/21 |
| 4 GiB | 1920x1080 | Wayland | 977 | 2931 | 336 | 25,1 | 23,7 | 0 | 21/21 |
| 4 GiB | 2560x1440 | X11 | 1016 | 2893 | 299 | 10,1 | 103,4 | 0 | 21/21 |
| 4 GiB | 2560x1440 | Wayland | 993 | 2915 | 342 | 31,1 | 23,7 | 0 | 21/21 |
| 8 GiB | 1920x1080 | X11 | 1487 | 6447 | 309 | 10,1 | 76,5 | 0 | 21/21 |
| 8 GiB | 1920x1080 | Wayland | 1527 | 6407 | 337 | 25,0 | 23,7 | 0 | 21/21 |
| 16 GiB | 1920x1080 | X11 | 2264 | 13722 | 299 | 10,0 | 77,7 | 0 | 21/21 |
| 16 GiB | 1920x1080 | Wayland | 2361 | 13625 | 336 | 25,1 | 23,7 | 0 | 21/21 |

MiB. \* A primeira célula registrou 20/21 por um erro do detector (o
processo era `/usr/bin/wmaker --for-real`, com caminho); corrigido o
padrão, a mesma sessão deu 21/21.

Leitura:
- A sessão ocupa o mesmo com 2 ou 16 GiB (~300 MiB no X11, ~336 no
  Wayland). O que cresce com a RAM é a reserva do kernel (marcas d'água e
  `min_free_kbytes` proporcionais à memória, A1): "RAM usada" vai de 0,7 a
  2,3 GiB sem nenhum processo mudar.
- Com 2 GiB sobra 1,2 GiB disponível em repouso, sem swap.
- A resolução pesa no servidor gráfico: Xorg 56 → 103 MiB e wlmaker 19,9 →
  31,1 MiB de 1024x768 a 2560x1440 (buffers da tela). O Xwayland
  (rootless, sob demanda) fica em 23,7 MiB.
- 1 boot por célula: serve para "funciona e quanto ocupa", não para
  diferenças de 1–2 MiB.

## As 21 verificações (`bigwm-perf-funcional.sh`)

Sessão gráfica ativa; compositor; Dock; Clip; bandeja (snixembed +
wmsystemtray / big-wm-tray); dockapps (wmcliphist, wmpulsemixer,
controlador do Dock com Abas); item do microfone; StatusNotifierWatcher;
notificação entregue; xdg-desktop-portal; agente do polkit; áudio
(PipeWire, WirePlumber, `pactl info`); área de transferência ida e volta
(xsel / wl-copy); captura de tela (scrot / grim); janela nova aparece (X11:
`_NET_CLIENT_LIST`; Wayland: nativa e pelo XWayland); xsettingsd (X11);
menu raiz; zram ativo; nenhum coredump; nenhuma unidade do usuário com
falha; nenhuma unidade do sistema com falha.

Também 21/21 nas duas sessões do OptiPlex 760 ([09](09-hardware-legado.md)).

## Não feito

Arrastar ícones, menus e atalhos de teclado nesta matriz (verificados à mão
na rodada do Clip em grade, `docs/dock-virt-manager-validation.md` do PR
#54); GPU 3D (VirGL) e QXL.
