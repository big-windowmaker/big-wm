# shellcheck shell=bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Funções comuns dos testes de integração (carregado com ".").

results=$OUT/results.tsv

record() { # nome, PASS|FAIL|SKIP|XFAIL, detalhe
	printf '%s\t%s\t%s\n' "$1" "$2" "${3:-}" >>"$results"
	printf '[%s] %s %s\n' "$2" "$1" "${3:-}"
}

check() { # nome, detalhe, comando...
	local name=$1 detail=$2
	shift 2
	if "$@" >/dev/null 2>&1; then
		record "$name" PASS "$detail"
	else
		record "$name" FAIL "$detail"
	fi
}

# Espera até $1 segundos o comando dar certo.
wait_for() {
	local timeout=$1
	shift
	local end=$((SECONDS + timeout))
	while ((SECONDS < end)); do
		"$@" >/dev/null 2>&1 && return 0
		sleep 0.5
	done
	return 1
}

alive() { pgrep -u "$(id -u)" -x "$1" >/dev/null; }
