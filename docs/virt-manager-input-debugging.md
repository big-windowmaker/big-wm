# Clique do mouse fechava o virt-manager (2026-11-13)

**Sintoma.** Com a ISO no virt-manager, um clique na lista de idiomas, na
tela de login ou no Calamares fechava a janela da VM e, às vezes, o próprio
virt-manager. Com o teclado, tudo funcionava.

## Qual camada caía

O problema estava no **host**, não no convidado:

| Evidência | Onde |
|---|---|
| 10 quedas do `python3 /usr/bin/virt-manager` com SIGSEGV desde 12/11 (6 em 13/11) | `coredumpctl list` |
| `segfault at 40 ... in libwayland-client.so.0.26.0[5284]`, em todas | `journalctl -k` |
| O deslocamento 0x5284 é `wl_proxy_get_version+4`: leitura de um campo de um objeto Wayland **NULL** | `nm -D` da libwayland-client |
| O virt-manager do host roda como cliente Wayland nativo do **wlmaker** (a sessão do usuário) | `XDG_SESSION_TYPE=wayland` |

## Causa (comprovada)

1. Quando o convidado não usa um ponteiro absoluto, o SPICE fica no modo
   de mouse "server", e um clique na tela da VM **captura o ponteiro**.
2. O spice-gtk 0.42 (`src/wayland-extensions.c`,
   `spice_wayland_extensions_lock_pointer`) chama
   `zwp_pointer_constraints_v1_lock_pointer(pointer_constraints, ...)` sem
   conferir se o global existe.
3. O wlmaker não oferecia `zwp_pointer_constraints_v1`. O ponteiro do
   global era NULL e o virt-manager inteiro caía junto (todas as janelas são
   um processo só).

## Correção

Patch 0149 do wlmaker (`packages/wlmaker`, wlmaker 36): o protocolo
`pointer-constraints-unstable-v1` pelo wlroots
(`wlr_pointer_constraints_v1`).

- A trava fica ativa enquanto a superfície dela tem o foco do ponteiro e
  acompanha as trocas de foco.
- Com o ponteiro **travado**, o cursor não se move, mas o movimento relativo
  continua indo ao cliente.
- Com o ponteiro **confinado**, o movimento que sairia da região é retido.
- A trava é desativada antes de desconectar, porque uma trava "oneshot" é
  destruída na desativação.

A correção ficou no compositor e não no spice-gtk, que é um pacote de
terceiros. Isso também atende jogos e outros visualizadores (VNC, QEMU GTK)
que travam o ponteiro.

## Provas

| Teste | wlmaker 35 | wlmaker 36 |
|---|---|---|
| Sonda que trava o ponteiro como o spice-gtk (global, `locked`, sem movimento travado, movimento volta depois) | global ausente (sai 3) | PASSOU: `locked`, 0 movimentos travado, 15 depois de soltar |
| `spicy` (spice-gtk) no wlmaker aninhado, contra uma VM QEMU descartável sem tablet (modo "server"), cliques na tela | **caiu (status 139)** | vivo; Ctrl+Alt solta e continua vivo |
| virt-manager no host com a ISO | NÃO EXECUTADO na sessão real: o wlmaker 36 está instalado e vale no próximo login | — |

**Do lado do convidado (não muda a correção).** A VM do usuário tem
`usb-tablet`. O modo "server" aparece quando o convidado ainda não usa o
tablet ou quando o spice-vdagent não roda. A queda era do host em qualquer
caso.
