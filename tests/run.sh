#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Testes estáticos do Big WindowMaker. Executar da raiz do repositório.

# "teste && pass || fail" é proposital (pass só escreve) e os laços por
# palavra percorrem nomes de ícones e diretórios, sem espaços.
# shellcheck disable=SC2015,SC2013

set -u
cd "$(dirname "$0")/.." || exit 1

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
	# UTF-8: no locale C o shellcheck aborta ao citar uma linha com acento
	# ("commitBuffer: invalid argument").
	sc() { LC_ALL=C.UTF-8 shellcheck "$@"; }
	if sc -x -P SCRIPTDIR/../.. "${shell_bins[@]}" &&
		sc -s bash session/lib/*.sh session/lib/big-wm-autostart-condition session/lib/wayland-bin/* &&
		sc themes/boot/grub/gerar.sh themes/boot/plymouth/gerar.sh \
			themes/boot/plymouth/libalpm/plymouth-theme-big-wm &&
		sc -s bash pkgbuild/*.install &&
		sc -x -P SCRIPTDIR tests/run.sh tests/integration/*.sh; then
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
for f in "${python_bins[@]}" session/lib/big-wm-x11-repaint session/lib/big-wm-dockapps-autolaunch session/lib/big-wm-clip-add; do
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
for f in session/autostart/*.desktop session/autostart-wayland/*.desktop tabbed-dock/data/big-wm-tabbed-dock.desktop.in; do
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
	allowed='^/(usr/bin/(startbigwm|startbigwm-wayland|big-wm-[a-z-]+)|usr/lib/big-wm/|usr/lib/systemd/user/(big-wm-|app-[^/]+@autostart\.service\.d/big-wm\.conf$)|etc/xdg/autostart/big-wm-|usr/share/(big-wm/|backgrounds/big-wm/|icons/hicolor/[a-z0-9]+/apps/big-wm\.(png|svg)$|icons/BigWindowMaker/|slick-greeter/badges/big-wm(-wayland)?\.svg$|xsessions/big-wm|wayland-sessions/big-wm|xdg-desktop-portal/(windowmaker|wlmaker)-portals\.conf|wlmaker/icons/big-wm-|WindowMaker/(Themes|Styles|Icons)/|plymouth/themes/big-wm/|libalpm/(hooks/80-plymouth-theme-big-wm-(antes|depois)\.hook|scripts/plymouth-theme-big-wm)$|applications/big-wm-tabbed-dock-preferences\.desktop$|locale/[A-Za-z_@]+/LC_MESSAGES/big-wm-tabbed-dock\.mo$)|boot/grub/themes/big-wm/)'
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

	# Temas de boot: tudo o que o theme.txt e o script do Plymouth pedem existe,
	# e cada fonte pedida pelo tema tem um .pf2 com esse nome.
	check_grub_theme() { # diretório do tema
		local t=$1/theme.txt f nome
		grep -q '@[A-Z_]*@' <(grep -v -e '@KEYMAP_SHORT@' -e '@TIMEOUT_NOTIFICATION_MIDDLE@' "$t") &&
			fail "tema do GRUB: marcador não substituído em $t"
		for f in $(sed -n 's/^[[:space:]]*\(file\|selected_item_pixmap_style\|scrollbar_frame\|scrollbar_thumb\)[[:space:]]*=[[:space:]]*"\(.*\)"/\2/p' "$t"); do
			compgen -G "$1/$f" >/dev/null || fail "tema do GRUB: falta $f"
		done
		while IFS= read -r nome; do
			grep -l -a -F "$nome" "$1"/*.pf2 >/dev/null || fail "tema do GRUB: nenhum .pf2 com a fonte \"$nome\""
		done < <(sed -n 's/.*font[[:space:]]*[:=][[:space:]]*"\([^"]*\)".*/\1/p' "$t" | sort -u)
	}
	check_grub_theme "$destdir/boot/grub/themes/big-wm"
	live=$(mktemp -d)
	if themes/boot/grub/gerar.sh "$live" live; then
		check_grub_theme "$live"
		grep -q 'Big WindowMaker Live' "$live/theme.txt" || fail "tema live sem o título da live"
	else
		fail "tema do GRUB da live"
	fi
	rm -rf "$live"
	ply=$destdir/usr/share/plymouth/themes/big-wm
	for f in $(grep -o 'Image("[^"]*")' "$ply/big-wm.script" | cut -d'"' -f2) \
		$(grep -o -E '(retangulo|relevo)\("[a-z-]+"(, "[a-z-]+")?' "$ply/big-wm.script" | grep -o '"[a-z-]*"' | tr -d '"' | sed 's/.*/cor-&.png/'); do
		[ -s "$ply/$f" ] || fail "tema do Plymouth: falta $f"
	done
	grep -qx 'ScriptFile=/usr/share/plymouth/themes/big-wm/big-wm.script' "$ply/big-wm.plymouth" ||
		fail "tema do Plymouth: ScriptFile"
	pass "temas de boot (GRUB e Plymouth)"
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
			-e 's|^ashyterm=.*|ashyterm=$home/ashyterm|' \\
			-e 's|^wmicons=.*|wmicons=$fakeroot/usr/share/WindowMaker/Icons|' \\
			session/bin/big-wm-firstrun > '$home/firstrun' && bash '$home/firstrun' $2"
}
mkdir -p "$home/kvantum/QTStepOpenStep"
touch "$home/ashyterm"
if run_firstrun pt_BR.UTF-8 x11 && run_firstrun pt_BR.UTF-8 wayland; then
	for f in GNUstep/Defaults/WindowMaker GNUstep/Defaults/WMRootMenu GNUstep/Defaults/WMState \
		.config/wlmaker/Config.plist .config/wlmaker/RootMenu.plist \
		.config/picom/picom.conf; do
		[ -e "$home/$f" ] || fail "firstrun não criou $f"
	done
	grep -q 'Aplicativos' "$home/GNUstep/Defaults/WMRootMenu" || fail "menu pt_BR não foi escolhido"
	grep -qx 'theme=QTStepOpenStep' "$home/.config/Kvantum/kvantum.kvconfig" || fail "firstrun não escolheu o QTStep no Kvantum"
	cmp -s "$home/.config/fastfetch/ascii-art" data/fastfetch/ascii-art &&
		python3 -I -c 'import json,sys,re; json.loads(re.sub(r"(?m)^\s*//.*$","",open(sys.argv[1]).read()))' "$home/.config/fastfetch/config.jsonc" ||
		fail "firstrun não instalou o fastfetch do Big WindowMaker"
	python3 -I -c 'import json, sys; assert json.load(open(sys.argv[1])) == {"transparency": 22}' \
		"$home/.config/ashyterm/settings.json" || fail "firstrun não ligou a transparência do AshyTerm"
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
	# Atalhos novos (captura de janela, Forçar o fechamento) entram uma vez.
	sed -i -e '/"Alt+Print"/d' -e '/"Ctrl+Alt+Escape"/d' "$wlconfig"
	run_firstrun pt_BR.UTF-8 wayland
	run_firstrun pt_BR.UTF-8 wayland
	[ "$(grep -c '"Ctrl+Alt+Escape" = (ShellExecute, "big-wm-kill");' "$wlconfig")" = 1 ] &&
		[ "$(grep -c '"Alt+Print" = (ShellExecute, "big-wm-screenshot window");' "$wlconfig")" = 1 ] &&
		python3 -I tests/plistcheck.py "$wlconfig" || fail "firstrun não acrescentou os atalhos novos do wlmaker"
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
	grep -q '("Forçar o fechamento", SHORTCUT, "Control+Mod1+Escape", EXEC, "big-wm-kill"),' "$old_menu" ||
		fail "firstrun não acrescentou o Ctrl+Alt+Esc ao menu editado"
	# Menu padrão da versão anterior (sem Captura de tela, Executar no %A e
	# sem Forçar o fechamento), sem alteração: dá lugar ao novo.
	sed -e '/^  ($/{N;/"Captura de tela"/{:a;N;/\n  ),$/!ba;d}}' -e '/big-wm-kill/d' \
		-e 's#("Executar...", EXEC, "big-wm-open run"),#("Executar...", EXEC, "%A(Executar, Digite o comando:)"),#' \
		data/GNUstep/Defaults/WMRootMenu.pt_BR >"$old_menu"
	run_firstrun pt_BR.UTF-8 x11
	cmp -s "$old_menu" data/GNUstep/Defaults/WMRootMenu.pt_BR ||
		fail "firstrun não trocou o menu X11 padrão antigo pelo novo"
	for m in data/GNUstep/Defaults/WMRootMenu data/GNUstep/Defaults/WMRootMenu.pt_BR; do
		grep -q 'EXEC, "big-wm-screenshot area")' "$m" && grep -q 'EXEC, "big-wm-screenshot window")' "$m" &&
			grep -q '"Control+Mod1+Escape", EXEC, "big-wm-kill")' "$m" && grep -q 'EXEC, "big-wm-open run")' "$m" ||
			fail "$m: falta Captura de tela, Forçar o fechamento ou Executar"
	done
	for m in data/wlmaker/RootMenu.plist data/wlmaker/RootMenu.plist.pt_BR; do
		grep -q 'ShellExecute, "big-wm-screenshot window")' "$m" && grep -q 'ShellExecute, "big-wm-kill")' "$m" ||
			fail "$m: falta Captura de janela ou Forçar o fechamento"
	done
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
	echo '{"transparency": 0}' >"$home/.config/ashyterm/settings.json"
	run_firstrun en_US.UTF-8 x11
	[ "$(cat "$home/GNUstep/Defaults/WindowMaker")" = usuario ] || fail "firstrun sobrescreveu configuração do usuário"
	grep -qx 'theme=KvFlat' "$home/.config/Kvantum/kvantum.kvconfig" || fail "firstrun trocou o tema do Kvantum escolhido pelo usuário"
	grep -qx '{"transparency": 0}' "$home/.config/ashyterm/settings.json" ||
		fail "firstrun mexeu na configuração do AshyTerm do usuário"
	pass big-wm-firstrun
else
	fail "big-wm-firstrun"
fi
rm -rf "$home"

# 6a. Dockapps acoplados: AutoLaunch ligado uma vez; a escolha posterior do
#     usuário (desligar) fica.
dock=$(mktemp -d)
cat >"$dock/WMState" <<'PLIST'
{
  Dock = {
    Applications = (
      {
        Name = Logo.WMDock;
        AutoLaunch = No;
      },
      {
        Name = wmusic.DockApp;
        Command = "/usr/bin/wmusic";
        AutoLaunch = No;
      },
      {
        AutoLaunch = No;
        Name = "wmclock.DockApp";
      }
    );
  };
}
PLIST
autolaunch() { python3 -I session/lib/big-wm-dockapps-autolaunch "$dock/WMState" "$dock/seen" 2>/dev/null; }
autolaunch
if [ "$(grep -c 'AutoLaunch = Yes;' "$dock/WMState")" = 2 ] && grep -q 'Logo.WMDock;' "$dock/WMState" &&
	python3 -I tests/plistcheck.py --wings "$dock/WMState"; then
	sed -i '/wmusic/,/AutoLaunch/s/AutoLaunch = Yes;/AutoLaunch = No;/' "$dock/WMState"
	autolaunch
	if grep -A2 wmusic "$dock/WMState" | grep -q 'AutoLaunch = No;'; then
		pass "dockapps acoplados iniciam com a sessão"
	else
		fail "big-wm-dockapps-autolaunch religou um dockapp desligado pelo usuário"
	fi
else
	fail "big-wm-dockapps-autolaunch: $(tr -d '\n' <"$dock/WMState")"
fi
rm -rf "$dock"

# 6a2. big-wm-clip-add: no Clip da área 1, na linha abaixo do Thunar, uma vez.
clipt=$(mktemp -d)
cp data/GNUstep/Defaults/WMState "$clipt/WMState"
clipadd() { BIGWM_WMSTATE="$clipt/WMState" BIGWM_CLIP_ADDED="$clipt/added" \
	python3 -I session/lib/big-wm-clip-add big-wm-tabbed-dock.Big-wm-tabbed-dock big-wm-tabbed-dock; }
clipadd && clipadd
if [ "$(grep -c 'big-wm-tabbed-dock.Big-wm-tabbed-dock' "$clipt/WMState")" = 1 ] &&
	grep -A8 'big-wm-tabbed-dock.Big-wm-tabbed-dock' "$clipt/WMState" | grep -q 'Position = "0,3";' &&
	python3 -I tests/plistcheck.py --wings "$clipt/WMState"; then
	sed -i '/big-wm-tabbed-dock.Big-wm-tabbed-dock/,/}/d' "$clipt/WMState"
	clipadd
	if grep -q 'big-wm-tabbed-dock.Big-wm-tabbed-dock' "$clipt/WMState"; then
		fail "big-wm-clip-add devolveu ao Clip um ícone tirado pelo usuário"
	else
		pass "Big Tabbed Dock no Clip abaixo do Thunar"
	fi
else
	fail "big-wm-clip-add: $(grep -n -A9 'Applications' "$clipt/WMState" | tr -d '\n' | head -c 600)"
fi
rm -rf "$clipt"

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
# Dockapp X11 com .desktop sem NotShowIn (pacote antigo): no menu X11, nunca no Wayland.
desktop sys wmusic 'Name=wmusic\nExec=wmusic\nCategories=AudioVideo;Audio;'
for d in WindowMaker wlmaker; do
	HOME=$apps LANG=pt_BR.UTF-8 XDG_CURRENT_DESKTOP=$d XDG_DATA_HOME=$apps/home XDG_DATA_DIRS=$apps/sys \
		python3 -I session/bin/big-wm-appmenu --no-cache --wlmaker >"$apps/menu-$d.plist"
done
grep -q '"gtk-launch wmusic"' "$apps/menu-WindowMaker.plist" || fail "big-wm-appmenu: wmusic sumiu do menu X11"
grep -q 'wmusic' "$apps/menu-wlmaker.plist" && fail "big-wm-appmenu: dockapp X11 (wmusic) no menu Wayland"
# big-wm-launcher: mesmos aplicativos e categorias do menu.
launcher() { HOME=$apps XDG_CACHE_HOME=$apps/cache XDG_CONFIG_HOME=$apps/config LANG=pt_BR.UTF-8 \
	XDG_CURRENT_DESKTOP=${2:-WindowMaker} XDG_DATA_HOME=$apps/home XDG_DATA_DIRS=$apps/sys \
	python3 -I session/bin/big-wm-launcher --print "$1"; }
launcher apps | grep -qx $'calc\tCalculadora\tUtility' || fail "big-wm-launcher: falta a Calculadora em Acessórios"
launcher apps | grep -q 'Oculto' && fail "big-wm-launcher: mostrou um NoDisplay"
launcher apps wlmaker | grep -q wmusic && fail "big-wm-launcher: dockapp X11 no Wayland"
# (Os .desktop do flatpak do sistema também entram: só as categorias dos falsos são certas.)
cats=$(launcher categories | cut -f2 | tr '\n' '|')
for c in Acessórios "Aplicativos web" Configurações Escritório; do
	[[ $cats == *"|$c|"* || $cats == "$c|"* ]] || fail "big-wm-launcher: falta a categoria $c ($cats)"
done
[[ $cats == *"|Outros|" ]] || fail "big-wm-launcher: Outros deve ser a última categoria ($cats)"
launcher Settings | grep -qx $'conf\tAjustes' || fail "big-wm-launcher: Ajustes fora de Configurações"
grep -q '"kb-cancel"' data/rofi/big-wm.rasi && grep -q 'button-close' data/rofi/big-wm.rasi ||
	fail "rofi: tema sem o botão de fechar"
# O rofi junta duas strings na mesma linha do tema: uma propriedade por linha.
grep -n '"[^"]*";[^"]*"' data/rofi/big-wm.rasi && fail "rofi: duas strings numa linha do tema"
LANG=pt_BR.UTF-8 python3 -I session/bin/big-wm-appmenu --dockapps >"$apps/dockapps.plist"
python3 -I tests/plistcheck.py --wings "$apps/dockapps.plist" || fail "big-wm-appmenu --dockapps: plist inválido"
rm -rf "$apps"
pass big-wm-appmenu

# 6b1. big-wm-wprefs-sync: analisador de property lists, conversão do WPrefs
#      (inclusive arquivos gravados pelo próprio WPrefs) e mesclagem com a
#      configuração do wlmaker (tests/wprefs).
if python3 -I -m unittest discover -s tests/wprefs -t tests/wprefs >"${TMPDIR:-/tmp}/wprefs-tests.log" 2>&1; then
	pass "big-wm-wprefs-sync ($(grep -o 'Ran [0-9]* tests' "${TMPDIR:-/tmp}/wprefs-tests.log"))"
else
	cat "${TMPDIR:-/tmp}/wprefs-tests.log" >&2
	fail "big-wm-wprefs-sync (tests/wprefs)"
fi
rm -f "${TMPDIR:-/tmp}/wprefs-tests.log"
# O menu Wayland padrão chama o WPrefs e os estilos do Window Maker.
for m in data/wlmaker/RootMenu.plist data/wlmaker/RootMenu.plist.pt_BR; do
	grep -q 'ShellExecute, "WPrefs")' "$m" && grep -q 'big-wm-wprefs-sync --open-menu' "$m" ||
		fail "$m: falta o WPrefs ou Temas/Estilos"
done

# 6b2. big-wm-screenshot: grava em Capturas de tela nas duas sessões e, no
#      Wayland, o slurp não herda a entrada padrão (pelo menu do wlmaker ela
#      nunca termina e o slurp esperava para sempre).
shots=$(mktemp -d)
mkdir -p "$shots/GNUstep/Library/WindowMaker/Screenshots"
shot() { # sessão, modo
	# Funções exportadas: /tmp pode ser noexec, nada de programa falso no PATH.
	# shellcheck disable=SC2016 # o script entre aspas simples roda no bash -c
	env HOME="$shots" LANG=pt_BR.UTF-8 XDG_SESSION_TYPE="$1" bash -c '
		scrot() { printf png >"${!#}"; }
		grim() { printf png >"${!#}"; }
		slurp() { read -r -t 2 _; [ $? -gt 128 ] && { echo "slurp: entrada sem fim" >&2; return 1; }; echo "1,1 9x9"; }
		notify-send() { :; }
		sleep() { :; }
		export -f scrot grim slurp notify-send sleep
		exec bash session/bin/big-wm-screenshot "$1"' _ "$2" < <(command sleep 4)
}
for s in x11:full x11:window x11:area wayland:full wayland:window wayland:area; do
	rm -f "$shots"/GNUstep/Library/WindowMaker/Screenshots/*
	shot "${s%%:*}" "${s#*:}" && compgen -G "$shots/GNUstep/Library/WindowMaker/Screenshots/screenshot_*.png" >/dev/null ||
		fail "big-wm-screenshot $s: nenhuma captura gravada"
done
rm -rf "$shots"
bash session/bin/big-wm-screenshot >/dev/null 2>&1; [ $? = 2 ] || fail "big-wm-screenshot sem modo deve sair com 2"
pass "big-wm-screenshot (tela, janela e área nas duas sessões)"

# 6b3. big-wm-tray: segue a borda e os lançadores do Dock do wlmaker.
tray=$(mktemp -d)
traylayout() { python3 -I - "$1" <<'PY'
import ast, sys
src = open("session/bin/big-wm-tray").read()
tree = ast.parse(src)
keep = [n for n in tree.body if isinstance(n, (ast.FunctionDef, ast.Assign)) and
        (getattr(n, "name", None) in ("state_path", "plist_block", "dock_layout") or
         (isinstance(n, ast.Assign) and getattr(n.targets[0], "id", "") == "DEFAULT_DOCK"))]
ns = {}
exec(compile(ast.Module(body=[ast.parse("import os, re").body[0], ast.parse("import re").body[0]] + keep,
             type_ignores=[]), "big-wm-tray", "exec"), ns)
print(*ns["dock_layout"](sys.argv[1]))
PY
}
[ "$(traylayout data/wlmaker/State.plist)" = "RIGHT TOP 2" ] || fail "big-wm-tray: Dock padrão lido errado: $(traylayout data/wlmaker/State.plist)"
sed -e 's/Edge = RIGHT;/Edge = LEFT;/' -e 's/Anchor = TOP;/Anchor = BOTTOM;/' data/wlmaker/State.plist >"$tray/s.plist"
[ "$(traylayout "$tray/s.plist")" = "LEFT BOTTOM 2" ] || fail "big-wm-tray: Dock arrastado lido errado: $(traylayout "$tray/s.plist")"
[ "$(traylayout "$tray/nao-existe")" = "RIGHT TOP 2" ] || fail "big-wm-tray: sem State.plist deve usar o padrão"
rm -rf "$tray"
pass "big-wm-tray junto dos lançadores do Dock"

# 6c. Autostarts cortados só nas sessões Big WindowMaker.
cond() { env -i PATH="$PATH" XDG_CURRENT_DESKTOP="$1" XDG_SESSION_TYPE="${3:-x11}" \
	bash session/lib/big-wm-autostart-condition "$2"; }
cond KDE plasma || fail "autostart: outro ambiente não pode ser afetado"
cond XFCE bluetooth || fail "autostart: outro ambiente não pode ser afetado (bluetooth)"
cond WindowMaker plasma && fail "autostart: big-preload-user deve ser pulado"
# O Sound Switcher Indicator fica na bandeja das duas sessões: sem drop-in.
compgen -G 'session/systemd/autostart-conditions/*sound*' >/dev/null &&
	fail "autostart: indicator-sound-switcher não pode ser cortado"
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

# 8. Dock com abas (C + GTK 3): compilação, testes unitários, instalação e
#    regras de segurança. O diretório de build fica no repositório (o /tmp
#    pode ser noexec e os testes executam binários).
if command -v meson >/dev/null && pkg-config --exists gtk+-3.0 2>/dev/null; then
	bdir=${BTD_BUILDDIR:-$PWD/tabbed-dock/.build-check}
	if { [ -d "$bdir" ] || meson setup --prefix=/usr "$bdir" tabbed-dock >/dev/null; } &&
		meson compile -C "$bdir" >/dev/null && meson test -C "$bdir" --print-errorlogs >/dev/null; then
		pass "dock com abas: compilação e testes unitários"
	else
		fail "dock com abas: compilação ou testes unitários (ver $bdir/meson-logs)"
	fi
	ddest=$(mktemp -d)
	if meson install -C "$bdir" --destdir "$ddest" --no-rebuild >/dev/null 2>&1; then
		while IFS= read -r path; do
			rel=${path#"$ddest"}
			[[ $rel =~ $allowed ]] || fail "dock com abas: instalado fora dos caminhos permitidos: $rel"
		done < <(find "$ddest" \( -type f -o -type l \))
		for f in "$ddest"/etc/xdg/autostart/big-wm-tabbed-dock.desktop \
			"$ddest"/usr/share/applications/big-wm-tabbed-dock-preferences.desktop; do
			[ -e "$f" ] || { fail "dock com abas: falta ${f#"$ddest"}"; continue; }
			command -v desktop-file-validate >/dev/null &&
				desktop-file-validate "$f" 2>&1 | grep -v -E 'unregistered value "(WindowMaker|wlmaker)"' | grep . &&
				fail "dock com abas: ${f##*/} inválido"
		done
		[ -e "$ddest/usr/share/locale/pt_BR/LC_MESSAGES/big-wm-tabbed-dock.mo" ] || fail "dock com abas: tradução pt_BR não instalada"
		pass "dock com abas: caminhos instalados"
	else
		fail "dock com abas: meson install"
	fi
	rm -rf "$ddest"
	msgfmt -c -o /dev/null tabbed-dock/po/pt_BR.po || fail "dock com abas: pt_BR.po"
	# Nada de shell nem de system()/popen() para lançar aplicativos.
	if grep -n -E '\b(system|popen|execl|g_spawn_command_line_(async|sync))[[:space:]]*\(|"(/bin/)?sh"[[:space:]]*,[[:space:]]*"-c"' \
		tabbed-dock/src/*/*.c; then
		fail "dock com abas: chamada de shell no código"
	else
		pass "dock com abas: sem shell para lançar aplicativos"
	fi
else
	echo "aviso: meson/gtk3 ausentes, dock com abas não verificado" >&2
fi

if [ "$failures" -gt 0 ]; then
	echo "$failures verificação(ões) falharam" >&2
	exit 1
fi
echo "todas as verificações passaram"
