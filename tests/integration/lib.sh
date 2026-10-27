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

# O nome do processo (comm) tem no máximo 15 caracteres.
alive() { pgrep -u "$(id -u)" -x "${1:0:15}" >/dev/null; }

# Terminal padrão translúcido. Abre o AshyTerm com o ambiente da sessão
# (big-wm-env.sh: OneStepBack e gtk-nocsd) e compara a tela antes e depois:
# a maior área que mudou é a janela. Na metade de baixo dela, sem texto, um
# fundo opaco é liso e um translúcido mostra o papel de parede por trás.
# Argumentos: nome do teste, prefixo dos arquivos, função de captura (recebe
# o arquivo de saída).
check_translucent_terminal() {
	local name=$1 prefix=$2 shot=$3 box w h x y std
	if ! command -v ashyterm >/dev/null; then
		record "$name" FAIL "AshyTerm não instalado"
		return
	fi
	"$shot" "$prefix-before.png"
	(
		# shellcheck disable=SC1091 # instalado pelo big-wm-session
		. /usr/lib/big-wm/big-wm-env.sh
		big_wm_nocsd
		exec ashyterm
	) >"$prefix.log" 2>&1 &
	sleep 12
	"$shot" "$prefix.png"
	box=$(magick "$prefix-before.png" "$prefix.png" -compose difference -composite \
		-colorspace gray -threshold 3% -define connected-components:verbose=true \
		-connected-components 8 null: 2>/dev/null |
		awk '$NF ~ /255/ && $4 > max { max = $4; box = $2 } END { print box }')
	if [[ $box =~ ^([0-9]+)x([0-9]+)\+([0-9]+)\+([0-9]+)$ ]] && ((BASH_REMATCH[1] > 200 && BASH_REMATCH[2] > 200)); then
		w=${BASH_REMATCH[1]} h=${BASH_REMATCH[2]} x=${BASH_REMATCH[3]} y=${BASH_REMATCH[4]}
		# Longe das bordas: a caixa pode incluir sombra e restos de outras janelas.
		std=$(magick "$prefix.png" -crop "$((w * 3 / 4))x$((h / 3))+$((x + w / 8))+$((y + h / 2))" \
			-colorspace gray -format '%[fx:standard_deviation]' info:)
		if awk -v s="$std" 'BEGIN { exit !(s > 0.005) }'; then
			record "$name" PASS "fundo com o papel de parede (desvio $std, $prefix.png)"
		else
			record "$name" FAIL "fundo liso, opaco (desvio $std, $prefix.png)"
		fi
	else
		record "$name" FAIL "janela do AshyTerm não apareceu ($prefix.png)"
	fi
	pkill -u "$(id -u)" -x ashyterm
}
