# Boot e serviços no OptiPlex 760 (disco rígido)

A tela de login passa a começar **6 s mais cedo** (wdm aos 13,9 s em vez
de 20,1 s; Xorg aos 17,1 s em vez de 20,7 s), com dois drop-ins da edição
(`iso-profiles`, `bigcommunity/wmaker/desktop-overlay`). Nada muda em
máquinas com NVIDIA nem no live.

## Resultado (3 reboots cada, mediana [mín–máx], s desde o boot)

| Marco | Antes | Depois |
|---|---:|---:|
| wdm iniciado | 20,1 [19,5–21,5] | 13,9 [13,7–14,3] |
| Xorg da tela de login (1ª linha no journal) | 20,7 [20,1–22,0] | 17,1 [15,9–19,5] |
| Login a frio → área pronta (s depois do Enter) | 8,7 [8,6–9,1] | 9,3 [8,1–9,8] |
| `graphical.target` | 27,6 [26,7–29,4] | 29,9 [29,8–30,8] |

O `graphical.target` termina mais tarde porque o `pacman-fix-keys` agora
cede o disco e demora mais; nem a tela de login nem o login esperam por
ele. O Xorg ganha menos que o wdm porque passa a subir disputando o disco
com o resto do boot.

## 1. `big-driver-manager-gpu` antes da tela de login

**Problema.** O wdm é ordenado depois desse serviço (`Before=display-
manager.service`), que roda `big-driver-manager-cli gpu boot`. A função
lista todos os pacotes (`pacman -Qq`) antes de olhar o hardware. A frio,
num HDD, terminava aos ~20 s; com cache, 0,35 s.

**Causa.** Sem placa NVIDIA e fora do live, o `gpu boot` não faz nada
(lido em `core/gpu_cli.py`: só age em modo híbrido, com o driver
proprietário sem placa, ou com o marcador `/livefs-pkgs.txt`).

**Correção.** `big-driver-manager-gpu.service.d/bigwm-so-com-nvidia.conf`:
`ExecCondition` com o mesmo teste (fabricante PCI `0x10de` ou o live).
Proposta para a BigLinux: verificar o hardware antes do `pacman -Qq`.

## 2. `auto-unlock-pacman` disputando o disco

`pacman-fix-keys` roda a cada boot (~10 s no HDD: `pacman-key --init` e
população das chaves). `auto-unlock-pacman.service.d/bigwm-sem-disputar-
o-disco.conf`: `Nice=19` e `IOSchedulingClass=idle`. Continua rodando a
cada boot.

## Medido e não mudado

- `NetworkManager-wait-online` (10,6 s) só segura o `graphical.target`
  pela cadeia smb → nmb → `network-online.target`; não segura a tela de
  login.
- `pamac-snapshot-maintenance` é um cliente D-Bus; quem trabalha é o
  broker do Pamac, já com pesos baixos no `pamac.slice`. Um drop-in nessa
  unidade não mudaria nada.
- Serviços residentes (Samba, CUPS, filtros do microfone): decisões A4,
  A5 e A12 em [../performance-decisions.md](../performance-decisions.md).

## Reverter

Apagar os dois diretórios `.service.d` em `/etc/systemd/system/` (ou no
overlay da edição) e `systemctl daemon-reload`.
