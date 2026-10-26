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
