#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-run.sh - série de boots medidos de um perfil (host).
#
#   bigwm-perf-run.sh IMAGEM.qcow2 RESULTADOS PERFIL N [--setup 'cmd root']...
#
# Faz N rodadas; em cada uma sobe dois boots ao mesmo tempo, um X11 e um
# Wayland (no máximo duas VMs de 4 GiB por vez), cada um num overlay novo
# sobre a mesma IMAGEM. Saída em RESULTADOS/PERFIL/{x11,wayland}-<n>/.
# Variável PERF_PASSWORD_FILE: senha da conta de teste (padrão
# ~/.cache/bigwm-perf/test-password). PERF_SESSIONS="x11 wayland" (padrão).
set -euo pipefail

image=${1:?imagem} results=${2:?resultados} profile=${3:?perfil} n=${4:?N}
shift 4
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
pw=${PERF_PASSWORD_FILE:-$HOME/.cache/bigwm-perf/test-password}
sessions=${PERF_SESSIONS:-x11 wayland}

for ((i = 1; i <= n; i++)); do
	pids=()
	for s in $sessions; do
		out="$results/$profile/$s-$i"
		if [[ -f "$out/guest/done" ]]; then
			echo "já medido: $out"
			continue
		fi
		rm -rf -- "$out"
		mkdir -p -- "$out"
		python3 "$here/bigwm-perf-vm.py" boot --image "$image" --session "$s" --out "$out" \
			--label "$profile-$s-$i" --password-file "$pw" "$@" >"$out/driver.log" 2>&1 &
		pids+=($!)
		sleep 5
	done
	for p in "${pids[@]}"; do
		wait "$p" || echo "boot falhou (pid $p): ver driver.log"
	done
	echo "rodada $i/$n de $profile concluída"
done
