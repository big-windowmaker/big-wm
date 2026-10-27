# SPDX-License-Identifier: GPL-2.0-or-later
#
# Variáveis de ambiente comuns às sessões X11 e Wayland do Big WindowMaker.
# Carregado com "." pelos wrappers de sessão. Nenhuma variável já definida
# pelo usuário (por exemplo em ~/.config/environment.d) é sobrescrita.

# Idioma das mensagens segue o LANG quando LANGUAGE não foi definido.
[ -z "$LANGUAGE" ] && export LANGUAGE="$LANG"

# Aplicativos Qt seguem o tema configurado pelo portal; Kvantum quando instalado.
[ -z "$QT_QPA_PLATFORMTHEME" ] && export QT_QPA_PLATFORMTHEME=xdg-desktop-portal
if [ -z "$QT_STYLE_OVERRIDE" ] && [ -e /usr/lib/qt6/plugins/styles/libkvantum.so ]; then
	export QT_STYLE_OVERRIDE=kvantum
fi

# Aplicativos GTK (Thunar, Firefox...) com o OneStepBack, o par NeXTSTEP do
# QTStep dos aplicativos Qt, ou com o OneStepBack-Dark no modo escuro
# (big-wm-theme dark, guardado em ~/.config/big-wm/theme). Só nas sessões
# Big WindowMaker: Plasma, GNOME e XFCE seguem com o tema deles. Para outro
# tema: GTK_THEME no ~/.config/environment.d ou BIGWM_GTK_THEME=0.
# shellcheck source=session/lib/big-wm-theme.sh
. "${BIGWM_LIBDIR:-/usr/lib/big-wm}/big-wm-theme.sh"
if [ -z "$GTK_THEME" ] && [ "${BIGWM_GTK_THEME:-1}" != 0 ] && [ -d "${BIGWM_THEMESDIR:-/usr/share/themes}/OneStepBack/gtk-3.0" ]; then
	GTK_THEME=$(big_wm_gtk_theme)
	export GTK_THEME
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
# pixmap, que vem com o próprio gtk2).
if [ -z "$GTK2_RC_FILES" ] && [ -n "$GTK_THEME" ] && [ -r "/usr/share/themes/${GTK_THEME%%:*}/gtk-2.0/gtkrc" ]; then
	export GTK2_RC_FILES="/usr/share/themes/${GTK_THEME%%:*}/gtk-2.0/gtkrc:$HOME/.gtkrc-2.0"
fi
