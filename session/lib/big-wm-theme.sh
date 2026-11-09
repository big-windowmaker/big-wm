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

# Tema GTK do modo escolhido (ou do modo dado): OneStepBack ou
# OneStepBack-Dark (o claro quando o pacote ainda não tem a variação escura).
big_wm_gtk_theme() {
	local themes=${BIGWM_THEMESDIR:-/usr/share/themes}
	if [ "${1:-$(big_wm_theme)}" = dark ] && [ -d "$themes/OneStepBack-Dark/gtk-3.0" ]; then
		echo OneStepBack-Dark
	else
		echo OneStepBack
	fi
}

# Nome estável do tema GTK das sessões (o GTK_THEME): um link do usuário,
# $XDG_DATA_HOME/themes/BigWindowMaker, aponta para o OneStepBack ou para o
# OneStepBack-Dark do modo escolhido (o GTK procura os temas primeiro ali). O
# nome não muda com o modo: o GTK_THEME herdado do Window Maker, do wlmaker,
# do systemd --user ou do D-Bus nunca fica velho, e quem abre um aplicativo
# depois da troca recebe o tema do modo atual. As janelas já abertas relêem
# o tema quando o XSETTINGS (X11), o gsettings (GTK 3 no Wayland) ou o
# color-scheme (libadwaita) mudam: big-wm-theme. Sem o link (o pacote não
# instalou o par, um diretório do usuário com o mesmo nome), o GTK_THEME é o
# nome do par, como antes. O /usr/share/themes/BigWindowMaker do pacote é o
# claro, para quem não tem o link (root, o instalador do live).
BIG_WM_GTK_THEME_NAME=BigWindowMaker

big_wm_gtk_theme_link_path() {
	echo "${XDG_DATA_HOME:-$HOME/.local/share}/themes/$BIG_WM_GTK_THEME_NAME"
}

# Cria ou acerta o link do modo atual, ou do modo dado (troca atômica:
# rename). Falha quando não há o tema do modo ou quando o caminho é do
# usuário (diretório próprio ou link para outro tema), que fica como está.
big_wm_gtk_theme_link() {
	local themes=${BIGWM_THEMESDIR:-/usr/share/themes} link want current
	link=$(big_wm_gtk_theme_link_path)
	want=$themes/$(big_wm_gtk_theme "${1:-}")
	[ -d "$want/gtk-3.0" ] || return 1
	if [ -L "$link" ]; then
		current=$(readlink "$link")
		[ "$current" = "$want" ] && return 0
		case ${current%/} in
		*/OneStepBack | */OneStepBack-Dark) ;;
		*) return 1 ;;
		esac
	elif [ -e "$link" ]; then
		return 1
	fi
	mkdir -p "${link%/*}" 2>/dev/null || return 1
	rm -f "$link.new.$$"
	ln -s "$want" "$link.new.$$" 2>/dev/null && mv -Tf "$link.new.$$" "$link" 2>/dev/null && return 0
	rm -f "$link.new.$$"
	return 1
}

# GTK_THEME da sessão: o nome estável quando o link está certo; senão, o
# nome do par do modo.
big_wm_session_gtk_theme() {
	if big_wm_gtk_theme_link; then
		echo "$BIG_WM_GTK_THEME_NAME"
	else
		big_wm_gtk_theme
	fi
}

# GTK_THEME gerenciado pelo Big WindowMaker (o nome estável ou um dos dois
# do par, de versões anteriores); outro valor é escolha do usuário.
big_wm_gtk_theme_managed() {
	case ${1:-} in
	OneStepBack | OneStepBack-Dark | "$BIG_WM_GTK_THEME_NAME") return 0 ;;
	esac
	return 1
}

# Numa sessão já aberta antes desta versão, o GTK_THEME herdado do Window
# Maker ou do wlmaker é um nome do par, o do login. Quem abre aplicativos
# (big-wm-open) chama esta função para que eles saiam no modo escolhido.
# Só troca o que é do Big WindowMaker: outro tema escolhido pelo usuário
# fica.
big_wm_theme_refresh_env() {
	if big_wm_gtk_theme_managed "${GTK_THEME:-}"; then
		GTK_THEME=$(big_wm_session_gtk_theme)
		export GTK_THEME
	fi
}
