#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-alloc-batch.sh IMAGEM SENHA RESULTADOS [RODADAS] [PARALELO]
# Benchmark de alocadores (docs/performance/02-metodologia-benchmark.md):
# para cada rodada, sessão e perfil (ordem dos perfis girada a cada rodada),
# um boot do bigwm-perf-vm.py com o perfil gravado em /etc/bigwm-malloc-profile
# (lido pelo wrapper experimental de wlmaker/wmaker da imagem) e a carga
# bigwm-perf-alloc-load.sh 150 s depois do boot. Resultados em
# RESULTADOS/<perfil>/<sessão>-<rodada>/, resumo pelo bigwm-perf-summary.py.
set -u
img=${1:?uso: IMAGEM SENHA RESULTADOS [RODADAS] [PARALELO]}
pw=${2:?}; out=${3:?}; rounds=${4:-5}; par=${5:-4}
here=$(cd "$(dirname "$0")" && pwd)
user=${BIGWM_PERF_USER:-ruscher}
read -r -a profiles <<<"${BIGWM_PERF_PROFILES:-glibc glibc-a2 mimalloc jemalloc tcmalloc}"
read -r -a sessions <<<"${BIGWM_PERF_SESSIONS:-wayland x11}"
load=$(base64 -w0 "$here/bigwm-perf-alloc-load.sh")
# Wrapper e biblioteca do experimento (gcc no host: a imagem não tem).
lib=$(mktemp -d)/libbigwm-malloc-once.so
gcc -O2 -shared -fPIC -o "$lib" "$here/bigwm-malloc-once.c" || exit 1
libb=$(base64 -w0 "$lib")
wrap=$(base64 -w0 "$here/bigwm-malloc-wrapper")
# Opcionais: BIGWM_PERF_PUSH="ORIGEM=DESTINO ..." (bibliotecas que a imagem
# não tem: snmalloc, Scudo), BIGWM_PERF_SAMPLES (padrão 30,90,300,600) e
# BIGWM_PERF_LOAD_REPEAT=S (repete a carga a cada S segundos: sessões
# longas).
samples=${BIGWM_PERF_SAMPLES:-30,90,300,600}
repeat=
[ -n "${BIGWM_PERF_LOAD_REPEAT:-}" ] && repeat="--on-unit-active=$BIGWM_PERF_LOAD_REPEAT"
push=()
for pair in ${BIGWM_PERF_PUSH:-}; do
	push+=(--setup "mkdir -p $(dirname "${pair#*=}")" --setup "@push ${pair%%=*} ${pair#*=}")
done
pushs=
[ ${#push[@]} -gt 0 ] && pushs=$(printf '%q ' "${push[@]}")
jobs=()
for r in $(seq 1 "$rounds"); do
	for s in "${sessions[@]}"; do
		k=${#profiles[@]}
		for i in $(seq 0 $((k - 1))); do
			p=${profiles[$(((i + r - 1) % k))]}
			[ -f "$out/$p/$s-$r/guest/done" ] && continue
			jobs+=("$p $s $r")
		done
	done
done
echo "$(date +%T) ${#jobs[@]} boots, $par em paralelo"
run_one() {
	set -- $1
	p=$1 s=$2 r=$3
	d="$out/$p/$s-$r"
	rm -rf "$d"; mkdir -p "$d"
	setup1="echo $p > /etc/bigwm-malloc-profile && echo $libb | base64 -d > /usr/local/lib/libbigwm-malloc-once.so && echo $wrap | base64 -d > /usr/local/bin/wlmaker && chmod 755 /usr/local/bin/wlmaker && cp /usr/local/bin/wlmaker /usr/local/bin/wmaker"
	setup2="echo $load | base64 -d > /usr/local/bin/bigwm-perf-alloc-load.sh && chmod 755 /usr/local/bin/bigwm-perf-alloc-load.sh && systemd-run --on-active=150 $repeat --unit=bigwm-perf-alloc-load /usr/local/bin/bigwm-perf-alloc-load.sh $user 40 /var/lib/bigwm-perf/$p"
	eval "extra=($pushs)"
	last=${samples##*,}
	timeout $((last + 900)) python3 "$here/bigwm-perf-vm.py" boot --image "$img" --session "$s" \
		--out "$d" --user "$user" --password-file "$pw" --label "$p" \
		--samples "$samples" "${extra[@]}" --setup "$setup1" --setup "$setup2" >"$d/driver.log" 2>&1
	echo "$(date +%T) $p $s $r rc=$? $(tail -1 "$d/driver.log")"
}
export -f run_one
export out img pw user here load libb wrap samples repeat pushs
printf '%s\n' "${jobs[@]}" | xargs -P "$par" -I{} bash -c 'run_one "$@"' _ {}
python3 "$here/bigwm-perf-summary.py" "$out"
