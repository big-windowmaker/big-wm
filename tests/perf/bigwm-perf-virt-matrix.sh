#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-virt-matrix.sh DOMÍNIO USUÁRIO@IP RESULTADOS
# Matriz de memória e resolução numa VM do Virt-Manager (libvirt
# qemu:///system; docs/performance/2026-11-23-memoria/
# 04-validacao-virt-manager.md). Para cada célula RAM:RESOLUÇÃO:SESSÃO:
# desliga a VM, muda a memória (setmaxmem/setmem --config) e a resolução
# preferida do virtio-vga (<resolution> no XML), liga e mede com o
# bigwm-perf-libvirt.sh (amostras 30, 90 e 300 s + bigwm-perf-funcional.sh).
# Muda a configuração da VM: guarde o XML antes (virsh dumpxml) e devolva no
# fim (virsh define). SSHPASS: senha do usuário.
#   BIGWM_PERF_MATRIX="2048:1920x1080:x11 ..."
set -u
dom=${1:?domínio} target=${2:?usuario@ip} out=${3:?saída}
: "${SSHPASS:?defina SSHPASS}"
here=$(cd "$(dirname "$0")" && pwd)
virsh=(virsh -c qemu:///system)
default=
for ram in 2048 4096 8192 16384; do
	for s in x11 wayland; do default="$default $ram:1920x1080:$s"; done
done
for res in 1024x768 1366x768 2560x1440; do
	for s in x11 wayland; do default="$default 4096:$res:$s"; done
done
read -r -a cells <<<"${BIGWM_PERF_MATRIX:-$default}"
off() {
	[ "$("${virsh[@]}" domstate "$dom")" = "desligado" ] || [ "$("${virsh[@]}" domstate "$dom")" = "shut off" ] && return 0
	"${virsh[@]}" shutdown "$dom" >/dev/null
	for _ in $(seq 90); do
		case $("${virsh[@]}" domstate "$dom") in desligado | "shut off") return 0 ;; esac
		sleep 2
	done
	echo "$dom não desligou" >&2
	return 1
}
for c in "${cells[@]}"; do
	IFS=: read -r ram res s <<<"$c"
	d="$out/$ram-$res/$s"
	[ -f "$d/guest/done" ] && continue
	mkdir -p "$d"
	off || exit 1
	"${virsh[@]}" setmaxmem "$dom" "${ram}M" --config >/dev/null
	"${virsh[@]}" setmem "$dom" "${ram}M" --config >/dev/null
	xml=$(mktemp)
	"${virsh[@]}" dumpxml --inactive "$dom" | python3 -I -c '
import sys, xml.etree.ElementTree as ET
w, h = sys.argv[1].split("x")
t = ET.parse(sys.stdin)
for m in t.getroot().iter("model"):
    if m.get("type") == "virtio" and m.get("primary") == "yes":
        for r in m.findall("resolution"):
            m.remove(r)
        ET.SubElement(m, "resolution", x=w, y=h)
t.write(sys.stdout, encoding="unicode")' "$res" >"$xml"
	"${virsh[@]}" define "$xml" >/dev/null
	rm -f "$xml"
	"${virsh[@]}" start "$dom" >/dev/null
	for _ in $(seq 120); do sshpass -e ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
		-o LogLevel=ERROR -o ConnectTimeout=5 "$target" true 2>/dev/null && break; sleep 3; done
	BIGWM_PERF_SAMPLES=30,90,300 BIGWM_PERF_FUNCIONAL=1 "$here/bigwm-perf-libvirt.sh" "$dom" "$target" "$s" "$d" \
		>"$d.log" 2>&1
	echo "$(date +%T) $ram $res $s rc=$? $(grep funcional: "$d/driver.log" 2>/dev/null | tail -n 1)"
done
