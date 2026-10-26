# Laboratório de testes das sessões X11 e Wayland

Como reproduzir, numa VM, os testes de [wayland-parity.md](wayland-parity.md).
O CI cobre o que roda sem tela (`tests/run.sh`, `tests/integration/`); o
resto precisa de uma VM de verdade, com a tela capturada pelo libvirt.

## VM

QEMU/KVM pelo libvirt, virtio-gpu, USB tablet (ponteiro absoluto), 8 GB.
Instale a ISO (BIOS ou UEFI), atualize com os pacotes a testar e tire um
snapshot antes:

```sh
virsh -c qemu:///system snapshot-create-as "Big WindowMaker" antes-dos-testes
sudo pacman -U big-wm-*.pkg.tar.zst      # nunca biglinux-livecd no instalado
```

Para escolher a sessão sem autologin, defina a sessão padrão da conta e
entre pelo greeter (é o caminho do usuário; ver o item L2 da matriz):

```sh
sudo busctl call org.freedesktop.Accounts /org/freedesktop/Accounts/User1000 \
  org.freedesktop.Accounts.User SetXSession s big-wm-wayland   # ou big-wm
```

## Ver e operar a tela da VM

A tela real vem do libvirt; nada de captura sintética:

```sh
virsh -c qemu:///system screenshot "Big WindowMaker" tela.ppm
```

Teclado: `virsh send-key` com `--holdtime 80` (sem ele a tecla é solta no
mesmo instante, o que não acontece com uma pessoa). Mouse: eventos absolutos
do tablet USB pelo QMP, de 0 a 32767 em cada eixo:

```sh
q() { virsh -c qemu:///system qemu-monitor-command "Big WindowMaker" "$1"; }
q '{"execute":"input-send-event","arguments":{"events":[
     {"type":"abs","data":{"axis":"x","value":16383}},
     {"type":"abs","data":{"axis":"y","value":16383}}]}}'
q '{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":true,"button":"right"}}]}}'
q '{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":false,"button":"right"}}]}}'
```

Mova o ponteiro, espere um instante e só então clique: um clique que chega
junto com o movimento não gera o evento de entrada na janela.

## Rodar programas na sessão gráfica pelo SSH

O logind tem `KillUserProcesses=yes`: o que nasce na sessão SSH morre quando
ela termina. Rode com o ambiente da sessão gráfica e fora do escopo do SSH:

```sh
# ambiente: no X11, o do processo wmaker; no Wayland, o do systemd --user
# (o processo wlmaker não tem WAYLAND_DISPLAY: ele só o define nos filhos).
while IFS= read -r kv; do export "$kv"; done < <(systemctl --user show-environment)
setsid systemd-run --user --scope --quiet --collect -- gnome-calculator &
```

Cuidados:
- `pkill -f`/`pgrep -f` com um padrão que aparece na própria linha de
  comando do SSH derruba a conexão; use `-x` ou o PID.
- No sistema instalado o `sudo` pede senha: `sudo -S` lendo de um arquivo
  com permissão 600, nunca a senha na linha de comando.
- Vários `systemctl restart lightdm` seguidos no live disparam a proteção do
  `startbiglive` (tela preta, `sleep infinity`): apague
  `/tmp/startbiglive-attempts` antes.
- Não compile pacotes na VM durante testes visuais: o primeiro desenho dos
  aplicativos atrasa e parece defeito de renderização.

## Roteiro mínimo por sessão

1. Login pelo greeter; conferir `systemctl --user show-environment`.
2. Menu raiz (F12 e clique direito), Aplicativos > uma categoria > abrir um app.
3. Super+Espaço (rofi com o tema), Super+E, Ctrl+Alt+T.
4. Bandeja: clique no ícone da rede abre o menu do nm-applet.
5. Dockapps: relógio, CPU, memória e rede junto do Clip (Wayland) ou no Dock (X11).
6. Bloquear pelo menu, digitar a senha, desbloquear; `loginctl lock-session`.
7. Aparência > Papéis de parede; sair e entrar de novo: a escolha continua.
8. Memória: PSS da sessão como em [memoria.md](memoria.md).
