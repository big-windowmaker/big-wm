# Consumo de memória da sessão X11

Medições numa VM QEMU/KVM (8 GB, virtio-gpu sem 3D, 6 CPUs) com a ISO
`bigcommunity_wmaker_2026-10-26`, sessão live em pt_BR com tema escuro, 90 s
depois do login e com o cache de páginas descartado
(`echo 3 > /proc/sys/vm/drop_caches`). PSS é a memória de cada processo com
as bibliotecas compartilhadas divididas entre quem as usa; a soma de PSS não
conta nada duas vezes.

## Resultado

| | ISO original | Com as mudanças | Diferença |
|---|---|---|---|
| PSS da sessão (processos do usuário) | 277 MB | 191 MB | **−86 MB (−31%)** |
| PSS de todos os processos | 425 MB | 315 MB | −110 MB |
| AnonPages | 258 MB | 180 MB | −78 MB |
| Xorg | 87 MB | 61 MB | −26 MB |
| Aplicativo GTK 4 (gnome-calculator) em VM | 121 MB | 66 MB | −55 MB por aplicativo |

Os 191 MB foram medidos sem o indicator-sound-switcher, que voltou a iniciar
com a sessão (ver abaixo): conte cerca de 35 MB a mais para a sessão atual.

O Window Maker em si usa cerca de 14 MB (`wmaker` 3,7 MB + `wmaker
--for-real` 10,5 MB). O consumo vinha dos programas iniciados com ele.

## De onde vinha a memória

| Processo | PSS | O que foi feito |
|---|---|---|
| blueman-applet (Python + GTK) | 36 MB | só inicia com um adaptador Bluetooth |
| indicator-sound-switcher (Python + GTK) | 35 MB | **mantido** (desde 27/10): fica na bandeja das duas sessões e troca a saída e a entrada de som com um clique, o que o wmix não faz |
| applet da fila de impressão (Python + GTK) | 24 MB | só inicia com impressora configurada (`lpstat -v`) |
| big-preload-user | processo de 30 s + lsof | não inicia: lista arquivos do Plasma e do KWin |
| picom | 3 MB | não inicia na sessão Wayland (é compositor X11) |
| renderizador GL do GTK 4 em VM | +55 MB por app | cairo em VM (o GL carrega o LLVM do llvmpipe em cada app) |

Os cortes valem só nas sessões Big WindowMaker: são drop-ins do systemd
(`/usr/lib/systemd/user/app-*@autostart.service.d/big-wm.conf`) com um
`ExecCondition` que libera tudo quando `XDG_CURRENT_DESKTOP` não é
`WindowMaker` nem `wlmaker`. Plasma, XFCE e GNOME não mudam. As condições
ficam em `session/lib/big-wm-autostart-condition`.

Um Bluetooth ou uma impressora adicionados depois do login aparecem na
bandeja no próximo login; configurar a impressora (system-config-printer) e
parear dispositivos (blueman-manager) funcionam sem o applet.

## O que parece memória e não é

- **`/run/initramfs` (236 MB), só no live.** O motor de ISO do BigLinux
  guarda o initramfs descompactado para desmontar o squashfs ao desligar. O
  sistema instalado não tem o hook `shutdown` em `mkinitcpio.conf`.
- **Disco do live em RAM.** No live, `/` é um overlay em tmpfs: tudo o que é
  gravado ou instalado conta como `Shmem`.
- **Cache de páginas.** `buff/cache` é devolvido quando falta memória; o
  número que importa é `MemAvailable` (coluna *available* do `free`).
- **Kernel** (~145 MB: slab, vmalloc, pilhas e tabelas de páginas).

## Medido e mantido

- **picom:** o Xorg ocupa o mesmo com ou sem ele (86 MB).
- **glamor no Xorg em VM:** o LLVM e o gallium do llvmpipe somam 23 MB no
  Xorg. Desligar a aceleração só ajudaria em VM sem 3D e pioraria quem usa
  virgl ou GPU de verdade.
- **nm-applet, polkit-gnome, xdg-desktop-portal-gtk, pamac-tray, snixembed,
  dunst:** fazem parte do funcionamento da sessão (rede, senhas de
  administrador, portais, atualizações, bandeja de apps Qt/Electron,
  notificações).
- **indicator-sound-switcher (Sound Switcher Indicator):** cortado na
  medição de 26/10 e devolvido a pedido dos usuários: é o jeito de trocar a
  placa de saída e o microfone pela bandeja, nas duas sessões (no Wayland,
  pelo `big-wm-tray`).
- **Opções do Window Maker:** redimensionamento não opaco, animações rápidas
  e sem suavização do fundo já são o padrão.

## Como medir de novo

```sh
sudo sync; echo 3 | sudo tee /proc/sys/vm/drop_caches
sudo python3 - <<'EOF'
import os, re
tot = 0
for pid in filter(str.isdigit, os.listdir("/proc")):
    try:
        status = open(f"/proc/{pid}/status").read()
        if re.search(r"^Uid:\s+1000\b", status, re.M):
            tot += int(re.search(r"^Pss:\s+(\d+)", open(f"/proc/{pid}/smaps_rollup").read(), re.M)[1])
    except (OSError, TypeError):
        pass
print(f"PSS da sessão: {tot // 1024} MB")
EOF
```
