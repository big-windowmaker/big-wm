#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-alloc-children.sh DIR: com um perfil de alocador ativo
# (bigwm-malloc-wrapper), confere que só o compositor o carregou e que os
# filhos dele (wlmdock, dockapps, programas do Dock) nasceram com o malloc da
# glibc: sem LD_PRELOAD/GLIBC_TUNABLES no ambiente e sem a biblioteca no
# /proc/PID/maps. Uma linha por processo em DIR/filhos.log; "fim N/M".
set -u
log=${1:?uso: bigwm-perf-alloc-children.sh DIR}/filhos.log
mkdir -p "$(dirname "$log")"
: >"$log"
re='lib(mimalloc|jemalloc|tcmalloc_minimal|snmallocshim|clang_rt\.scudo)'
ok=0 n=0
comps=" $(pgrep -x wlmaker | tr '\n' ' ') $(pgrep -f '^(/usr/bin/)?wmaker --for-real' | tr '\n' ' ') "
for c in $comps; do
	lib=$(grep -oE "${re}[^ ]*" "/proc/$c/maps" | sort -u | tr '\n' ' ')
	echo "compositor $c $(tr '\0' ' ' <"/proc/$c/cmdline"): ${lib:-glibc}" >>"$log"
done
# Todos os outros processos (o wlmaker inicia os programas desacoplados: os
# filhos diretos não bastam). O lançador "wmaker" (sem --for-real) repassa o
# preload ao WM de propósito e é só registrado.
for d in /proc/[0-9]*; do
	p=${d#/proc/}
	case "$comps" in *" $p "*) continue ;; esac
	[ "$p" = $$ ] && continue
	comm=$(cat "$d/comm" 2>/dev/null) || continue
	# Só alocador conta: a sessão X11 já tem LD_PRELOAD=libgtk-nocsd.so.0.
	env=$(tr '\0' '\n' <"$d/environ" 2>/dev/null | grep -E "^LD_PRELOAD=.*$re|^GLIBC_TUNABLES=.*malloc" | tr '\n' ' ')
	lib=$(grep -oE "${re}[^ ]*" "$d/maps" 2>/dev/null | sort -u | tr '\n' ' ')
	n=$((n + 1))
	if [ -z "$env$lib" ]; then
		ok=$((ok + 1))
	elif [ "$comm" = wmaker ]; then
		ok=$((ok + 1))
		echo "lançador $p wmaker: ${env}${lib}" >>"$log"
	else
		echo "FALHA $p $comm: ${env}${lib}" >>"$log"
	fi
done
echo "processos conferidos (fora o compositor): $n" >>"$log"
echo "fim $ok/$n" >>"$log"
