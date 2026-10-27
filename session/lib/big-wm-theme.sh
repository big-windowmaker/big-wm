# shellcheck shell=bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Modo claro ou escuro do Big WindowMaker (big-wm-theme): funções comuns ao
# big-wm-env.sh, ao big-wm-open e ao próprio big-wm-theme. Carregado com ".".
#
# A escolha fica em ~/.config/big-wm/theme ("light" ou "dark"); sem o
# arquivo, vale o modo claro, a aparência original do Big WindowMaker.

big_wm_theme_file() {
	echo "${XDG_CONFIG_HOME:-$HOME/.config}/big-wm/theme"
}

# Imprime light ou dark.
big_wm_theme() {
	local mode
	mode=$(cat "$(big_wm_theme_file)" 2>/dev/null)
	if [ "$mode" = dark ]; then
		echo dark
	else
		echo light
	fi
}

# Tema GTK do modo escolhido: OneStepBack ou OneStepBack-Dark (o claro
# quando o pacote ainda não tem a variação escura).
big_wm_gtk_theme() {
	local themes=${BIGWM_THEMESDIR:-/usr/share/themes}
	if [ "$(big_wm_theme)" = dark ] && [ -d "$themes/OneStepBack-Dark/gtk-3.0" ]; then
		echo OneStepBack-Dark
	else
		echo OneStepBack
	fi
}

# Numa sessão já aberta, o GTK_THEME herdado do Window Maker ou do wlmaker é
# o do login. Quem abre aplicativos (big-wm-open) chama esta função para que
# eles já saiam no modo escolhido depois. Só troca entre o par claro/escuro
# do Big WindowMaker: outro tema escolhido pelo usuário fica.
big_wm_theme_refresh_env() {
	case ${GTK_THEME:-} in
	OneStepBack | OneStepBack-Dark)
		GTK_THEME=$(big_wm_gtk_theme)
		export GTK_THEME
		;;
	esac
}
