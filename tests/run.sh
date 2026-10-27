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

# 1. Scripts de shell (os de Python ficam com o passo 1b).
shell_bins=() python_bins=()
for f in session/bin/*; do
	case $(head -n1 "$f") in
	*python*) python_bins+=("$f") ;;
	*) shell_bins+=("$f") ;;
	esac
done
if command -v shellcheck >/dev/null; then
	if shellcheck -x -P SCRIPTDIR/../.. "${shell_bins[@]}" &&
		shellcheck -s bash session/lib/*.sh session/lib/big-wm-autostart-condition; then
		pass shellcheck
	else
		fail shellcheck
	fi
else
	echo "aviso: shellcheck não instalado, verificação pulada" >&2
fi
for f in "${shell_bins[@]}"; do
	bash -n "$f" || fail "sintaxe: $f"
done

# 1b. Scripts de Python.
for f in "${python_bins[@]}" session/lib/big-wm-x11-repaint; do
	python3 -I -c 'import ast, sys; ast.parse(open(sys.argv[1]).read(), sys.argv[1])' "$f" ||
		fail "sintaxe: $f"
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
	errors=$(desktop-file-validate session/autostart/*.desktop session/autostart-wayland/*.desktop session/xsessions/*.desktop \
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
for f in session/autostart/*.desktop session/autostart-wayland/*.desktop; do
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
	allowed='^/(usr/bin/(startbigwm|startbigwm-wayland|big-wm-[a-z-]+)|usr/lib/big-wm/|usr/lib/systemd/user/(big-wm-|app-[^/]+@autostart\.service\.d/big-wm\.conf$)|etc/xdg/autostart/big-wm-|usr/share/(big-wm/|backgrounds/big-wm/|icons/hicolor/[a-z0-9]+/apps/big-wm\.(png|svg)$|icons/BigWindowMaker/|slick-greeter/badges/big-wm(-wayland)?\.svg$|xsessions/big-wm|wayland-sessions/big-wm|xdg-desktop-portal/(windowmaker|wlmaker)-portals\.conf|wlmaker/icons/big-wm-|WindowMaker/(Themes|Styles|Icons)/))'
	while IFS= read -r path; do
		rel=${path#"$destdir"}
		[[ $rel =~ $allowed ]] || fail "arquivo instalado fora dos caminhos permitidos: $rel"
	done < <(find "$destdir" \( -type f -o -type l \))
	[ -L "$destdir/usr/share/backgrounds/big-wm/default.jpg" ] &&
		[ -e "$destdir/usr/share/backgrounds/big-wm/default.jpg" ] || fail "default.jpg quebrado"
	# Ícones e wallpapers referenciados pela configuração precisam existir.
	for icon in $(grep -h -o 'big-wm-[a-z0-9-]*\.png' data/GNUstep/Defaults/* data/wlmaker/*); do
		find "$destdir" -name "$icon" | grep -q . || fail "ícone referenciado não instalado: $icon"
	done
	for bg in $(grep -h -o '/usr/share/backgrounds/big-wm/[a-z0-9.-]*' -r data themes); do
		[ -e "$destdir$bg" ] || fail "wallpaper referenciado não instalado: $bg"
	done
	# Tema de ícones BigWindowMaker: links sem destino quebrado, diretórios
	# do index.theme presentes e os PNG do Window Maker (64 px) e do wlmaker
	# (56 px) gerados para cada SVG.
	theme=$destdir/usr/share/icons/BigWindowMaker
	while IFS= read -r link; do
		[ -e "$link" ] || fail "tema de ícones: link quebrado ${link#"$theme"/}"
	done < <(find "$theme" -type l)
	for d in $(sed -n 's/^Directories=//p' "$theme/index.theme" | tr ',' ' '); do
		[ -d "$theme/$d" ] || fail "tema de ícones: falta o diretório $d do index.theme"
	done
	for name in places/folder apps/utilities-terminal apps/org.xfce.thunar apps/firefox apps/web-browser; do
		[ -e "$theme/scalable/$name.svg" ] || fail "tema de ícones: falta $name"
	done
	for svg in themes/icons/svg/*.svg; do
		i=${svg##*/}
		i=${i%.svg}
		python3 -I -c 'import sys, xml.etree.ElementTree as E; E.parse(sys.argv[1])' "$svg" || fail "SVG inválido: $svg"
		[ -s "$destdir/usr/share/WindowMaker/Icons/big-wm-$i.png" ] || fail "PNG do Window Maker não gerado: big-wm-$i.png"
		[ -s "$destdir/usr/share/wlmaker/icons/big-wm-$i-56x56.png" ] || fail "PNG do wlmaker não gerado: big-wm-$i-56x56.png"
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
		bash -c "sed -e 's|^datadir=.*|datadir=$fakeroot/usr/share/big-wm|' \\
			-e 's|^kvantumdir=.*|kvantumdir=$home/kvantum|' \\
			-e 's|^wmicons=.*|wmicons=$fakeroot/usr/share/WindowMaker/Icons|' \\
			session/bin/big-wm-firstrun > '$home/firstrun' && bash '$home/firstrun' $2"
}
mkdir -p "$home/kvantum/QTStepOpenStep"
if run_firstrun pt_BR.UTF-8 x11 && run_firstrun pt_BR.UTF-8 wayland; then
	for f in GNUstep/Defaults/WindowMaker GNUstep/Defaults/WMRootMenu GNUstep/Defaults/WMState \
		.config/wlmaker/Config.plist .config/wlmaker/RootMenu.plist \
		.config/picom/picom.conf; do
		[ -e "$home/$f" ] || fail "firstrun não criou $f"
	done
	grep -q 'Aplicativos' "$home/GNUstep/Defaults/WMRootMenu" || fail "menu pt_BR não foi escolhido"
	grep -qx 'theme=QTStepOpenStep' "$home/.config/Kvantum/kvantum.kvconfig" || fail "firstrun não escolheu o QTStep no Kvantum"
	[ -L "$home/GNUstep/Library/WindowMaker/Screenshots" ] &&
		[ -d "$home/Pictures/Capturas de tela" ] || fail "pasta de capturas não foi ligada a Imagens"
	[ -e "$home/GNUstep/Library/WindowMaker/autostart" ] && fail "firstrun não deve criar autostart (o Window Maker chamaria system())"
	grep -q '@XKB_' "$home/.config/wlmaker/Config.plist" && fail "Config.plist com marcadores não substituídos"
	python3 -I tests/plistcheck.py "$home/.config/wlmaker/Config.plist" || fail "Config.plist gerado inválido"
	grep -q 'GeneratePlistMenu, "big-wm-appmenu --wlmaker"' "$home/.config/wlmaker/RootMenu.plist" ||
		fail "menu Wayland sem o submenu Aplicativos gerado"
	# Config.plist de uma versão anterior: só as linhas antigas mudam.
	wlconfig=$home/.config/wlmaker/Config.plist
	sed -i -e 's#"big-wm-open apps"#"rofi -show drun"#' \
		-e 's#Command = "/usr/bin/big-wm-lock";#Command = "/usr/bin/swaylock -f -c 202a2e";#' "$wlconfig"
	run_firstrun pt_BR.UTF-8 wayland
	grep -q '"Logo+space" = (ShellExecute, "big-wm-open apps");' "$wlconfig" &&
		grep -q 'Command = "/usr/bin/big-wm-lock";' "$wlconfig" ||
		fail "firstrun não migrou o Config.plist do wlmaker"
	# Menu de uma versão anterior: só as linhas quebradas mudam.
	old_menu=$home/GNUstep/Defaults/WMRootMenu
	cat >"$old_menu" <<'EOF'
(
  "Meu menu",
  (Aplicativos, OPEN_PLMENU, "|| wmmenugen -parser:xdg /usr/share/applications/ /var/lib/flatpak/exports/share/applications/ 2>/dev/null"),
  ("Window Maker", EXEC, "/usr/lib/GNUstep/Applications/WPrefs.app/WPrefs"),
  ("Bloquear tela", SHORTCUT, "Mod4+l", EXEC, "loginctl lock-session"),
  ("Meu item", EXEC, "meu-programa")
)
EOF
	run_firstrun pt_BR.UTF-8 x11
	grep -q '"|| big-wm-appmenu"' "$old_menu" &&
		grep -q 'EXEC, WPrefs)' "$old_menu" &&
		grep -q 'EXEC, "big-wm-lock")' "$old_menu" &&
		grep -q '"Meu item", EXEC, "meu-programa"' "$old_menu" &&
		grep -q '"Meu menu"' "$old_menu" || fail "firstrun não migrou o menu antigo corretamente"
	python3 -I tests/plistcheck.py --wings "$old_menu" || fail "menu migrado inválido"
	# Ícones: conta nova já vem com o tema Window Maker em SVG.
	attrs=$home/GNUstep/Defaults/WMWindowAttributes
	grep -q '"big-wm-netscape.png"' "$attrs" && grep -q '"big-wm-folder.png"' "$attrs" ||
		fail "firstrun: atributos novos sem os ícones do Netscape e da pasta"
	# Atributos de uma versão anterior, como o Window Maker os grava: o
	# Firefox e o "*" ainda com os padrões antigos mudam; o Thunar com um
	# ícone escolhido pelo usuário e o Firefox de outra classe ficam.
	cat >"$attrs" <<'EOF'
{
  Logo.WMClip = {
    Icon = "big-wm-clip.png";
    AlwaysUserIcon = YES;
  };
  Navigator.firefox = {
    Icon = "/usr/share/icons/hicolor/64x64/apps/firefox.png";
  };
  thunar.Thunar = {
    Icon = "/home/usuario/minha-pasta.png";
  };
  "Navigator.Firefox-esr" = {
    Icon = "/usr/share/icons/hicolor/64x64/apps/firefox.png";
    NoTitlebar = YES;
  };
  "*" = {
    Icon = defaultAppIcon.tiff;
  };
}
EOF
	run_firstrun pt_BR.UTF-8 x11
	python3 -I tests/plistcheck.py --wings "$attrs" || fail "atributos migrados inválidos"
	flat=$(tr -s ' \n' ' ' <"$attrs")
	grep -qF 'Navigator.firefox = { Icon = "big-wm-netscape.png"; AlwaysUserIcon = YES; };' <<<"$flat" &&
		grep -qF 'thunar.Thunar = { Icon = "/home/usuario/minha-pasta.png"; };' <<<"$flat" &&
		grep -qF '"Navigator.Firefox-esr" = { Icon = "/usr/share/icons/hicolor/64x64/apps/firefox.png"; NoTitlebar = YES; };' <<<"$flat" &&
		grep -qF '"*" = { Icon = "big-wm-defaultapp.png"; };' <<<"$flat" ||
		fail "firstrun não migrou os ícones X11 corretamente: $flat"
	# Lançadores do wlmaker de uma versão anterior (globo, pasta amarela e o
	# terminal do wlmaker), com um lançador do usuário que também usa o
	# terminal-56x56.png e deve ficar como está.
	wlstate=$home/.config/wlmaker/State.plist
	sed -e 's/"big-wm-netscape-56x56.png"/"big-wm-browser-56x56.png"/' \
		-e 's/"big-wm-folder-56x56.png"/"big-wm-files-56x56.png"/' \
		-e 's/"big-wm-terminal-56x56.png"/"terminal-56x56.png"/' \
		-e '/^    Icon = "big-wm-clip-56x56.png";/d' \
		-e 's#^    Launchers = ($#&\n      {\n        CommandLine = "/usr/bin/xterm";\n        Icon = "terminal-56x56.png";\n      },#' \
		data/wlmaker/State.plist >"$wlstate"
	run_firstrun pt_BR.UTF-8 wayland
	python3 -I tests/plistcheck.py "$wlstate" || fail "State.plist migrado inválido"
	flat=$(tr -s ' \n' ' ' <"$wlstate")
	grep -qF 'CommandLine = "/usr/bin/big-wm-open terminal"; Icon = "big-wm-terminal-56x56.png";' <<<"$flat" &&
		grep -qF 'CommandLine = "/usr/bin/xterm"; Icon = "terminal-56x56.png";' <<<"$flat" &&
		grep -qF 'Icon = "big-wm-netscape-56x56.png";' <<<"$flat" &&
		grep -qF 'Icon = "big-wm-folder-56x56.png";' <<<"$flat" &&
		! grep -q -e 'big-wm-browser' -e 'big-wm-files' "$wlstate" ||
		fail "firstrun não migrou os ícones do wlmaker corretamente: $flat"
	echo usuario >"$home/GNUstep/Defaults/WindowMaker"
	printf '[General]\ntheme=KvFlat\n' >"$home/.config/Kvantum/kvantum.kvconfig"
	run_firstrun en_US.UTF-8 x11
	[ "$(cat "$home/GNUstep/Defaults/WindowMaker")" = usuario ] || fail "firstrun sobrescreveu configuração do usuário"
	grep -qx 'theme=KvFlat' "$home/.config/Kvantum/kvantum.kvconfig" || fail "firstrun trocou o tema do Kvantum escolhido pelo usuário"
	pass big-wm-firstrun
else
	fail "big-wm-firstrun"
fi
rm -rf "$home"

# 6b. big-wm-appmenu: menu Aplicativos a partir de .desktop falsos.
apps=$(mktemp -d)
mkdir -p "$apps/home/applications" "$apps/sys/applications"
desktop() { # diretório, id, conteúdo
	printf '[Desktop Entry]\nType=Application\n%b\n' "$3" >"$apps/$1/applications/$2.desktop"
}
desktop sys calc 'Name=Calculator\nName[pt_BR]=Calculadora\nExec=calc\nCategories=Utility;'
desktop sys hidden 'Name=Oculto\nExec=x\nNoDisplay=true\nCategories=Utility;'
desktop sys xfce 'Name=Sobre o Xfce\nExec=x\nOnlyShowIn=XFCE;\nCategories=Utility;'
desktop sys missing 'Name=Sem programa\nExec=x\nTryExec=/nao/existe\nCategories=Game;'
desktop sys web 'Name=WhatsApp\nExec=x\nCategories=Webapps;'
desktop sys conf 'Name=Ajustes\nExec=x\nCategories=System;Settings;'
desktop sys loose 'Name=Solto\nExec=x'
desktop sys over 'Name=Do sistema\nExec=x\nCategories=Office;'
desktop home over 'Name=Do usuário\nExec=x\nCategories=Office;'
menu=$(HOME=$apps LANG=pt_BR.UTF-8 XDG_CURRENT_DESKTOP=WindowMaker XDG_DATA_HOME=$apps/home \
	XDG_DATA_DIRS=$apps/sys python3 -I session/bin/big-wm-appmenu --no-cache)
printf '%s\n' "$menu" >"$apps/menu.plist"
python3 -I tests/plistcheck.py --wings "$apps/menu.plist" || fail "big-wm-appmenu: plist inválido"
expect() { grep -qF -- "$1" <<<"$menu" || fail "big-wm-appmenu: falta $1"; }
reject() { grep -qF -- "$1" <<<"$menu" && fail "big-wm-appmenu: não devia ter $1"; }
expect '"Aplicativos",'
expect '("Calculadora", EXEC, "gtk-launch calc")'
expect '"Acessórios",'
expect '"Aplicativos web",'
expect '("Ajustes", EXEC, "gtk-launch conf")'
expect '"Configurações",'
expect '"Outros",'
expect '"Do usuário"'
reject 'Oculto'
reject 'Xfce'
reject 'Sem programa'
reject 'Do sistema'
[ "$(grep -n '"Outros"' <<<"$menu" | cut -d: -f1)" -gt "$(grep -n '"Escritório"' <<<"$menu" | cut -d: -f1)" ] ||
	fail "big-wm-appmenu: Outros deve ser a última categoria"
# Mesmo menu no formato do wlmaker (GeneratePlistMenu do menu Wayland).
wlmenu=$(HOME=$apps LANG=pt_BR.UTF-8 XDG_CURRENT_DESKTOP=wlmaker XDG_DATA_HOME=$apps/home \
	XDG_DATA_DIRS=$apps/sys python3 -I session/bin/big-wm-appmenu --wlmaker --no-cache)
printf '%s\n' "$wlmenu" >"$apps/wlmenu.plist"
python3 -I tests/plistcheck.py "$apps/wlmenu.plist" || fail "big-wm-appmenu --wlmaker: plist inválido"
grep -qF '("Calculadora", ShellExecute, "gtk-launch calc")' "$apps/wlmenu.plist" ||
	fail "big-wm-appmenu --wlmaker: item sem ShellExecute"
mkdir -p "$apps/home/GNUstep/Library/WindowMaker/Backgrounds"
: >"$apps/home/GNUstep/Library/WindowMaker/Backgrounds/praia.jpg"
HOME=$apps/home LANG=pt_BR.UTF-8 python3 -I session/bin/big-wm-appmenu --wlmaker --wallpapers >"$apps/wall.plist"
python3 -I tests/plistcheck.py "$apps/wall.plist" && grep -q '"praia", ShellExecute, "big-wm-wallpaper ' "$apps/wall.plist" ||
	fail "big-wm-appmenu --wallpapers"
LANG=pt_BR.UTF-8 python3 -I session/bin/big-wm-appmenu --dockapps >"$apps/dockapps.plist"
python3 -I tests/plistcheck.py --wings "$apps/dockapps.plist" || fail "big-wm-appmenu --dockapps: plist inválido"
rm -rf "$apps"
pass big-wm-appmenu

# 6c. Autostarts cortados só nas sessões Big WindowMaker.
cond() { env -i PATH="$PATH" XDG_CURRENT_DESKTOP="$1" XDG_SESSION_TYPE="${3:-x11}" \
	bash session/lib/big-wm-autostart-condition "$2"; }
cond KDE sound-switcher || fail "autostart: outro ambiente não pode ser afetado"
cond XFCE bluetooth || fail "autostart: outro ambiente não pode ser afetado (bluetooth)"
cond WindowMaker sound-switcher && fail "autostart: indicator-sound-switcher deve ser pulado"
cond wlmaker x11 wayland && fail "autostart: picom não pode subir na sessão Wayland"
cond WindowMaker x11 x11 || fail "autostart: picom deve subir na sessão X11"
for f in session/systemd/autostart-conditions/*.service; do
	arg=$(sed -n 's|^ExecCondition=/usr/lib/big-wm/big-wm-autostart-condition ||p' "$f")
	grep -q "^${arg})" session/lib/big-wm-autostart-condition || fail "$f: condição desconhecida '$arg'"
done
pass "condições de autostart"

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
