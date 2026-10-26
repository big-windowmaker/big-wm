#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Testes estáticos do Big WindowMaker. Executar da raiz do repositório.

set -u
cd "$(dirname "$0")/.."

failures=0
fail() {
	echo "FALHOU: $*" >&2
	failures=$((failures + 1))
}
pass() {
	echo "ok: $*"
}

# 1. Scripts de shell.
if command -v shellcheck >/dev/null; then
	if shellcheck -x -P SCRIPTDIR/../.. session/bin/* &&
		shellcheck -s bash session/lib/*.sh; then
		pass shellcheck
	else
		fail shellcheck
	fi
else
	echo "aviso: shellcheck não instalado, verificação pulada" >&2
fi
for f in session/bin/*; do
	bash -n "$f" || fail "sintaxe: $f"
done

# 2. Property lists.
python3 -I tests/plistcheck.py --wings data/GNUstep/Defaults/* themes/WindowMaker/Themes/*/style \
	themes/WindowMaker/Styles/*.style && pass "plists do Window Maker" || fail "plists do Window Maker"
python3 -I tests/plistcheck.py data/wlmaker/*.plist data/wlmaker/*.plist.* data/wlmaker/Config.plist.in &&
	pass "plists do wlmaker" || fail "plists do wlmaker"

# 3. Arquivos .desktop. O validador não conhece as convenções dos gerenciadores
#    de login (Type=XSession, DesktopNames) nem nomes de ambiente fora da lista
#    da freedesktop.org (WindowMaker, wlmaker); só essas mensagens são toleradas.
if command -v desktop-file-validate >/dev/null; then
	errors=$(desktop-file-validate session/autostart/*.desktop session/xsessions/*.desktop \
		session/wayland-sessions/*.desktop 2>&1 |
		grep -v -E 'unregistered value "(WindowMaker|wlmaker)"|"XSession" for key "Type"|key "DesktopNames"')
	if [ -z "$errors" ]; then
		pass desktop-file-validate
	else
		echo "$errors" >&2
		fail desktop-file-validate
	fi
fi

# 4. Autostart nunca pode vazar para outros ambientes (Plasma, XFCE, GNOME...)
#    nem usar chaves que o systemd-xdg-autostart-generator ignora.
for f in session/autostart/*.desktop; do
	grep -q '^OnlyShowIn=' "$f" || fail "$f sem OnlyShowIn"
	grep -q '^X-GNOME-Autostart-Phase=' "$f" && fail "$f usa X-GNOME-Autostart-Phase (ignorado pelo systemd)"
	grep '^OnlyShowIn=' "$f" | grep -q -v -E '^OnlyShowIn=((WindowMaker|wlmaker);)+$' &&
		fail "$f: OnlyShowIn deve conter só WindowMaker/wlmaker"
done
pass "autostart restrito às sessões Big WindowMaker"

# 5. Instalação: nada fora dos caminhos permitidos. O Big WindowMaker não
#    pode tocar /etc/skel, binários do Window Maker nem arquivos de outros pacotes.
destdir=$(mktemp -d)
trap 'rm -rf "$destdir"' EXIT
if make -s install DESTDIR="$destdir" >/dev/null; then
	pass "make install"
	allowed='^/(usr/bin/(startbigwm|startbigwm-wayland|big-wm-[a-z-]+)|usr/lib/big-wm/|usr/lib/systemd/user/big-wm-|etc/xdg/autostart/big-wm-|usr/share/(big-wm/|backgrounds/big-wm/|icons/hicolor/[a-z0-9]+/apps/big-wm\.(png|svg)$|slick-greeter/badges/big-wm(-wayland)?\.svg$|xsessions/big-wm|wayland-sessions/big-wm|xdg-desktop-portal/(windowmaker|wlmaker)-portals\.conf|wlmaker/icons/big-wm-|WindowMaker/(Themes|Styles|Icons)/))'
	while IFS= read -r path; do
		rel=${path#"$destdir"}
		[[ $rel =~ $allowed ]] || fail "arquivo instalado fora dos caminhos permitidos: $rel"
	done < <(find "$destdir" \( -type f -o -type l \))
	[ -L "$destdir/usr/share/backgrounds/big-wm/default.jpg" ] &&
		[ -e "$destdir/usr/share/backgrounds/big-wm/default.jpg" ] || fail "default.jpg quebrado"
	# Ícones e wallpapers referenciados pela configuração precisam existir.
	for icon in $(grep -h -o 'big-wm-[a-z-]*\.png' data/GNUstep/Defaults/* data/wlmaker/*); do
		find "$destdir" -name "$icon" | grep -q . || fail "ícone referenciado não instalado: $icon"
	done
	for bg in $(grep -h -o '/usr/share/backgrounds/big-wm/[a-z0-9.-]*' -r data themes); do
		[ -e "$destdir$bg" ] || fail "wallpaper referenciado não instalado: $bg"
	done
	pass "caminhos instalados"
else
	fail "make install"
fi

# 6. big-wm-firstrun: aplica padrões numa conta vazia e não sobrescreve nada.
home=$(mktemp -d)
fakeroot=$destdir
run_firstrun() {
	HOME=$home XDG_CONFIG_HOME=$home/.config LANG=$1 \
		BIGWM_LIBDIR=$fakeroot/usr/lib/big-wm \
		bash -c "sed 's|^datadir=.*|datadir=$fakeroot/usr/share/big-wm|' session/bin/big-wm-firstrun > '$home/firstrun' && bash '$home/firstrun' $2"
}
if run_firstrun pt_BR.UTF-8 x11 && run_firstrun pt_BR.UTF-8 wayland; then
	for f in GNUstep/Defaults/WindowMaker GNUstep/Defaults/WMRootMenu GNUstep/Defaults/WMState \
		.config/wlmaker/Config.plist .config/wlmaker/RootMenu.plist \
		.config/picom/picom.conf; do
		[ -e "$home/$f" ] || fail "firstrun não criou $f"
	done
	grep -q 'Aplicativos' "$home/GNUstep/Defaults/WMRootMenu" || fail "menu pt_BR não foi escolhido"
	[ -L "$home/GNUstep/Library/WindowMaker/Screenshots" ] &&
		[ -d "$home/Pictures/Capturas de tela" ] || fail "pasta de capturas não foi ligada a Imagens"
	[ -e "$home/GNUstep/Library/WindowMaker/autostart" ] && fail "firstrun não deve criar autostart (o Window Maker chamaria system())"
	grep -q '@XKB_' "$home/.config/wlmaker/Config.plist" && fail "Config.plist com marcadores não substituídos"
	python3 -I tests/plistcheck.py "$home/.config/wlmaker/Config.plist" || fail "Config.plist gerado inválido"
	echo usuario >"$home/GNUstep/Defaults/WindowMaker"
	run_firstrun en_US.UTF-8 x11
	[ "$(cat "$home/GNUstep/Defaults/WindowMaker")" = usuario ] || fail "firstrun sobrescreveu configuração do usuário"
	pass big-wm-firstrun
else
	fail "big-wm-firstrun"
fi
rm -rf "$home"

# 7. Layout de teclado (big-wm-keyboard.sh) com localectl falso.
kbd=$(mktemp -d)
printf 'br-abnt2\t\tbr\tabnt2\t\t-\t\tterminate:ctrl_alt_bksp\npt-latin1\t\tpt\tpc105\t\t-\t\tterminate:ctrl_alt_bksp\n' >"$kbd/map"
keyboard() { # saída do localectl falso, conteúdo do vconsole.conf
	# Função exportada: /tmp pode ser noexec, então nada de script falso no PATH.
	printf '%s\n' "$2" >"$kbd/vconsole"
	FAKE_LOCALECTL=$1 BIGWM_VCONSOLE=$kbd/vconsole BIGWM_KBD_MAP=$kbd/map bash -c \
		'localectl() { printf "%b" "$FAKE_LOCALECTL"; }
		. session/lib/big-wm-keyboard.sh; big_wm_keyboard; echo "$XKB_LAYOUT|$XKB_MODEL|$XKB_VARIANT|$XKB_OPTIONS"'
}
got=$(keyboard '   X11 Layout: (unset)\n' 'KEYMAP=br-abnt2')
[ "$got" = "br|abnt2||terminate:ctrl_alt_bksp" ] || fail "teclado: X11 sem layout deve seguir o console (obtido: $got)"
got=$(keyboard '   X11 Layout: de\n   X11 Variant: nodeadkeys\n' 'KEYMAP=br-abnt2')
[ "$got" = "de|pc105|nodeadkeys|" ] || fail "teclado: layout X11 do localectl deve prevalecer (obtido: $got)"
got=$(keyboard '' '')
[ "$got" = "us|pc105||" ] || fail "teclado: sem configuração deve cair em us (obtido: $got)"
rm -rf "$kbd"
pass "layout de teclado (localectl, console, padrão)"

if [ "$failures" -gt 0 ]; then
	echo "$failures verificação(ões) falharam" >&2
	exit 1
fi
echo "todas as verificações passaram"
