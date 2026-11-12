# SPDX-License-Identifier: GPL-2.0-or-later
#
# Variáveis de ambiente comuns às sessões X11 e Wayland do Big WindowMaker.
# Carregado com "." pelos wrappers de sessão. Nenhuma variável já definida
# pelo usuário (por exemplo em ~/.config/environment.d) é sobrescrita.

# Idioma das mensagens segue o LANG quando LANGUAGE não foi definido.
[ -z "$LANGUAGE" ] && export LANGUAGE="$LANG"

# Aplicativos Qt seguem o tema configurado pelo portal; Kvantum quando instalado.
# Os aplicativos QML acham o estilo "kvantum" do Qt Quick Controls em
# /usr/lib/qt6/qml/kvantum (o Fusion, com a paleta do Kvantum).
[ -z "$QT_QPA_PLATFORMTHEME" ] && export QT_QPA_PLATFORMTHEME=xdg-desktop-portal
if [ -z "$QT_STYLE_OVERRIDE" ] && [ -e /usr/lib/qt6/plugins/styles/libkvantum.so ]; then
	export QT_STYLE_OVERRIDE=kvantum
fi

# Aplicativos GTK (Thunar, Firefox, libadwaita...) com o OneStepBack, o par
# NeXTSTEP do QTStep dos aplicativos Qt, ou com o OneStepBack-Dark no modo
# escuro (big-wm-theme dark, guardado em ~/.config/big-wm/theme). Só nas
# sessões Big WindowMaker: Plasma, GNOME e XFCE seguem com o tema deles. Para
# outro tema: GTK_THEME no ~/.config/environment.d ou BIGWM_GTK_THEME=0.
# shellcheck source=session/lib/big-wm-theme.sh
. "${BIGWM_LIBDIR:-/usr/lib/big-wm}/big-wm-theme.sh"
#
# Wayland: o GTK_THEME é o nome estável BigWindowMaker, um link para o tema
# do modo (ver big-wm-theme.sh), para GTK 3, GTK 4 e libadwaita. Ele não
# fica velho no ambiente do wlmaker, do systemd --user e do D-Bus quando o
# modo muda com a sessão aberta: tudo o que abre depois sai no modo novo.
#
# X11 com o xsettingsd: sem GTK_THEME. O tema vai pelo Net/ThemeName do
# XSETTINGS (ver big-wm-theme) e as janelas GTK 3 e 4 já abertas mudam na
# hora; com o GTK_THEME definido, o gtk-nocsd fixa o gtk-theme-name no valor
# dele e elas não releriam o tema. O libadwaita segue o color-scheme.
# BIGWM_GTK_THEME=1 usa o GTK_THEME também no X11.
#
# Um GTK_THEME herdado que já é do Big WindowMaker (de uma sessão anterior,
# do ambiente de uma sessão que caiu) é refeito; outro é escolha do usuário.
big_wm_use_xsettings() {
	[ "${XDG_CURRENT_DESKTOP:-}" = WindowMaker ] && [ "${BIGWM_GTK_THEME:-}" != 1 ] &&
		command -v xsettingsd >/dev/null
}
if big_wm_gtk_theme_managed "$GTK_THEME"; then
	unset GTK_THEME
	case ${GTK2_RC_FILES:-} in
	*/themes/OneStepBack/gtk-2.0/gtkrc:* | */themes/OneStepBack-Dark/gtk-2.0/gtkrc:* | \
		*/themes/"$BIG_WM_GTK_THEME_NAME"/gtk-2.0/gtkrc:*)
		unset GTK2_RC_FILES
		;;
	esac
fi
if [ -z "$GTK_THEME" ] && [ "${BIGWM_GTK_THEME:-1}" != 0 ] && [ -d "${BIGWM_THEMESDIR:-/usr/share/themes}/OneStepBack/gtk-3.0" ]; then
	if big_wm_use_xsettings; then
		# só o link, para o GTK 2 (abaixo)
		big_wm_gtk_theme_link
	else
		GTK_THEME=$(big_wm_session_gtk_theme)
		export GTK_THEME
	fi
fi

# Terminal padrão.
[ -z "$TERMINAL" ] && command -v xdg-terminal-exec >/dev/null && export TERMINAL=xdg-terminal-exec

# Renderizador do GTK 4 por software (cairo) no Mesa amber (placas antigas,
# sem OpenGL 3) e em máquinas virtuais. Medido numa VM QEMU (virtio-gpu sem
# 3D): a calculadora ocupa 66 MB com cairo e 121 MB com GL, que carrega o
# LLVM do llvmpipe em cada aplicativo; o padrão ainda tenta Vulkan antes e
# falha.
if [ -z "$GSK_RENDERER" ]; then
	if compgen -G '/var/lib/pacman/local/mesa-amber-*' >/dev/null || systemd-detect-virt -q; then
		export GSK_RENDERER=cairo
	fi
fi

# Janelas GTK 3, GTK 4 e libadwaita com a barra de título e as bordas do
# gerenciador de janelas: o gtk-nocsd desliga a decoração desenhada pelo
# aplicativo (CSD). Os aplicativos Qt já pedem a decoração do servidor.
# Para voltar à CSD: GTK_CSD=1 ou BIGWM_NOCSD=0 (por exemplo no ~/.xprofile).
big_wm_nocsd() {
	local nocsd=/usr/lib/libgtk-nocsd.so.0
	[ -e "$nocsd" ] && [ "${BIGWM_NOCSD:-1}" != 0 ] && [ "${GTK_CSD:-0}" != 1 ] || return 0
	case ":${LD_PRELOAD:-}:" in
	*":$nocsd:"*) ;;
	*) export LD_PRELOAD="$nocsd${LD_PRELOAD:+:$LD_PRELOAD}" ;;
	esac
}

# GTK 2 não lê o GTK_THEME: recebe o gtkrc do mesmo tema (sem a variante
# ":dark") pelo GTK2_RC_FILES, seguido do ~/.gtkrc-2.0, que continua valendo
# por cima. Só quando o tema tem gtk-2.0 (o OneStepBack tem; usa o motor
# pixmap, que vem com o próprio gtk2). Com o nome estável, o gtkrc é o do
# link: o GTK 2 aberto depois de uma troca já sai no modo novo.
if [ -z "$GTK2_RC_FILES" ] && { [ "$GTK_THEME" = "$BIG_WM_GTK_THEME_NAME" ] ||
	{ [ -z "$GTK_THEME" ] && [ "${BIGWM_GTK_THEME:-1}" != 0 ] && big_wm_use_xsettings; }; } &&
	[ -r "$(big_wm_gtk_theme_link_path)/gtk-2.0/gtkrc" ]; then
	GTK2_RC_FILES="$(big_wm_gtk_theme_link_path)/gtk-2.0/gtkrc:$HOME/.gtkrc-2.0"
	export GTK2_RC_FILES
elif [ -z "$GTK2_RC_FILES" ] && [ -n "$GTK_THEME" ] && [ -r "${BIGWM_THEMESDIR:-/usr/share/themes}/${GTK_THEME%%:*}/gtk-2.0/gtkrc" ]; then
	export GTK2_RC_FILES="${BIGWM_THEMESDIR:-/usr/share/themes}/${GTK_THEME%%:*}/gtk-2.0/gtkrc:$HOME/.gtkrc-2.0"
fi
