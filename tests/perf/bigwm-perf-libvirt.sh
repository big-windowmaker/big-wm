#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-libvirt.sh - um boot medido numa VM libvirt já existente (por
# exemplo a VM de testes do usuário), por SSH + virsh send-key. Verificação
# cruzada: a referência reprodutível é bigwm-perf-vm.py (imagem dourada).
#
#   SSHPASS=... bigwm-perf-libvirt.sh DOMÍNIO USUÁRIO@IP x11|wayland SAÍDA
#
# SSHPASS: senha do usuário (também usada no sudo -S e digitada no greeter
# do wdm). Passos: reinicia a VM (sudo systemctl reboot), espera o SSH, envia
# o coletor, grava a sessão lembrada do wdm, inicia o coletor numa unidade
# systemd-run (bigwm-perf-collect: fica fora da sessão do usuário e é
# contado como "ferramenta"), fecha o SSH, espera 20 s (o systemd --user do
# login por SSH encerra) e entra pelo greeter com o teclado (virsh send-key).
# No fim, copia DIR/guest e grava virsh dominfo.
set -euo pipefail
dom=${1:?domínio} target=${2:?usuario@ip} session=${3:?x11|wayland} out=${4:?saída}
: "${SSHPASS:?defina SSHPASS}"
export SSHPASS
user=${target%@*}
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
virsh=(virsh -c qemu:///system)
ssh_opts=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=5)
gd=/var/tmp/bigwm-perf-$(date +%s)
mkdir -p "$out"
log() { printf '[libvirt %s] %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$out/driver.log"; }
rsh() { sshpass -e ssh "${ssh_opts[@]}" "$target" "$@"; }

case $session in
x11) sess=x11/big-wm ;;
wayland) sess=wayland/big-wm-wayland ;;
*) echo "sessão inválida" >&2; exit 2 ;;
esac

"${virsh[@]}" dominfo "$dom" >"$out/dominfo.txt"
log "reiniciando $dom"
rsh "printf '%s\n' '$SSHPASS' | sudo -S -p '' systemctl reboot" || true
sleep 20
for _ in $(seq 120); do rsh true 2>/dev/null && break; sleep 3; done
log "SSH de volta"
sshpass -e scp "${ssh_opts[@]}" "$here/bigwm-perf-collect.py" "$target:/tmp/bigwm-perf-collect.py"
rsh "printf '%s\n' '$SSHPASS' | sudo -S -p '' sh -c 'mkdir -p /var/lib/wdm/sessions && printf \"%s\\n\" $sess >/var/lib/wdm/sessions/$user && \
systemd-run --unit=bigwm-perf-collect --collect /usr/bin/python3 -I /tmp/bigwm-perf-collect.py run $user $gd'"
log "coletor iniciado; esperando o greeter"
for _ in $(seq 60); do rsh 'pgrep -x wdmLogin >/dev/null' && break; sleep 2; done
sleep 20
# Senha pelo teclado (US qcodes do virsh: KEY_*).
send() { "${virsh[@]}" send-key "$dom" "$@" >/dev/null; sleep 0.1; }
send KEY_ENTER
sleep 2
for ((i = 0; i < ${#SSHPASS}; i++)); do
	c=${SSHPASS:i:1}
	case $c in
	[a-z]) send "KEY_${c^^}" ;;
	[A-Z]) send KEY_LEFTSHIFT "KEY_$c" ;;
	[0-9]) send "KEY_$c" ;;
	'!') send KEY_LEFTSHIFT KEY_1 ;;
	'@') send KEY_LEFTSHIFT KEY_2 ;;
	'#') send KEY_LEFTSHIFT KEY_3 ;;
	'.') send KEY_DOT ;;
	'-') send KEY_MINUS ;;
	'_') send KEY_LEFTSHIFT KEY_MINUS ;;
	*) echo "caractere sem mapeamento na senha" >&2; exit 2 ;;
	esac
done
send KEY_ENTER
log "login enviado ($session)"
sleep 1000
for _ in $(seq 60); do rsh "test -e $gd/done" && break; sleep 10; done
rsh "printf '%s\n' '$SSHPASS' | sudo -S -p '' tar -C $gd -czf - ." >"$out/guest.tar.gz"
mkdir -p "$out/guest"
tar -C "$out/guest" -xzf "$out/guest.tar.gz"
rm -f "$out/guest.tar.gz"
"${virsh[@]}" screenshot "$dom" "$out/fim.png" >/dev/null 2>&1 || true
python3 -c 'import json,sys; json.dump({"session": sys.argv[1], "label": sys.argv[2], "done": open(sys.argv[3]).read().strip(), "vm": "libvirt " + sys.argv[4]}, open(sys.argv[5], "w"), indent=1)' \
	"$session" "$(basename "$out")" "$out/guest/done" "$dom" "$out/meta.json"
log "fim: $(cat "$out/guest/done")"
