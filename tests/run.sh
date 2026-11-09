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
		sc session/lib/big-wm-pamac-tray &&
		sc session/zoom/big-wm-picom &&
		sc themes/boot/grub/gerar.sh themes/boot/plymouth/gerar.sh \
			themes/boot/plymouth/libalpm/plymouth-theme-big-wm &&
		sc -s bash pkgbuild/*.install &&
		sc -x -P SCRIPTDIR tests/run.sh tests/integration/*.sh tests/perf/*.sh; then
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
for f in "${python_bins[@]}" session/lib/big-wm-wmstyle session/lib/big-wm-x11-repaint session/lib/big-wm-dockapps-autolaunch session/lib/big-wm-dock-commands session/lib/big-wm-clip-add \
	tests/integration/*.py tests/perf/*.py; do
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
		session/wayland-sessions/*.desktop welcome/data/*.desktop 2>&1 |
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
	allowed='^/(usr/bin/(startbigwm|startbigwm-wayland|big-wm-[a-z-]+|grub-resolucao)$|usr/lib/systemd/system/big-wm-grub-resolucao\.service$|usr/lib/big-wm/|usr/lib/qt6/qml/kvantum/qmldir$|usr/lib/systemd/user/(big-wm-|(app-[^/]+@autostart|sync-gnome-theme-to-qt|pamac-tray)\.service\.d/big-wm\.conf$)|etc/xdg/autostart/big-wm-|etc/xdg/(windowmaker|wlmaker)-(mimeapps|xdg-terminals)\.list$|usr/share/(big-wm/|desktop-identity/big-wm\.conf$|xfce4/helpers/big-wm-terminal\.desktop$|backgrounds/big-wm/|icons/hicolor/[a-z0-9]+/apps/big-wm(-tabbed-dock)?\.(png|svg)$|themes/BigWindowMaker/|icons/BigWindowMaker/|slick-greeter/badges/big-wm(-wayland)?\.svg$|xsessions/big-wm|wayland-sessions/big-wm|xdg-desktop-portal/(windowmaker|wlmaker)-portals\.conf|wlmaker/icons/big-wm-|WindowMaker/(Themes|Styles|Icons)/|plymouth/themes/big-wm/|libalpm/(hooks/(80-plymouth-theme-big-wm-(antes|depois)|big-wm-name)\.hook|scripts/(plymouth-theme-big-wm|big-wm-name))$|applications/(big-wm-tabbed-dock-preferences|org\.communitybig\.BigWindowMaker\.About)\.desktop$|locale/[A-Za-z_@]+/LC_MESSAGES/big-wm-tabbed-dock\.mo$)|boot/grub/themes/big-wm/)'
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
	# Todo programa de session/bin vai para o pacote (o big-wm-clipboard
	# ficou de fora da lista do Makefile uma vez).
	for f in session/bin/*; do
		[ -x "$destdir/usr/bin/${f##*/}" ] || fail "make install não instala ${f##*/} (SESSION_BIN/WAYLAND_BIN)"
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

# 5b. grub-resolucao: EDID e sysfs sintéticos, grub-mkconfig falso que lê o
#     /etc/default/grub e o grub.d como o verdadeiro.
gr=$(mktemp -d)
grtool=themes/boot/grub/grub-resolucao
# monitor <nome> <LxA|""> [modos]: conector conectado com EDID (ou sem)
monitor() {
	local d=$gr/sys/class/drm/card0-$1
	mkdir -p "$d" && echo connected >"$d/status"
	printf '%s\n' "${@:3}" >"$d/modes"
	if [ -n "$2" ]; then
		python3 -I -c 'import sys
w, h = map(int, sys.argv[1].split("x"))
e = bytearray(128)
e[0:8] = b"\x00\xff\xff\xff\xff\xff\xff\x00"
e[54], e[55] = 0x02, 0x3a
e[56], e[58] = w & 0xff, (w >> 8) << 4
e[59], e[61] = h & 0xff, (h >> 8) << 4
sys.stdout.buffer.write(bytes(e))' "$2" >"$d/edid"
	else
		: >"$d/edid"
	fi
}
grreset() { # novo sistema falso; $1 = uefi|bios
	rm -rf "$gr/sys" "$gr/raiz"
	mkdir -p "$gr/sys/class/drm" "$gr/raiz/etc/default/grub.d" "$gr/raiz/boot/grub"
	[ "$1" = uefi ] && mkdir -p "$gr/sys/firmware/efi"
	printf 'GRUB_TIMEOUT=5\n# comentário do usuário\n' >"$gr/raiz/etc/default/grub"
	printf 'GRUB_GFXMODE="1024x768,800x600,auto"\n' >"$gr/raiz/etc/default/grub.d/big-wm-theme.cfg"
	echo quiet >"$gr/cmdline"
}
cat >"$gr/mkconfig" <<'EOF'
#!/bin/bash
[ -n "${FALHAR:-}" ] && { echo "erro de teste" >&2; exit 1; }
. "$RAIZ/etc/default/grub"
for x in "$RAIZ"/etc/default/grub.d/*.cfg; do [ -e "$x" ] && . "$x"; done
printf 'set gfxmode=%s\nset gfxpayload=%s\n' "$GRUB_GFXMODE" "${GRUB_GFXPAYLOAD_LINUX:-}" >"$2"
EOF
chmod +x "$gr/mkconfig"
grrun() {
	env BIG_WM_GRUB_SYSFS="$gr/sys" BIG_WM_GRUB_CMDLINE="$gr/cmdline" BIG_WM_GRUB_MKCONFIG="$gr/mkconfig" \
		RAIZ="$gr/raiz" bash "$grtool" --raiz "$gr/raiz" "$@" 2>>"$gr/log"
}
grcfg() { cat "$gr/raiz/boot/grub/grub.cfg" 2>/dev/null; }
nbackups() { find "$gr/raiz/var/lib/big-wm/grub-resolucao" -name 'backup-*' 2>/dev/null | wc -l; }
# Um monitor Full HD, UEFI: nativa, dois modos 16:9 menores, reservas, auto, keep.
grreset uefi
monitor HDMI-A-1 1920x1080 1920x1080 3840x2160
grrun --auto
[ "$(grcfg)" = "$(printf 'set gfxmode=1920x1080,1600x900,1366x768,1024x768,800x600,auto\nset gfxpayload=keep')" ] ||
	fail "grub-resolucao: Full HD no UEFI: $(grcfg)"
grep -q '^# comentário do usuário$' "$gr/raiz/etc/default/grub" || fail "grub-resolucao: mexeu no /etc/default/grub"
[ "$(stat -c %a "$gr/raiz/etc/default/grub.d/zz-big-wm-resolucao.cfg")" = 644 ] || fail "grub-resolucao: permissão do arquivo"
# Idempotente: a segunda vez não muda nada nem guarda outro backup.
grrun --auto
[ "$(nbackups)" = 1 ] || fail "grub-resolucao: segunda execução não foi idempotente ($(nbackups) backups)"
# Restaurar: volta ao que era (sem o arquivo) e regenera.
grrun --restaurar
[ ! -e "$gr/raiz/etc/default/grub.d/zz-big-wm-resolucao.cfg" ] &&
	[ "$(grcfg)" = "$(printf 'set gfxmode=1024x768,800x600,auto\nset gfxpayload=')" ] ||
	fail "grub-resolucao --restaurar: $(grcfg)"
grrun --restaurar
[ $? = 6 ] || fail "grub-resolucao --restaurar sem backup deveria sair com 6"
# Três monitores com o painel do notebook: o painel decide.
grreset uefi
monitor DP-1 3440x1440
monitor eDP-1 1920x1200
monitor HDMI-A-1 2560x1080
grrun --auto
grcfg | grep -qx 'set gfxmode=1920x1200,1680x1050,1440x900,1024x768,800x600,auto' ||
	fail "grub-resolucao: painel interno com monitores externos: $(grcfg)"
# Monitores externos (ultrawide e Full HD): o menor decide; BIOS sem keep.
grreset bios
monitor DP-1 3440x1440
monitor DP-2 1920x1080
grrun --auto
[ "$(grcfg)" = "$(printf 'set gfxmode=1920x1080,1600x900,1366x768,1024x768,800x600,auto\nset gfxpayload=')" ] ||
	fail "grub-resolucao: dois externos no BIOS: $(grcfg)"
# Ultrawide sozinho: 21:9 menor e as reservas 4:3.
grreset uefi
monitor DP-1 3440x1440
grrun --auto
grcfg | grep -qx 'set gfxmode=3440x1440,2560x1080,1024x768,800x600,auto' || fail "grub-resolucao: ultrawide: $(grcfg)"
# Sem EDID: o modo preferido do KMS; o GRUB_GFXPAYLOAD_LINUX do administrador fica.
grreset uefi
echo 'GRUB_GFXPAYLOAD_LINUX=text' >>"$gr/raiz/etc/default/grub"
monitor Virtual-1 "" 1280x800 1024x768
grrun --auto
[ "$(grcfg)" = "$(printf 'set gfxmode=1280x800,1024x768,800x600,auto\nset gfxpayload=text')" ] ||
	fail "grub-resolucao: sem EDID / payload do administrador: $(grcfg)"
# Nenhum monitor conhecido (máquina virtual sem EDID nem modos): nada muda.
grreset uefi
grrun --auto
[ ! -e "$gr/raiz/etc/default/grub.d/zz-big-wm-resolucao.cfg" ] || fail "grub-resolucao: gravou sem monitor"
# Falha do grub-mkconfig: a configuração anterior volta e o grub.cfg é refeito.
grreset uefi
monitor HDMI-A-1 1920x1080
printf 'GRUB_GFXMODE="1280x1024,auto"\n' >"$gr/raiz/etc/default/grub.d/zz-big-wm-resolucao.cfg"
FALHAR=1 grrun --auto
[ $? = 4 ] && [ "$(cat "$gr/raiz/etc/default/grub.d/zz-big-wm-resolucao.cfg")" = 'GRUB_GFXMODE="1280x1024,auto"' ] ||
	fail "grub-resolucao: falha do grub-mkconfig não restaurou a configuração anterior"
# Sem GRUB: nada muda (saída 3).
grreset uefi
monitor HDMI-A-1 1920x1080
rm -r "$gr/raiz/boot/grub"
grrun --auto
[ $? = 3 ] && [ ! -e "$gr/raiz/etc/default/grub.d/zz-big-wm-resolucao.cfg" ] || fail "grub-resolucao: sem GRUB deveria sair com 3"
# Sistema Live (sem --raiz): recusa antes de qualquer coisa.
echo 'BOOT_IMAGE=/boot/vmlinuz misobasedir=manjaro' >"$gr/cmdline"
env BIG_WM_GRUB_SYSFS="$gr/sys" BIG_WM_GRUB_CMDLINE="$gr/cmdline" bash "$grtool" --auto 2>/dev/null
[ $? = 5 ] || fail "grub-resolucao: no Live deveria sair com 5"
# --listar não grava nada.
grreset uefi
monitor HDMI-A-1 1920x1080
grrun --listar | grep -q 'Sugestão (monitor HDMI-A-1): GRUB_GFXMODE="1920x1080,' &&
	[ ! -e "$gr/raiz/etc/default/grub.d/zz-big-wm-resolucao.cfg" ] || fail "grub-resolucao --listar"
rm -rf "$gr"
pass "grub-resolucao (EDID, vários monitores, ultrawide, BIOS/UEFI, backup, restaurar, falhas, Live)"

# 6. big-wm-firstrun: aplica padrões numa conta vazia e não sobrescreve nada.
home=$(mktemp -d)
fakeroot=$destdir
run_firstrun() {
	HOME=$home XDG_CONFIG_HOME=$home/.config XDG_DATA_HOME=$home/.local/share LANG=$1 \
		BIGWM_LIBDIR=$fakeroot/usr/lib/big-wm \
		bash -c "sed -e 's|^datadir=.*|datadir=$fakeroot/usr/share/big-wm|' \\
			-e 's|^kvantumdir=.*|kvantumdir=$home/kvantum|' \\
			-e 's|^ashyterm=.*|ashyterm=$home/ashyterm|' \\
			-e 's|^ashyterm_frame=.*|ashyterm_frame=$home/ashyterm-lib/wm_frame.py|' \\
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
	# A versão anterior, intocada, é trocada; uma editada pelo usuário fica.
	cp tests/data/fastfetch-config-2026-11.jsonc "$home/.config/fastfetch/config.jsonc"
	run_firstrun pt_BR.UTF-8 x11 >/dev/null 2>&1
	cmp -s "$home/.config/fastfetch/config.jsonc" data/fastfetch/config.jsonc ||
		fail "firstrun não atualizou o fastfetch de fábrica (Desktop: Big WindowMaker)"
	{ cat tests/data/fastfetch-config-2026-11.jsonc; echo "// meu"; } >"$home/.config/fastfetch/config.jsonc"
	run_firstrun pt_BR.UTF-8 x11 >/dev/null 2>&1
	grep -q '^// meu$' "$home/.config/fastfetch/config.jsonc" || fail "firstrun trocou um fastfetch editado pelo usuário"
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
	sed -i -e '/"Alt+Print"/d' -e '/"Ctrl+Alt+Escape"/d' -e '/"Logo+V"/d' \
		-e '/"Logo+R"/d' -e '/"Alt+space"/d' -e '/"Ctrl+Shift+Escape"/d' "$wlconfig"
	run_firstrun pt_BR.UTF-8 wayland
	run_firstrun pt_BR.UTF-8 wayland
	[ "$(grep -c '"Ctrl+Alt+Escape" = (ShellExecute, "big-wm-kill");' "$wlconfig")" = 1 ] &&
		[ "$(grep -c '"Logo+V" = (ShellExecute, "big-wm-open clipboard");' "$wlconfig")" = 1 ] &&
		[ "$(grep -c '"Alt+Print" = (ShellExecute, "big-wm-screenshot window");' "$wlconfig")" = 1 ] &&
		[ "$(grep -c '"Logo+R" = (ShellExecute, "big-wm-open run");' "$wlconfig")" = 1 ] &&
		[ "$(grep -c '"Alt+space" = (ShellExecute, "big-wm-open apps");' "$wlconfig")" = 1 ] &&
		[ "$(grep -c '"Ctrl+Shift+Escape" = (ShellExecute, "big-wm-open taskmanager");' "$wlconfig")" = 1 ] &&
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
	# Monitores: com o Big WM Display Resolution instalado, a linha padrão do
	# ARandR num menu editado passa a abri-lo; sem ele, nada muda.
	sed -i 's/^  ("Meu item", EXEC, "meu-programa")/    (Monitores, EXEC, arandr),\n&/' "$old_menu"
	run_firstrun pt_BR.UTF-8 x11
	grep -q '(Monitores, EXEC, arandr),' "$old_menu" || fail "firstrun trocou o ARandR sem o big-wm-display-resolution"
	mkdir -p "$home/bin"
	printf '#!/bin/sh\n' >"$home/bin/big-wm-display-resolution"
	chmod +x "$home/bin/big-wm-display-resolution"
	PATH="$home/bin:$PATH" run_firstrun pt_BR.UTF-8 x11
	grep -q '(Monitores, EXEC, "big-wm-display-resolution"),' "$old_menu" &&
		grep -q '"Meu item", EXEC, "meu-programa"' "$old_menu" &&
		python3 -I tests/plistcheck.py --wings "$old_menu" ||
		fail "firstrun não pôs o big-wm-display-resolution em Monitores no menu editado"
	sed -i '/(Monitores, EXEC, "big-wm-display-resolution"),/d' "$old_menu"
	rm -rf "${home:?}/bin"
	# Menu padrão da versão anterior (sem Captura de tela, Executar no %A e
	# sem Forçar o fechamento), sem alteração: dá lugar ao novo.
	sed -e '/^  ($/{N;/"Captura de tela"/{:a;N;/\n  ),$/!ba;d}}' -e '/big-wm-kill/d' -e '/big-wm-theme/d' -e '/big-wm-open clipboard/d' -e '/RESET_DOCK/d' -e '/big-wm-locate-pointer/d' -e '/big-wm-lock-settings/d' -e '/big-wm-welcome/d' -e '/big-wm-tabbed-dock --preferences/d' -e 's/^    (Lupa, EXEC, wmagnify),$/    (Lupa, EXEC, wmagnify)/' -e 's%OPEN_PLMENU, "|| big-wm-appmenu --icon-themes"),%OPEN_MENU, "-noext /usr/share/WindowMaker/IconSets ~/GNUstep/Library/WindowMaker/IconSets WITH seticons"),%' \
		-e '/^    ($/{N;/"Ajuste do papel de parede"\|"Organização de janelas"/{:b;N;/\n    ),$/!bb;d}}' -e 's#WITH big-wm-wallpaper"),#WITH wmsetbg -u -s"),#' \
		-e 's#("Executar...", SHORTCUT, "Mod4+r", EXEC, "big-wm-open run"),#("Executar...", EXEC, "%A(Executar, Digite o comando:)"),#' \
		-e 's#SHORTCUT, "Mod1+space", EXEC, "big-wm-open apps"#SHORTCUT, "Mod4+space", EXEC, "big-wm-open apps"#' \
		-e '/big-wm-open taskmanager/d' -e 's#(Monitores, EXEC, "big-wm-display-resolution")#(Monitores, EXEC, arandr)#' \
		data/GNUstep/Defaults/WMRootMenu.pt_BR >"$old_menu"
	run_firstrun pt_BR.UTF-8 x11
	cmp -s "$old_menu" data/GNUstep/Defaults/WMRootMenu.pt_BR ||
		fail "firstrun não trocou o menu X11 padrão antigo pelo novo"
	# Menus padrão de antes das Preferências do Dock com Abas: dão lugar aos
	# novos. Um menu editado ganha a linha uma vez, abaixo do Window Maker
	# (só com o big-wm-tabbed-dock instalado); tirada depois, não volta. (O
	# menu do wlmaker pode sair convertido do WMRootMenu editado pelo
	# big-wm-wprefs-sync que o big-wm-theme chama: sem aspas no WPrefs.)
	tdbin=$home/td-bin && mkdir -p "$tdbin" && printf '#!/bin/sh\n' >"$tdbin/big-wm-tabbed-dock" && chmod +x "$tdbin/big-wm-tabbed-dock"
	grep -v 'big-wm-tabbed-dock --preferences' data/GNUstep/Defaults/WMRootMenu.pt_BR >"$old_menu"
	PATH=$tdbin:$PATH run_firstrun pt_BR.UTF-8 x11
	cmp -s "$old_menu" data/GNUstep/Defaults/WMRootMenu.pt_BR ||
		fail "firstrun não trocou o menu X11 sem as Preferências do Dock com Abas"
	wl_menu=$home/.config/wlmaker/RootMenu.plist
	grep -v 'big-wm-tabbed-dock --preferences' data/wlmaker/RootMenu.plist.pt_BR >"$wl_menu"
	PATH=$tdbin:$PATH run_firstrun pt_BR.UTF-8 wayland
	cmp -s "$wl_menu" data/wlmaker/RootMenu.plist.pt_BR ||
		fail "firstrun não trocou o menu Wayland sem as Preferências do Dock com Abas"
	rm -f "$home"/.config/big-wm/migrations/tabbed-dock-menu-*
	grep -v 'big-wm-tabbed-dock --preferences' data/GNUstep/Defaults/WMRootMenu.pt_BR |
		sed 's#^  (Navegador, EXEC, "big-wm-open browser"),$#&\n  ("Meu item", EXEC, "meu-programa"),#' >"$old_menu"
	grep -v 'big-wm-tabbed-dock --preferences' data/wlmaker/RootMenu.plist.pt_BR |
		sed 's#^ ("Navegador", ShellExecute, "big-wm-open browser"),$#&\n ("Meu item", ShellExecute, "meu-programa"),#' >"$wl_menu"
	PATH=$tdbin:$PATH run_firstrun pt_BR.UTF-8 x11
	PATH=$tdbin:$PATH run_firstrun pt_BR.UTF-8 x11
	PATH=$tdbin:$PATH run_firstrun pt_BR.UTF-8 wayland
	PATH=$tdbin:$PATH run_firstrun pt_BR.UTF-8 wayland
	[ "$(grep -c 'big-wm-tabbed-dock --preferences' "$old_menu")" = 1 ] &&
		grep -A1 '("Window Maker", EXEC, WPrefs),' "$old_menu" |
		grep -qF '    ("Preferências do Dock com Abas", EXEC, "big-wm-tabbed-dock --preferences"),' &&
		grep -q '"Meu item"' "$old_menu" && python3 -I tests/plistcheck.py --wings "$old_menu" ||
		fail "firstrun: Preferências do Dock com Abas no menu X11 editado: $(grep -n -A2 WPrefs "$old_menu")"
	[ "$(grep -c 'big-wm-tabbed-dock --preferences' "$wl_menu")" = 1 ] &&
		grep -A1 -E '\("Window Maker", ShellExecute, "?WPrefs"?\),' "$wl_menu" |
		grep -qF '  ("Preferências do Dock com Abas", ShellExecute, "big-wm-tabbed-dock --preferences"),' &&
		grep -q '"Meu item"' "$wl_menu" && python3 -I tests/plistcheck.py "$wl_menu" ||
		fail "firstrun: Preferências do Dock com Abas no menu Wayland editado: $(grep -n -A2 WPrefs "$wl_menu")"
	sed -i '/big-wm-tabbed-dock --preferences/d' "$old_menu"
	PATH=$tdbin:$PATH run_firstrun pt_BR.UTF-8 x11
	grep -q 'big-wm-tabbed-dock --preferences' "$old_menu" && fail "firstrun: Preferências do Dock com Abas tiradas pelo usuário voltaram"
	cp data/wlmaker/RootMenu.plist.pt_BR "$wl_menu"
	# Menu e preferências de antes dos atalhos de novembro de 2026, com um
	# item do usuário: Super+R, Alt+Espaço, Ctrl+Shift+Esc e as teclas Print
	# pelo menu entram uma vez; o item do usuário fica.
	git show 41280d3:data/GNUstep/Defaults/WMRootMenu.pt_BR 2>/dev/null |
		sed 's#^  (Navegador, EXEC, "big-wm-open browser"),$#&\n  ("Meu item", EXEC, "meu-programa"),#' >"$old_menu"
	if [ -s "$old_menu" ]; then
		prefs=$home/GNUstep/Defaults/WindowMaker
		cp "$prefs" "$home/prefs.orig"
		sed -i -e 's/^  ScreenCaptureKey = .*/  ScreenCaptureKey = Print;/' \
			-e 's/^  WindowCaptureKey = .*/  WindowCaptureKey = "Mod1+Print";/' \
			-e 's/^  PartialCaptureKey = .*/  PartialCaptureKey = "Shift+Print";/' "$prefs"
		run_firstrun pt_BR.UTF-8 x11
		run_firstrun pt_BR.UTF-8 x11
		{
			grep -q '"Meu item", EXEC, "meu-programa"' "$old_menu" &&
				[ "$(grep -c '"Mod4+r", EXEC, "big-wm-open run")' "$old_menu")" = 1 ] &&
				[ "$(grep -c '"Mod1+space", EXEC, "big-wm-open apps")' "$old_menu")" = 1 ] &&
				[ "$(grep -c '("Gerenciador de tarefas", SHORTCUT, "Control+Shift+Escape", EXEC, "big-wm-open taskmanager"),' "$old_menu")" = 1 ] &&
				grep -q '("Tela inteira", SHORTCUT, Print, EXEC, "big-wm-screenshot full")' "$old_menu" &&
				grep -q '(Janela, SHORTCUT, "Mod1+Print", EXEC, "big-wm-screenshot window")' "$old_menu" &&
				grep -q '("Área", SHORTCUT, "Shift+Print", EXEC, "big-wm-screenshot area")' "$old_menu" &&
				[ "$(grep -c 'CaptureKey = None;' "$prefs")" = 3 ] &&
				python3 -I tests/plistcheck.py --wings "$old_menu"
		} || fail "firstrun não migrou os atalhos de novembro de 2026 no menu editado"
		# picom.conf padrão de uma versão anterior vira o atual (vsync); um
	# editado fica.
	git show 9befe49:data/picom.conf >"$home/.config/picom/picom.conf" 2>/dev/null &&
		{ run_firstrun pt_BR.UTF-8 x11
		cmp -s "$home/.config/picom/picom.conf" data/picom.conf ||
			fail "firstrun não atualizou o picom.conf padrão antigo"; }
	echo '# meu' >>"$home/.config/picom/picom.conf"
	run_firstrun pt_BR.UTF-8 x11
	tail -n1 "$home/.config/picom/picom.conf" | grep -qx '# meu' || fail "firstrun trocou o picom.conf editado"
	# Regra do obscurer do xsecurelock (aviso ao desbloquear): entra uma vez
	# num picom.conf com rules que não a tem, mesmo editado; tirada pelo
	# usuário depois, não volta.
	grep -q "class_g = 'xsecurelock' && class_i = 'obscurer'\"; opacity = 0; fade = false;" data/picom.conf ||
		fail "picom.conf padrão sem a regra do obscurer do xsecurelock"
	grep -v "class_i = 'obscurer'" data/picom.conf >"$home/.config/picom/picom.conf"
	echo '# meu' >>"$home/.config/picom/picom.conf"
	rm -f "$home/.config/big-wm/migracao-picom-obscurer"
	run_firstrun pt_BR.UTF-8 x11
	run_firstrun pt_BR.UTF-8 x11
	[ "$(grep -c "class_i = 'obscurer'" "$home/.config/picom/picom.conf")" = 1 ] &&
		grep -A1 -x 'rules = (' "$home/.config/picom/picom.conf" | grep -q "class_i = 'obscurer'" &&
		tail -n1 "$home/.config/picom/picom.conf" | grep -qx '# meu' ||
		fail "firstrun: regra do obscurer no picom.conf: $(grep -n -A1 'rules = (' "$home/.config/picom/picom.conf")"
	sed -i "/class_i = 'obscurer'/d" "$home/.config/picom/picom.conf"
	run_firstrun pt_BR.UTF-8 x11
	grep -q "class_i = 'obscurer'" "$home/.config/picom/picom.conf" && fail "firstrun: regra do obscurer tirada pelo usuário voltou"
	rm -f "$home"/.config/picom/picom.conf.bak-*
	cp data/picom.conf "$home/.config/picom/picom.conf"
	# Ícones com margem (windowmaker 10): o NO das contas antigas vira YES
		# uma vez; o NO escolhido depois fica.
		rm -f "$home/.config/big-wm/migracao-margem-dos-icones"
		sed -i 's/^  EnforceIconMargin = .*;$/  EnforceIconMargin = NO;/' "$prefs"
		run_firstrun pt_BR.UTF-8 x11
		grep -qx '  EnforceIconMargin = YES;' "$prefs" || fail "firstrun não ligou a margem dos ícones na conta antiga"
		sed -i 's/^  EnforceIconMargin = .*;$/  EnforceIconMargin = NO;/' "$prefs"
		run_firstrun pt_BR.UTF-8 x11
		grep -qx '  EnforceIconMargin = NO;' "$prefs" || fail "firstrun religou a margem dos ícones que o usuário desligou"
		# Tecla Print escolhida pelo usuário nas preferências: nada muda.
		cp "$home/prefs.orig" "$prefs"
		sed -i 's/^  ScreenCaptureKey = .*/  ScreenCaptureKey = "Mod4+Print";/' "$prefs"
		git show 41280d3:data/GNUstep/Defaults/WMRootMenu.pt_BR | sed 's#^  (Navegador, EXEC, "big-wm-open browser"),$#&\n  ("Meu item", EXEC, "meu-programa"),#' >"$old_menu"
		run_firstrun pt_BR.UTF-8 x11
		grep -q 'SHORTCUT, Print,' "$old_menu" && fail "firstrun pôs Print no menu com a tecla do usuário nas preferências"
		cp "$home/prefs.orig" "$prefs"
	fi
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
	# Firefox: o líder do grupo das janelas (firefox.Firefox), pelo qual o
	# Window Maker escolhe o ícone do aplicativo e reconhece o lançador do
	# Clip, ganha o ícone escolhido para Navigator.firefox; o lançador
	# "Navigator.firefox" de versões anteriores passa a "firefox.Firefox".
	grep -qF 'firefox.Firefox = { Icon = "big-wm-netscape.png"; AlwaysUserIcon = YES; };' \
		<<<"$(tr -s ' \n' ' ' <"$attrs")" || fail "firstrun: firefox.Firefox sem o ícone escolhido para Navigator.firefox"
	sed 's/Name = firefox\.Firefox;/Name = Navigator.firefox;/' data/GNUstep/Defaults/WMState >"$home/GNUstep/Defaults/WMState"
	run_firstrun pt_BR.UTF-8 x11
	grep -q 'Name = firefox\.Firefox;' "$home/GNUstep/Defaults/WMState" &&
		! grep -q 'Navigator\.firefox' "$home/GNUstep/Defaults/WMState" ||
		fail "firstrun: lançador do Firefox no Clip continua Navigator.firefox"
	# Um ícone do usuário já gravado para firefox.Firefox fica.
	sed -i '/^  firefox\.Firefox = {$/,/};/{/big-wm-netscape/d}; s#^  firefox\.Firefox = {$#&\n    Icon = "/home/usuario/fx.png";#' "$attrs"
	run_firstrun pt_BR.UTF-8 x11
	grep -qF 'firefox.Firefox = { Icon = "/home/usuario/fx.png"; AlwaysUserIcon = YES; };' \
		<<<"$(tr -s ' \n' ' ' <"$attrs")" || fail "firstrun: sobrescreveu o ícone do usuário em firefox.Firefox: $(tr -s ' \n' ' ' <"$attrs")"

	# Salvar sessão: componentes da própria sessão (Dock com abas, launcher,
	# Lupa) marcados DontSaveSession; numa conta anterior, acrescentados
	# uma vez, sem duplicar o que já existe.
	for c in big-wm-tabbed-dock.Big-wm-tabbed-dock big-wm-launcher.BigWMLauncher magnify.Magnify; do
		grep -qE "\"?${c//./\\.}\"? = \{[^}]*DontSaveSession = YES;[^}]*\};" <<<"$(tr -s ' \n' ' ' <data/GNUstep/Defaults/WMWindowAttributes)" ||
			fail "WMWindowAttributes padrão: $c sem DontSaveSession"
	done
	rm -f "$home/.config/big-wm/session-attrs"
	run_firstrun pt_BR.UTF-8 x11
	run_firstrun pt_BR.UTF-8 x11
	python3 -I tests/plistcheck.py --wings "$attrs" || fail "firstrun: atributos inválidos depois de Salvar sessão"
	[ "$(grep -c 'DontSaveSession = YES' "$attrs")" = 3 ] ||
		fail "firstrun: DontSaveSession dos componentes da sessão: $(tr -s ' \n' ' ' <"$attrs")"

	# AshyTerm com a barra do Window Maker: só quando o AshyTerm instalado
	# esconde os próprios botões de janela (wm_frame.py), uma vez só.
	run_firstrun pt_BR.UTF-8 x11
	grep -q 'org\.communitybig\.ashyterm' "$attrs" && fail "firstrun: AshyTerm sem wm_frame.py ganhou IgnoreDecorationChanges"
	mkdir -p "$home/ashyterm-lib" && touch "$home/ashyterm-lib/wm_frame.py"
	run_firstrun pt_BR.UTF-8 x11
	run_firstrun pt_BR.UTF-8 x11
	python3 -I tests/plistcheck.py --wings "$attrs" || fail "firstrun: atributos inválidos depois do AshyTerm"
	[ "$(grep -c 'org\.communitybig\.ashyterm' "$attrs")" = 1 ] &&
		grep -qF '"org.communitybig.ashyterm" = { IgnoreDecorationChanges = YES; };' <<<"$(tr -s ' \n' ' ' <"$attrs")" ||
		fail "firstrun: AshyTerm sem a barra do Window Maker: $(tr -s ' \n' ' ' <"$attrs")"
	sed -i '/"org\.communitybig\.ashyterm" = {/,/};/d' "$attrs"
	run_firstrun pt_BR.UTF-8 x11
	grep -q 'org\.communitybig\.ashyterm' "$attrs" && fail "firstrun: atributo do AshyTerm apagado pelo usuário voltou"

	# Super+V (histórico da área de transferência): um menu da versão
	# anterior ganha a linha e fica igual ao padrão novo; rodar de novo não
	# duplica; um menu que já usa Mod4+v para outra coisa fica como está.
	menu=$home/GNUstep/Defaults/WMRootMenu
	grep -v 'big-wm-open clipboard' data/GNUstep/Defaults/WMRootMenu.pt_BR >"$menu"
	run_firstrun pt_BR.UTF-8 x11
	run_firstrun pt_BR.UTF-8 x11
	cmp -s "$menu" data/GNUstep/Defaults/WMRootMenu.pt_BR || fail "firstrun: Super+V não entrou no menu anterior como no padrão"
	grep -v 'big-wm-open clipboard' data/GNUstep/Defaults/WMRootMenu.pt_BR |
		sed 's/("Bloquear tela", SHORTCUT, "Mod4+l"/("Bloquear tela", SHORTCUT, "Mod4+v"/' >"$menu"
	cp "$menu" "$home/menu-conflito"
	run_firstrun pt_BR.UTF-8 x11
	cmp -s "$menu" "$home/menu-conflito" || fail "firstrun: Super+V entrou num menu que já usava Mod4+v"
	cp data/GNUstep/Defaults/WMRootMenu.pt_BR "$menu"

	# Terminal do Thunar/exo: sem o xfce4-mime-helper o exo só lê o
	# helpers.rc; o firstrun aponta para o big-wm-terminal sem trocar uma
	# escolha existente.
	PATH=$PWD/session/bin:$PATH run_firstrun pt_BR.UTF-8 x11
	grep -qx 'TerminalEmulator=big-wm-terminal' "$home/.config/xfce4/helpers.rc" ||
		fail "firstrun: helpers.rc sem TerminalEmulator=big-wm-terminal"
	printf 'TerminalEmulator=kitty\n' >"$home/.config/xfce4/helpers.rc"
	PATH=$PWD/session/bin:$PATH run_firstrun pt_BR.UTF-8 x11
	[ "$(cat "$home/.config/xfce4/helpers.rc")" = TerminalEmulator=kitty ] ||
		fail "firstrun: trocou o terminal escolhido no helpers.rc"
	# wmcliphist: .wmcliphistrc vazio (o wmcliphist cria um assim) ganha a
	# CLIPBOARD; "persist no" só com um binário que o entende; um arquivo do
	# usuário fica como está.
	wb=$(mktemp -d)
	printf '#!/bin/sh\n# persist\n' >"$wb/wmcliphist"
	chmod +x "$wb/wmcliphist"
	: >"$home/.wmcliphistrc"
	PATH=$wb:$PATH run_firstrun pt_BR.UTF-8 x11
	grep -qx 'clipboard CLIPBOARD' "$home/.wmcliphistrc" && grep -qx 'persist no' "$home/.wmcliphistrc" ||
		fail "firstrun: .wmcliphistrc sem a CLIPBOARD"
	printf '#!/bin/sh\n' >"$wb/wmcliphist"
	: >"$home/.wmcliphistrc"
	PATH=$wb:$PATH run_firstrun pt_BR.UTF-8 x11
	grep -q 'persist' "$home/.wmcliphistrc" && fail "firstrun: persist para um wmcliphist que não o entende"
	echo 'clipboard PRIMARY' >"$home/.wmcliphistrc"
	PATH=$wb:$PATH run_firstrun pt_BR.UTF-8 x11
	[ "$(cat "$home/.wmcliphistrc")" = 'clipboard PRIMARY' ] || fail "firstrun: trocou o .wmcliphistrc do usuário"
	rm -rf "$wb"
	# Restaurar posição do Dock: um menu sem o item o ganha uma vez.
	grep -v RESET_DOCK data/GNUstep/Defaults/WMRootMenu.pt_BR >"$home/GNUstep/Defaults/WMRootMenu"
	run_firstrun pt_BR.UTF-8 x11
	run_firstrun pt_BR.UTF-8 x11
	cmp -s "$home/GNUstep/Defaults/WMRootMenu" data/GNUstep/Defaults/WMRootMenu.pt_BR ||
		fail "firstrun: Restaurar posição do Dock não entrou no menu como no padrão"
	# Fundo suave: NO (gravado pelo menu antigo) passa a YES uma vez; quem
	# volta a NO depois fica com NO.
	prefs=$home/GNUstep/Defaults/WindowMaker
	sed -i 's/^\([[:space:]]*SmoothWorkspaceBack = \)YES;/\1NO;/' "$prefs"
	rm -f "$home/.config/big-wm/migrations/smooth-background"
	run_firstrun pt_BR.UTF-8 x11
	grep -q '^[[:space:]]*SmoothWorkspaceBack = YES;' "$prefs" || fail "firstrun: SmoothWorkspaceBack continua NO"
	sed -i 's/^\([[:space:]]*SmoothWorkspaceBack = \)YES;/\1NO;/' "$prefs"
	run_firstrun pt_BR.UTF-8 x11
	grep -q '^[[:space:]]*SmoothWorkspaceBack = NO;' "$prefs" || fail "firstrun: desfez a escolha SmoothWorkspaceBack = NO do usuário"
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
	# State.plist de antes da bandeja como dockapp e das gavetas: ganha
	# Dock.Dockapps = ("big-wm-tray") e Dock.DrawerIcon, uma vez só.
	python3 -I -c 'import re, sys
t = open(sys.argv[1], encoding="utf-8").read()
t = re.sub(r"\n    Dockapps = \(\n(?:      [^\n]*\n)*?    \);", "", t, count=1)
sys.stdout.write("\n".join(l for l in t.split("\n") if "DrawerIcon" not in l))' data/wlmaker/State.plist >"$wlstate"
	rm -f "$home/.config/big-wm/dock-dockapps-x11-wayland"
	run_firstrun pt_BR.UTF-8 wayland
	run_firstrun pt_BR.UTF-8 wayland
	python3 -I tests/plistcheck.py "$wlstate" || fail "State.plist migrado (bandeja) inválido"
	[ "$(grep -c 'DrawerIcon = ' "$wlstate")" = 1 ] &&
		[ "$(sed -n '/^  Dock = {$/,/^  };$/p' "$wlstate" | grep -c -e '^    Dockapps = (wmclock, "big-wm-tray", wmix,' -e DrawerIcon)" = 2 ] ||
		fail "firstrun não pôs a bandeja (com os dockapps do X11) e o ícone das gavetas no Dock do wlmaker"
	# Uma lista mudada pelo usuário fica como está.
	sed -i 's/^    Dockapps = (wmclock, .*);$/    Dockapps = ("big-wm-tray", wmcube);/' "$wlstate"
	rm -f "$home/.config/big-wm/dock-dockapps-x11-wayland"
	run_firstrun pt_BR.UTF-8 wayland
	grep -q '^    Dockapps = ("big-wm-tray", wmcube);$' "$wlstate" ||
		fail "firstrun trocou a lista de dockapps do Dock mudada pelo usuário"
	grep -q 'Command = "big-wm-systemtray";' "$home/GNUstep/Defaults/WMState" ||
		fail "WMState padrão sem a bandeja pelo big-wm-systemtray"
	# WMState de uma versão anterior: a bandeja passa ao big-wm-systemtray;
	# um wmsystemtray com opções do usuário fica.
	sed -i 's/Command = "big-wm-systemtray";/Command = wmsystemtray;/' "$home/GNUstep/Defaults/WMState"
	run_firstrun pt_BR.UTF-8 x11
	grep -q 'Command = "big-wm-systemtray";' "$home/GNUstep/Defaults/WMState" ||
		fail "firstrun não migrou a bandeja do Dock para o big-wm-systemtray"
	sed -i 's/Command = "big-wm-systemtray";/Command = "wmsystemtray -s";/' "$home/GNUstep/Defaults/WMState"
	run_firstrun pt_BR.UTF-8 x11
	grep -q 'Command = "wmsystemtray -s";' "$home/GNUstep/Defaults/WMState" ||
		fail "firstrun trocou o wmsystemtray com opções do usuário"
	echo usuario >"$home/GNUstep/Defaults/WindowMaker"
	printf '[General]\ntheme=BigAdwaitaRoundGtk\n' >"$home/.config/Kvantum/kvantum.kvconfig"
	run_firstrun en_US.UTF-8 x11
	grep -qx 'theme=QTStepOpenStep' "$home/.config/Kvantum/kvantum.kvconfig" ||
		fail "firstrun manteve o BigAdwaitaRoundGtk (não é escolha do usuário) no Kvantum"
	printf '[General]\ntheme=KvFlat\n' >"$home/.config/Kvantum/kvantum.kvconfig"
	echo '{"transparency": 0}' >"$home/.config/ashyterm/settings.json"
	run_firstrun en_US.UTF-8 x11
	[ "$(cat "$home/GNUstep/Defaults/WindowMaker")" = usuario ] || fail "firstrun sobrescreveu configuração do usuário"
	grep -qx 'theme=KvFlat' "$home/.config/Kvantum/kvantum.kvconfig" || fail "firstrun trocou o tema do Kvantum escolhido pelo usuário"
	grep -qx '{"transparency": 0}' "$home/.config/ashyterm/settings.json" ||
		fail "firstrun mexeu na configuração do AshyTerm do usuário"
	# Moldura do Window Maker em todas as janelas: Config.plist e
	# WMWindowAttributes novos já têm; anteriores ganham uma vez (sem
	# repetir nem voltar depois de apagado); botões só os do Window Maker;
	# Chrome sem escolha ganha a barra do sistema, com escolha ou aberto não.
	grep -q '^        "\*" = { ServerDecoration = Yes; };$' "$home/.config/wlmaker/Config.plist" ||
		fail "Config.plist novo sem a moldura do Window Maker em todas as janelas"
	sed -n '/^  "\*" = {$/,/^  };$/p' "$fakeroot/usr/share/big-wm/GNUstep/Defaults/WMWindowAttributes" |
		grep -q 'IgnoreDecorationChanges = YES;' ||
		fail "WMWindowAttributes sem a moldura do Window Maker em todas as janelas"
	grep -qx 'gtk-decoration-layout=:' "$home/.config/gtk-3.0/settings.ini" ||
		fail "settings.ini do GTK com os botões de janela do programa"
	printf '{\n    WindowAttributes = {\n        steam = { NoCloseButton = Yes; };\n    };\n}\n' >"$home/.config/wlmaker/Config.plist"
	printf '{\n  xterm.XTerm = {\n    NoTitlebar = YES;\n  };\n}\n' >"$home/GNUstep/Defaults/WMWindowAttributes"
	rm -f "$home/.config/big-wm/migrations/wayland-server-decoration-all" \
		"$home/.config/big-wm/migrations/x11-frames-all"
	mkdir -p "$home/.config/google-chrome/Default" "$home/.config/google-chrome/Profile 1" \
		"$home/.config/chromium/Default"
	echo '{"browser":{"show_home_button":true}}' >"$home/.config/google-chrome/Default/Preferences"
	echo '{"browser":{"custom_chrome_frame":true}}' >"$home/.config/google-chrome/Profile 1/Preferences"
	echo '{}' >"$home/.config/chromium/Default/Preferences"
	ln -s host-1 "$home/.config/chromium/SingletonLock"
	run_firstrun en_US.UTF-8 wayland
	run_firstrun en_US.UTF-8 x11
	run_firstrun en_US.UTF-8 wayland
	run_firstrun en_US.UTF-8 x11
	[ "$(grep -c 'ServerDecoration = Yes' "$home/.config/wlmaker/Config.plist")" = 1 ] &&
		grep -q 'steam = { NoCloseButton = Yes; };' "$home/.config/wlmaker/Config.plist" ||
		fail "firstrun não pôs a moldura do Window Maker no \"*\" do Config.plist (ou mexeu no resto)"
	[ "$(grep -c 'IgnoreDecorationChanges = YES;' "$home/GNUstep/Defaults/WMWindowAttributes")" = 1 ] &&
		grep -q 'NoTitlebar = YES;' "$home/GNUstep/Defaults/WMWindowAttributes" ||
		fail "firstrun não pôs a moldura do Window Maker no \"*\" do WMWindowAttributes (ou mexeu no resto)"
	sed -i '/ServerDecoration/d' "$home/.config/wlmaker/Config.plist"
	run_firstrun en_US.UTF-8 wayland
	grep -q 'ServerDecoration' "$home/.config/wlmaker/Config.plist" &&
		fail "firstrun devolveu a moldura apagada pelo usuário"
	python3 -c 'import json, sys
a, b, c = (json.load(open(p)) for p in sys.argv[1:])
assert a["browser"] == {"show_home_button": True, "custom_chrome_frame": False}, a
assert b["browser"]["custom_chrome_frame"] is True and c == {}, (b, c)' \
		"$home/.config/google-chrome/Default/Preferences" \
		"$home/.config/google-chrome/Profile 1/Preferences" \
		"$home/.config/chromium/Default/Preferences" ||
		fail "firstrun: barra de título do sistema no Chrome"
	pass big-wm-firstrun
else
	fail "big-wm-firstrun"
fi
rm -rf "$home"

# 6c. big-wm-theme: modo claro/escuro numa conta falsa, ida e volta, sem
#     tocar a sessão de quem roda os testes (gsettings, systemctl, D-Bus,
#     dunstctl e logger são substitutos que só anotam).
th=$(mktemp -d)
th_home=$th/home
mkdir -p "$th/run" "$th/kvantum/QTStepOpenStep" "$th/kvantum/QTStepOpenStepDark" \
	"$th/themes/OneStepBack/gtk-3.0" "$th/themes/OneStepBack-Dark/gtk-3.0" \
	"$th/icons/bigicons-papient" "$th/icons/bigicons-papient-dark"
touch "$th/icons/bigicons-papient/index.theme" "$th/icons/bigicons-papient-dark/index.theme"
echo default >"$th/color-scheme"
echo OneStepBack >"$th/gtk-theme"
# Substitutos como funções exportadas (o /tmp pode ser noexec), chamados só
# pelos scripts testados (SC2317, SC2329). As aspas simples dos "bash -c" abaixo são
# código para o bash filho (SC2016).
export BIGWM_TEST_TH=$th
# shellcheck disable=SC2317,SC2329
{
	# Uma chave por arquivo ($th/color-scheme, $th/icon-theme); sem o
	# arquivo, o padrão do esquema (icon-theme: Adwaita).
	gsettings() {
		echo "gsettings $*" >>"$BIGWM_TEST_TH/calls"
		# gtk-theme do gsettings separado do GTK_THEME do systemctl abaixo
		local key=$3
		[ "$key" = gtk-theme ] && key=gs-gtk-theme
		case $1 in
		get) if [ -e "$BIGWM_TEST_TH/$key" ]; then echo "'$(cat "$BIGWM_TEST_TH/$key")'"; else echo "'Adwaita'"; fi ;;
		set) echo "$4" >"$BIGWM_TEST_TH/$key" ;;
		esac
	}
	# dconf read: só o que foi gravado ($th/dconf-<chave>).
	dconf() { [ "$1" = read ] && cat "$BIGWM_TEST_TH/dconf-${2##*/}" 2>/dev/null; return 0; }
	systemctl() {
		echo "systemctl $*" >>"$BIGWM_TEST_TH/calls"
		[ "$2" = show-environment ] && echo "GTK_THEME=$(cat "$BIGWM_TEST_TH/gtk-theme")"
		return 0
	}
	dbus-update-activation-environment() {
		echo "dbus-update-activation-environment $*" >>"$BIGWM_TEST_TH/calls"
		echo "${2#GTK_THEME=}" >"$BIGWM_TEST_TH/gtk-theme"
	}
	dunstctl() { echo "dunstctl $*" >>"$BIGWM_TEST_TH/calls"; }
	# Um dunst desta sessão: o big-wm-theme confere o DISPLAY no environ.
	pgrep() { [ "${*: -1}" = dunst ] && echo "$BIGWM_TEST_DUNST_PID"; }
	logger() { :; }
	big-wm-theme() { bash "$BIGWM_TEST_PWD/session/bin/big-wm-theme" "$@"; }
	# O instalado no sistema (quando há) reescreveria o Config.plist do teste.
	big-wm-wprefs-sync() { :; }
}
export BIGWM_TEST_PWD=$PWD
export -f gsettings dconf systemctl dbus-update-activation-environment dunstctl pgrep logger big-wm-theme big-wm-wprefs-sync
env -i DISPLAY=:bigwm-test sleep 600 </dev/null >/dev/null 2>&1 &
export BIGWM_TEST_DUNST_PID=$!
# shellcheck disable=SC2064 # $destdir e o PID valem já.
trap "kill $BIGWM_TEST_DUNST_PID 2>/dev/null; rm -rf '$destdir'" EXIT
theme_env=(DISPLAY=:bigwm-test HOME="$th_home" XDG_CONFIG_HOME="$th_home/.config" XDG_DATA_HOME="$th_home/.local/share" XDG_RUNTIME_DIR="$th/run"
	BIGWM_LIBDIR="$PWD/session/lib" BIGWM_DATADIR="$destdir/usr/share/big-wm"
	BIGWM_WMSTYLES="$destdir/usr/share/WindowMaker/Styles" BIGWM_KVANTUMDIR="$th/kvantum"
	BIGWM_THEMESDIR="$th/themes" BIGWM_ICONSDIR="$th/icons" DBUS_SESSION_BUS_ADDRESS=disabled: GSETTINGS_BACKEND=memory)
run_theme() { env "${theme_env[@]}" bash session/bin/big-wm-theme "$@" 2>>"$th/stderr"; }
wm_state() {
	python3 -I session/lib/big-wm-wmstyle state "$th_home/GNUstep/Defaults/WindowMaker" \
		themes/WindowMaker/Styles/BigWindowMaker.style themes/WindowMaker/Styles/BigWindowMaker-Dark.style
}
env_gtk_theme() {
	# shellcheck disable=SC2016
	env -i "${theme_env[@]}" bash -c '. session/lib/big-wm-env.sh; echo "$GTK_THEME"'
}
mkdir -p "$th_home/GNUstep/Defaults" "$th_home/.config/wlmaker"
cp data/GNUstep/Defaults/WindowMaker "$th_home/GNUstep/Defaults/WindowMaker"
cp data/wlmaker/Config.plist.in "$th_home/.config/wlmaker/Config.plist"
wlconf=$th_home/.config/wlmaker/Config.plist
kvconf=$th_home/.config/Kvantum/kvantum.kvconfig
# O estilo claro é exatamente a aparência padrão do Window Maker.
[ "$(wm_state)" = light ] || fail "big-wm-theme: BigWindowMaker.style difere do WindowMaker padrão"
[ "$(run_theme get)" = light ] || fail "big-wm-theme: o padrão deve ser light"
th_link=$th_home/.local/share/themes/BigWindowMaker
[ "$(env_gtk_theme)" = BigWindowMaker ] && [ "$(readlink "$th_link")" = "$th/themes/OneStepBack" ] ||
	fail "big-wm-env.sh: modo claro sem o BigWindowMaker -> OneStepBack: $(env_gtk_theme) $(readlink "$th_link")"
run_theme dark || fail "big-wm-theme dark saiu com erro"
[ "$(cat "$th_home/.config/big-wm/theme")" = dark ] && [ "$(run_theme get)" = dark ] ||
	fail "big-wm-theme: escolha dark não guardada"
[ "$(wm_state)" = dark ] || fail "big-wm-theme: Window Maker não passou ao BigWindowMaker-Dark"
python3 -I tests/plistcheck.py --wings "$th_home/GNUstep/Defaults/WindowMaker" ||
	fail "big-wm-theme: WindowMaker gravado inválido"
grep -q 'WorkspaceBack = (fpixmap, "/usr/share/backgrounds/big-wm/default.jpg"' "$th_home/GNUstep/Defaults/WindowMaker" ||
	fail "big-wm-theme: perdeu chaves que não são do estilo (WorkspaceBack)"
grep -qF 'ThemeFile = "/usr/share/big-wm/wlmaker/Theme-Dark.plist";' "$wlconf" ||
	fail "big-wm-theme: wlmaker sem o Theme-Dark.plist"
grep -qx 'theme=QTStepOpenStepDark' "$kvconf" || fail "big-wm-theme: Kvantum sem o QTStepOpenStepDark"
# Ícones iguais nas duas sessões: gsettings (Wayland) e settings.ini (X11).
[ "$(cat "$th/icon-theme")" = bigicons-papient-dark ] &&
	grep -qx 'gtk-icon-theme-name=bigicons-papient-dark' "$th_home/.config/gtk-3.0/settings.ini" &&
	grep -qx 'gtk-icon-theme-name=bigicons-papient-dark' "$th_home/.config/gtk-4.0/settings.ini" ||
	fail "big-wm-theme: ícones do modo escuro não foram para o gsettings e o settings.ini"
[ "$(cat "$th/color-scheme")" = prefer-dark ] || fail "big-wm-theme: color-scheme não ficou prefer-dark"
# Sessão aberta antes do nome estável (GTK_THEME=OneStepBack exportado no
# login): passa ao BigWindowMaker; o link já aponta para o escuro.
[ "$(cat "$th/gtk-theme")" = BigWindowMaker ] || fail "big-wm-theme: GTK_THEME da sessão não passou ao nome estável"
[ "$(readlink "$th_link")" = "$th/themes/OneStepBack-Dark" ] || fail "big-wm-theme: link do tema GTK sem o escuro"
# GTK 3 no Wayland: gtk-theme do gsettings (do padrão do esquema ao par)
[ "$(cat "$th/gs-gtk-theme" 2>/dev/null)" = OneStepBack-Dark ] || fail "big-wm-theme: gtk-theme do gsettings não acompanhou o modo"
grep -qF "dunstctl reload $destdir/usr/share/big-wm/dunstrc-dark" "$th/calls" ||
	fail "big-wm-theme: dunst não recarregou o tema escuro"
[ "$(env_gtk_theme)" = BigWindowMaker ] || fail "big-wm-env.sh: modo escuro sem o BigWindowMaker"
# shellcheck disable=SC2016
[ "$(env -i "${theme_env[@]}" GTK_THEME=OneStepBack-Dark GTK2_RC_FILES=/usr/share/themes/OneStepBack-Dark/gtk-2.0/gtkrc:/x bash -c '. session/lib/big-wm-env.sh; echo "$GTK_THEME ${GTK2_RC_FILES:-}"')" = "BigWindowMaker " ] ||
	fail "big-wm-env.sh: GTK_THEME/GTK2_RC_FILES herdados de uma sessão anterior não foram refeitos"
# shellcheck disable=SC2016
[ "$(env "${theme_env[@]}" GTK_THEME=OneStepBack bash -c '. session/lib/big-wm-theme.sh; big_wm_theme_refresh_env; echo "$GTK_THEME"')" = BigWindowMaker ] &&
	[ "$(env "${theme_env[@]}" GTK_THEME=Adwaita bash -c '. session/lib/big-wm-theme.sh; big_wm_theme_refresh_env; echo "$GTK_THEME"')" = Adwaita ] ||
	fail "big-wm-theme.sh: GTK_THEME dos aplicativos abertos depois da troca"
# Idempotente: de novo no mesmo modo, nada muda nem é regravado.
cp -p "$th_home/GNUstep/Defaults/WindowMaker" "$th/wm-dark"
cp -p "$wlconf" "$th/wl-dark"
: >"$th/calls"
run_theme dark
cmp -s "$th/wm-dark" "$th_home/GNUstep/Defaults/WindowMaker" &&
	[ ! "$th_home/GNUstep/Defaults/WindowMaker" -nt "$th/wm-dark" ] && cmp -s "$th/wl-dark" "$wlconf" &&
	! grep -q -e 'gsettings set' -e dbus-update "$th/calls" || fail "big-wm-theme dark duas vezes não é idempotente"
# Volta ao claro: tudo como era.
run_theme light || fail "big-wm-theme light saiu com erro"
[ "$(run_theme get)" = light ] && [ "$(wm_state)" = light ] || fail "big-wm-theme: Window Maker não voltou ao claro"
cmp -s data/wlmaker/Config.plist.in "$wlconf" || fail "big-wm-theme: Config.plist do wlmaker não voltou ao original"
grep -qx 'theme=QTStepOpenStep' "$kvconf" || fail "big-wm-theme: Kvantum não voltou ao QTStepOpenStep"
[ "$(cat "$th/color-scheme")" = default ] && [ "$(cat "$th/gtk-theme")" = BigWindowMaker ] &&
	[ "$(readlink "$th_link")" = "$th/themes/OneStepBack" ] ||
	fail "big-wm-theme: color-scheme, GTK_THEME ou o link do tema GTK não voltaram"
grep -qF "dunstctl reload $destdir/usr/share/big-wm/dunstrc" "$th/calls" || fail "big-wm-theme: dunst não voltou ao claro"
[ "$(cat "$th/icon-theme")" = bigicons-papient ] &&
	grep -qx 'gtk-icon-theme-name=bigicons-papient' "$th_home/.config/gtk-3.0/settings.ini" ||
	fail "big-wm-theme: ícones não voltaram ao bigicons-papient"
[ "$(env_gtk_theme)" = BigWindowMaker ] || fail "big-wm-env.sh: não voltou ao BigWindowMaker"
# Um diretório do usuário com o nome do tema fica: GTK_THEME com o par.
mv "$th_link" "$th/link-guardado" && mkdir "$th_link"
[ "$(env_gtk_theme)" = OneStepBack ] && [ -d "$th_link" ] && [ ! -L "$th_link" ] ||
	fail "big-wm-env.sh: trocou o diretório BigWindowMaker do usuário"
rmdir "$th_link" && mv "$th/link-guardado" "$th_link"
# Kvantum gravado pelo assistente do BigLinux ou pela sincronização com o
# tema do GNOME (BigAdwaitaRoundGtk*) ou sem tema: não é escolha do usuário.
printf '[General]\n    theme=BigAdwaitaRoundGtkDark\n' >"$kvconf"
run_theme dark
grep -qx 'theme=QTStepOpenStepDark' "$kvconf" || fail "big-wm-theme: BigAdwaitaRoundGtk não deu lugar ao QTStep"
printf '[General]\n[Applications]\nx=1\n' >"$kvconf"
run_theme light
[ "$(sed -n 2p "$kvconf")" = theme=QTStepOpenStep ] || fail "big-wm-theme: kvantum.kvconfig sem tema não recebeu o QTStep"
# Escolhas do usuário fora do par claro/escuro ficam como estão.
python3 -I session/lib/big-wm-wmstyle apply "$th_home/GNUstep/Defaults/WindowMaker" \
	themes/WindowMaker/Styles/BigLinux.style
cp -p "$th_home/GNUstep/Defaults/WindowMaker" "$th/wm-user"
printf '[General]\ntheme=KvFlat\n' >"$kvconf"
sed -i 's#ThemeFile = "[^"]*";#ThemeFile = "/home/usuario/MeuTema.plist";#' "$wlconf"
echo Adwaita >"$th/gtk-theme"
echo Arc >"$th/gs-gtk-theme"
echo prefer-light >"$th/color-scheme"
echo Papirus >"$th/icon-theme"
sed -i 's/^gtk-icon-theme-name=.*/gtk-icon-theme-name=Papirus/' "$th_home/.config/gtk-3.0/settings.ini"
run_theme dark
[ "$(cat "$th/icon-theme")" = Papirus ] && grep -qx 'gtk-icon-theme-name=Papirus' "$th_home/.config/gtk-3.0/settings.ini" ||
	fail "big-wm-theme trocou o tema de ícones do usuário"
# Adwaita gravado no dconf é escolha do usuário; o padrão do esquema, não.
rm -f "$th/icon-theme"
echo "'Adwaita'" >"$th/dconf-icon-theme"
run_theme light
[ ! -e "$th/icon-theme" ] || fail "big-wm-theme trocou o Adwaita escolhido pelo usuário"
rm -f "$th/dconf-icon-theme"
run_theme dark
cmp -s "$th/wm-user" "$th_home/GNUstep/Defaults/WindowMaker" || fail "big-wm-theme trocou um estilo do Window Maker do usuário"
grep -qx 'theme=KvFlat' "$kvconf" || fail "big-wm-theme trocou o tema do Kvantum do usuário"
grep -q 'ThemeFile = "/home/usuario/MeuTema.plist";' "$wlconf" || fail "big-wm-theme trocou o tema do wlmaker do usuário"
[ "$(cat "$th/gtk-theme")" = Adwaita ] || fail "big-wm-theme trocou o GTK_THEME do usuário"
[ "$(cat "$th/gs-gtk-theme")" = Arc ] || fail "big-wm-theme trocou o gtk-theme do gsettings escolhido pelo usuário"
[ "$(cat "$th/color-scheme")" = prefer-dark ] || fail "big-wm-theme dark (explícito) não gravou o color-scheme"
run_theme sépia
[ $? = 2 ] || fail "big-wm-theme: modo inválido deve sair com 2"
[ "$(cat "$th_home/.config/big-wm/theme")" = dark ] || fail "big-wm-theme: modo inválido mudou a escolha"
# Assistente da live: modo escuro escolhido antes do primeiro login. O
# big-wm-firstrun instala os padrões e o "apply" os passa ao escuro.
rm -rf "$th_home" && mkdir -p "$th_home"
echo default >"$th/color-scheme"
run_theme dark
[ ! -e "$th_home/GNUstep" ] || fail "big-wm-theme criou ~/GNUstep antes do primeiro login"
for session in x11 wayland; do
	env "${theme_env[@]}" LANG=pt_BR.UTF-8 BIGWM_LIBDIR="$destdir/usr/lib/big-wm" \
		bash -c "sed -e 's|^datadir=.*|datadir=$destdir/usr/share/big-wm|' \\
			-e 's|^kvantumdir=.*|kvantumdir=$th/kvantum|' -e 's|^ashyterm=.*|ashyterm=$th/sem-ashyterm|' \\
			-e 's|^wmicons=.*|wmicons=$destdir/usr/share/WindowMaker/Icons|' \\
			-e 's|^\\(\\x9\\)*setup_firefox &|true|' \\
			session/bin/big-wm-firstrun >'$th/firstrun' && bash '$th/firstrun' $session" ||
		fail "big-wm-firstrun $session com o modo escuro"
done
[ "$(wm_state)" = dark ] || fail "big-wm-firstrun: conta nova no modo escuro sem o estilo escuro do Window Maker"
grep -qF 'ThemeFile = "/usr/share/big-wm/wlmaker/Theme-Dark.plist";' "$wlconf" ||
	fail "big-wm-firstrun: wlmaker sem o tema escuro"
grep -qx 'theme=QTStepOpenStepDark' "$kvconf" || fail "big-wm-firstrun: Kvantum sem o QTStepOpenStepDark"
[ "$(cat "$th/color-scheme")" = prefer-dark ] || fail "big-wm-firstrun: color-scheme sem o modo escuro"
# Arquivos do modo escuro instalados e consistentes com os do claro.
for f in usr/share/big-wm/wlmaker/Theme-Dark.plist usr/share/big-wm/rofi/big-wm-dark.rasi \
	usr/share/big-wm/dunstrc-dark usr/share/WindowMaker/Styles/BigWindowMaker.style \
	usr/share/WindowMaker/Styles/BigWindowMaker-Dark.style usr/lib/big-wm/big-wm-theme.sh \
	usr/lib/big-wm/big-wm-wmstyle usr/bin/big-wm-theme; do
	[ -e "$destdir/$f" ] || fail "modo escuro: /$f não instalado"
done
cmp -s "$destdir/usr/share/big-wm/dunstrc" "$destdir/usr/share/big-wm/dunstrc-dark" && fail "dunstrc-dark igual ao claro"
python3 -I - themes/WindowMaker/Styles/BigWindowMaker.style themes/WindowMaker/Styles/BigWindowMaker-Dark.style <<'EOF' ||
import re, sys
keys = [set(re.findall(r'^  (\w+) =', open(f).read(), re.M)) for f in sys.argv[1:]]
sys.exit(keys[0] != keys[1] or "WorkspaceBack" in keys[0])
EOF
	fail "estilos claro e escuro do Window Maker com chaves diferentes (ou com WorkspaceBack)"
diff <(grep -o '^ *[A-Za-z]* =' data/wlmaker/Theme.plist) <(grep -o '^ *[A-Za-z]* =' data/wlmaker/Theme-Dark.plist) >/dev/null ||
	fail "Theme-Dark.plist com estrutura diferente do Theme.plist"
for m in data/GNUstep/Defaults/WMRootMenu data/GNUstep/Defaults/WMRootMenu.pt_BR data/wlmaker/RootMenu.plist data/wlmaker/RootMenu.plist.pt_BR; do
	grep -q '"big-wm-theme light")' "$m" && grep -q '"big-wm-theme dark")' "$m" || fail "$m: falta Tema claro/escuro"
done
unset -f gsettings systemctl dbus-update-activation-environment dunstctl pgrep logger big-wm-theme
kill "$BIGWM_TEST_DUNST_PID" 2>/dev/null
rm -rf "$th"
pass big-wm-theme

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
      },
      {
        Name = wmglobe.wmglobe;
        Command = "wmglobe";
        DropCommand = "wmglobe %d";
        AutoLaunch = No;
      },
      {
        Name = thunar.Thunar;
        Command = "thunar";
        AutoLaunch = No;
      }
    );
  };
}
PLIST
autolaunch() {
	BIG_WM_APPMENU=session/bin/big-wm-appmenu python3 -I session/lib/big-wm-dockapps-autolaunch \
		"$dock/WMState" "$dock/seen" 2>/dev/null
}
autolaunch
# wmglobe tem classe própria (não .DockApp) e vem do catálogo do menu; o
# Thunar acoplado é um aplicativo comum e continua sem AutoLaunch.
if [ "$(grep -c 'AutoLaunch = Yes;' "$dock/WMState")" = 3 ] && grep -q 'Logo.WMDock;' "$dock/WMState" &&
	grep -A2 'thunar.Thunar' "$dock/WMState" | grep -q 'AutoLaunch = No;' &&
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

# 6a1a. big-wm-dock-commands: comandos quebrados de ícones acoplados (a linha
# de comando de um script que mudou de nome, um script relativo) viram o
# gtk-launch do .desktop correspondente, uma vez; os que rodam, os dockapps
# e os sem .desktop ficam; o PasteCommand escolhido pelo usuário fica.
dock=$(mktemp -d)
mkdir -p "$dock/data/applications" "$dock/bin"
printf '[Desktop Entry]\nType=Application\nName=TTS\nExec=python3 /usr/share/tts/main.py\nStartupWMClass=biglinux-tts\n' \
	>"$dock/data/applications/br.com.biglinux.tts.desktop"
printf '[Desktop Entry]\nType=Application\nName=Telegram\nExec=Telegram\nStartupWMClass=TelegramDesktop\n' \
	>"$dock/data/applications/org.telegram.desktop.desktop"
printf '#!/bin/sh\n' >"$dock/bin/gtk-launch"
chmod +x "$dock/bin/gtk-launch"
cat >"$dock/WMState" <<'PLIST'
{
  Dock = {
    Applications = (
      {
        Name = "biglinux-tts.Biglinux-tts";
        Command = "biglinux-tts main.py";
        PasteCommand = "biglinux-tts main.py %s";
        DropCommand = "meu-comando %d";
        AutoLaunch = No;
      },
      {
        Name = "Telegram.TelegramDesktop";
        Command = "Telegram-que-nao-existe";
      },
      {
        Name = "sem\\.desktop.Sem";
        Command = "programa-que-nao-existe";
      },
      {
        Name = "thunar.Thunar";
        Command = "sh";
      },
      {
        Name = "wmfoo.DockApp";
        Command = "wmfoo-que-nao-existe";
      }
    );
  };
}
PLIST
dockcmd() {
	PATH="$dock/bin:/usr/bin:/bin" XDG_DATA_HOME="$dock/data" XDG_DATA_DIRS=/nonexistent \
		python3 -I session/lib/big-wm-dock-commands "$dock/WMState" "$dock/mark" 2>/dev/null
}
dockcmd
if grep -q 'Command = "gtk-launch br.com.biglinux.tts.desktop";' "$dock/WMState" &&
	grep -q 'PasteCommand = "gtk-launch br.com.biglinux.tts.desktop %s";' "$dock/WMState" &&
	grep -q 'DropCommand = "meu-comando %d";' "$dock/WMState" &&
	grep -q 'Command = "gtk-launch org.telegram.desktop.desktop";' "$dock/WMState" &&
	grep -q 'Command = "programa-que-nao-existe";' "$dock/WMState" &&
	grep -q 'Command = "sh";' "$dock/WMState" &&
	grep -q 'Command = "wmfoo-que-nao-existe";' "$dock/WMState" &&
	[ -f "$dock/WMState.bak-dock-commands" ] && [ -f "$dock/mark" ] &&
	python3 -I tests/plistcheck.py --wings "$dock/WMState"; then
	# Uma vez: um comando quebrado depois da marca fica.
	sed -i 's/Command = "sh";/Command = "biglinux-tts main.py";/' "$dock/WMState"
	dockcmd
	if grep -q 'Command = "biglinux-tts main.py";' "$dock/WMState"; then
		pass "comandos quebrados do Dock trocados pelo .desktop, uma vez"
	else
		fail "big-wm-dock-commands rodou de novo depois da marca"
	fi
else
	fail "big-wm-dock-commands: $(tr -d '\n' <"$dock/WMState")"
fi
rm -rf "$dock"

# 6a1b. big-wm-dockapps (Wayland): o padrão, mais os dockapps do catálogo que
#       o usuário deixou acoplados (Order/Dock do Dockapps.plist), menos os
#       tirados (Removed). Sem o XWayland (sem DISPLAY), os nativos; com ele,
#       os do Dock do X11 (os de GTK com o backend X11), e um nativo acoplado
#       antes não roda ao lado do dockapp do X11 de mesma função. Dockapps
#       falsos como funções exportadas: /tmp pode ser noexec.
wld=$(mktemp -d)
mkdir -p "$wld/cfg/wlmaker"
cat >"$wld/cfg/wlmaker/Dockapps.plist" <<'PLIST'
{
  Order = (wlmclock, wlmbattery, "big-wm-tray", wlmcpugraph, wmglobe);
  Dock = ("wmake-zones-wl", outro-programa);
  Removed = (wlmnetgraph, wmnet);
}
PLIST
run_dockapps() { # DISPLAY ou vazio
	rm -f "$wld/log"
	# shellcheck disable=SC2016 # o script entre aspas simples roda no bash -c
	env -u WAYLAND_DISPLAY DISPLAY="$1" XDG_CONFIG_HOME="$wld/cfg" LOG="$wld/log" BIG_WM_APPMENU=session/bin/big-wm-appmenu bash -c '
		[ -n "$DISPLAY" ] || unset DISPLAY
		for f in wlmcalclock wlmclock wlmmixer wlmcpugraph wlmmemgraph wlmnetgraph wlmbattery wlmeyes big-wm-tray wmake-zones-wl outro-programa \
			big-wm-clock wmclock big-wm-mixer wmix wmcpuload wmmemload wmnet wmcliphist wmshutdown wmglobe; do
			eval "$f() { echo \"$f\${GDK_BACKEND:+:\$GDK_BACKEND}\${1:+ \$*}\" >>\"\$LOG\"; }"
			export -f "$f"
		done
		exec bash session/bin/big-wm-dockapps'
	sort "$wld/log" 2>/dev/null | tr '\n' ','
}
started=$(run_dockapps "")
want="wlmbattery,wlmcalclock,wlmclock,wlmcpugraph,wlmeyes --dockapp,wlmmemgraph,wlmmixer,wmake-zones-wl,"
if [ "$started" = "$want" ]; then
	pass "big-wm-dockapps sem XWayland: os nativos; acoplados iniciam, tirados não"
else
	fail "big-wm-dockapps sem XWayland: iniciou '$started', esperado '$want'"
fi
started=$(run_dockapps ":99")
want="big-wm-clock:x11,big-wm-mixer:x11,wlmbattery,wlmeyes --dockapp,wmake-zones-wl,wmcliphist:x11,wmcpuload:x11,wmglobe:x11,wmmemload:x11,wmshutdown:x11,"
if [ "$started" = "$want" ]; then
	pass "big-wm-dockapps com XWayland: os do Dock do X11 (wmnet tirado), sem os nativos de mesma função"
else
	fail "big-wm-dockapps com XWayland: iniciou '$started', esperado '$want'"
fi
rm -rf "$wld"

# 6a1c. big-wm-clipboard: histórico da área de transferência (SQLite).
cbt=$(mktemp -d)
cb() { XDG_DATA_HOME="$cbt/data" XDG_CONFIG_HOME="$cbt/cfg" XDG_RUNTIME_DIR="$cbt/rt" \
	python3 -I session/bin/big-wm-clipboard "$@"; }
cbdump() { cb --dump 500 | tr '\0' '\n'; }
mkdir -p "$cbt/rt" "$cbt/cfg/big-wm"
for i in 1 2 3; do printf 'item %s ação' "$i" | cb --store; done
printf 'item 1 ação' | cb --store
printf 'senha' | CLIPBOARD_STATE=sensitive cb --store
printf '  \n ' | cb --store
printf 'a\0b' | cb --store
head -c 300000 /dev/zero | tr '\0' x | cb --store
printf 'favorito' | cb --store && printf 'favorito' | cb --favorite on
[ "$(cbdump | tr '\n' '|')" = "Ffavorito|Hitem 1 ação|Hitem 3 ação|Hitem 2 ação|" ] ||
	fail "big-wm-clipboard: gravação, repetido no topo, sensível/binário/vazio/grande fora: $(cbdump | tr '\n' '|')"
[ "$(stat -c %a "$cbt/data/big-wm/clipboard.db")" = 600 ] && [ "$(stat -c %a "$cbt/data/big-wm")" = 700 ] ||
	fail "big-wm-clipboard: banco e pasta só do usuário"
printf 'limit=10\n' >"$cbt/cfg/big-wm/clipboard.conf"
for i in $(seq 20); do printf 'n%s' "$i" | cb --store; done
[ "$(cbdump | wc -l)" = 11 ] || fail "big-wm-clipboard: limite de itens (favoritos fora da conta)"
cb --clear history
[ "$(cbdump)" = Ffavorito ] || fail "big-wm-clipboard: limpar mantendo os favoritos"
cb --clear all
[ -z "$(cbdump)" ] || fail "big-wm-clipboard: limpar tudo"
printf 'lixo' >"$cbt/data/big-wm/clipboard.db" && rm -f "$cbt/data/big-wm/clipboard.db-"*
printf 'depois' | cb --store
[ "$(cbdump)" = Hdepois ] && compgen -G "$cbt/data/big-wm/clipboard.db.corrompido-*" >/dev/null ||
	fail "big-wm-clipboard: banco corrompido guardado ao lado e um novo criado"
python3 -I -c 'import sqlite3, sys; d = sqlite3.connect(sys.argv[1]); d.execute("PRAGMA user_version=9"); d.commit()' \
	"$cbt/data/big-wm/clipboard.db"
printf 'x' | cb --store 2>/dev/null
[ "$(python3 -I -c 'import sqlite3, sys; print(sqlite3.connect(sys.argv[1]).execute("PRAGMA user_version").fetchone()[0])' \
	"$cbt/data/big-wm/clipboard.db")" = 9 ] || fail "big-wm-clipboard: banco de versão mais nova ficou intocado"
printf 'persist=false\n' >"$cbt/cfg/big-wm/clipboard.conf"
printf 'só na memória' | cb --store
[ -s "$cbt/rt/big-wm/clipboard.db" ] && ! cbdump | grep -q x ||
	fail "big-wm-clipboard: sem persistência o banco fica em XDG_RUNTIME_DIR"
printf 'capture=false\n' >"$cbt/cfg/big-wm/clipboard.conf"
printf 'não' | cb --store
cbdump | grep -q 'não' && fail "big-wm-clipboard: com a gravação desligada nada entra"
rm -rf "$cbt"
pass "big-wm-clipboard (histórico, favoritos, limites, privacidade, banco corrompido)"

# 6a2. big-wm-clip-add: no Clip da área 1, na linha abaixo do Thunar, uma vez.
clipt=$(mktemp -d)
# Ícone do dock com abas: o padrão já o traz; uma entrada sem ícone (a
# instalação de novembro, com o cubo padrão no Clip) ganha o ícone mesmo
# depois de "já adicionado", sem perder as outras chaves.
grep -A3 '"big-wm-tabbed-dock.Big-wm-tabbed-dock" = {' data/GNUstep/Defaults/WMWindowAttributes |
	grep -q 'Icon = "big-wm-tabbed-dock.png";' || fail "WMWindowAttributes: o dock com abas sem ícone (cubo no Clip)"
printf '{\n  "big-wm-tabbed-dock.Big-wm-tabbed-dock" = {\n    DontSaveSession = YES;\n  };\n}\n' >"$clipt/WMWindowAttributes"
echo big-wm-tabbed-dock.Big-wm-tabbed-dock >"$clipt/added"
: >"$clipt/WMState"
BIGWM_WMSTATE="$clipt/WMState" BIGWM_CLIP_ADDED="$clipt/added" python3 -I session/lib/big-wm-clip-add \
	big-wm-tabbed-dock.Big-wm-tabbed-dock big-wm-tabbed-dock data/GNUstep/Defaults/WMState
grep -q 'Icon = "data/GNUstep/Defaults/WMState"; AlwaysUserIcon = YES;' "$clipt/WMWindowAttributes" &&
	grep -q 'DontSaveSession = YES;' "$clipt/WMWindowAttributes" &&
	python3 -I tests/plistcheck.py --wings "$clipt/WMWindowAttributes" ||
	fail "big-wm-clip-add: ícone do dock com abas numa entrada sem ícone"
rm -f "$clipt/WMWindowAttributes" "$clipt/added" "$clipt/WMState"
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
# Marca da base fora do nome e o "Sistema e Distribuição" da BigCommunity fora do menu.
desktop sys tts 'Name=BigLinux Text to Speech\nName[pt_BR]=BigLinux Narrador de Texto\nExec=x\nCategories=Utility;'
desktop sys snap 'Name=Restauração de Snapshot BigLinux\nExec=x\nCategories=System;'
desktop sys org.bigcommunity.CommRelease 'Name=BigCommunity - Sistema e Distribuição\nExec=x\nCategories=System;'
# Um ID que termina em ".desktop" (o do Telegram) vai como nome de arquivo:
# "gtk-launch org.telegram.desktop" não acha o aplicativo.
desktop sys org.telegram.desktop 'Name=Telegram\nExec=x\nCategories=Network;'
# Já no menu raiz (Configurações > Preferências do Dock com Abas): não se repete
# em Aplicativos; o Buscar aplicativos continua mostrando.
desktop sys btdprefs 'Name=Preferências do Dock com Abas\nExec=x\nCategories=Settings;\nX-BigWM-RootMenu=true'
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
expect '("Narrador de Texto", EXEC, "gtk-launch tts")'
expect '("Restauração de Snapshot", EXEC, "gtk-launch snap")'
expect '("Telegram", EXEC, "gtk-launch org.telegram.desktop.desktop")'
reject 'BigLinux'
reject 'BigCommunity'
reject 'Sistema e Distribuição'
reject 'Preferências do Dock com Abas'
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
launcher Settings | grep -q '^btdprefs' || fail "big-wm-launcher: sem as Preferências do Dock com Abas"
# Busca: nome, descrição/palavras-chave e categoria, sem acento, em
# qualquer ordem; mais relevantes primeiro. Executar: sem shell, a não ser
# no modo shell explícito.
lsearch() { HOME=$apps XDG_CACHE_HOME=$apps/cache XDG_CONFIG_HOME=$apps/config LANG=pt_BR.UTF-8 \
	XDG_CURRENT_DESKTOP=WindowMaker XDG_DATA_HOME=$apps/home XDG_DATA_DIRS=$apps/sys \
	python3 -I session/bin/big-wm-launcher --search "$@" | cut -f1 | tr '\n' ' '; }
[[ "$(lsearch calc)" == "calc "* ]] || fail "big-wm-launcher --search calc: $(lsearch calc)"
[[ "$(lsearch CALCULADÓRA)" == "calc "* ]] || fail "big-wm-launcher --search sem acento/maiúsculas: $(lsearch CALCULADÓRA)"
[[ " $(lsearch acessorios) " == *" calc "* ]] || fail "big-wm-launcher --search pela categoria: $(lsearch acessorios)"
[ -z "$(lsearch calc zzzinexistente)" ] || fail "big-wm-launcher --search: todas as palavras devem aparecer"
lparse() { python3 -I session/bin/big-wm-launcher --parse "$1" "$2" | tr '\n' '|'; }
# shellcheck disable=SC2016 # $HOME literal de propósito: não pode ser expandido
lline='echo "a b" $HOME; rm x' lwant='echo|a b|$HOME;|rm|x|' lshell='echo $HOME' lwshell='/bin/sh|-c|echo $HOME|' 
[ "$(lparse direct "$lline")" = "$lwant" ] ||
	fail "big-wm-launcher --parse: interpretou como shell: $(lparse direct "$lline")"
[ "$(lparse direct 'echo "aberto')" = '<inválido>|' ] || fail "big-wm-launcher --parse: aspas abertas aceitas"
[ "$(lparse shell "$lshell")" = "$lwshell" ] || fail "big-wm-launcher --parse shell"
grep -q '"kb-cancel"' data/rofi/big-wm.rasi && grep -q 'button-close' data/rofi/big-wm.rasi ||
	fail "rofi: tema sem o botão de fechar"
# O rofi junta duas strings na mesma linha do tema: uma propriedade por linha.
grep -n '"[^"]*";[^"]*"' data/rofi/big-wm.rasi && fail "rofi: duas strings numa linha do tema"
LANG=pt_BR.UTF-8 python3 -I session/bin/big-wm-appmenu --dockapps >"$apps/dockapps.plist"
python3 -I tests/plistcheck.py --wings "$apps/dockapps.plist" || fail "big-wm-appmenu --dockapps: plist inválido"
# Menu Dockapps do Wayland: os dockapps do X11 conferidos no XWayland só com
# ele (DISPLAY), com o backend X11 do GTK e o wmnet com -w; os olhos são o
# wlmeyes --dockapp (o wmeyes no XWayland não vê o ponteiro).
env -u DISPLAY LANG=pt_BR.UTF-8 python3 -I session/bin/big-wm-appmenu --wlmaker --dockapps >"$apps/wl-dockapps.plist"
env DISPLAY=:99 LANG=pt_BR.UTF-8 python3 -I session/bin/big-wm-appmenu --wlmaker --dockapps >"$apps/xwl-dockapps.plist"
python3 -I tests/plistcheck.py "$apps/wl-dockapps.plist" && python3 -I tests/plistcheck.py "$apps/xwl-dockapps.plist" ||
	fail "big-wm-appmenu --wlmaker --dockapps: plist inválido"
grep -q 'GDK_BACKEND=x11' "$apps/wl-dockapps.plist" &&
	fail "big-wm-appmenu --wlmaker --dockapps: dockapp do X11 sem o XWayland"
for x in wmcpuload wmcliphist wmshutdown; do
	if command -v "$x" >/dev/null; then
		grep -qF "\"env GDK_BACKEND=x11 $x\")" "$apps/xwl-dockapps.plist" ||
			fail "big-wm-appmenu --wlmaker --dockapps: sem $x pelo XWayland"
	fi
done
if command -v wmnet >/dev/null; then
	grep -qF '"env GDK_BACKEND=x11 wmnet -w")' "$apps/xwl-dockapps.plist" ||
		fail "big-wm-appmenu --wlmaker --dockapps: wmnet sem -w"
fi
grep -q 'wmeyes' "$apps/xwl-dockapps.plist" && fail "big-wm-appmenu --wlmaker --dockapps: wmeyes do X11 no Wayland"
rm -rf "$apps"
pass big-wm-appmenu

# Painel de informações do Wayland (duplo clique no ladrilho principal do
# Dock, como o Info Panel do X11): a edição, os avisos de copyright e os
# créditos do Big WindowMaker Custom.
about=$(LANG=pt_BR.UTF-8 python3 -I session/bin/big-wm-about --text)
for want in "Big WindowMaker" "Baseado no BigLinux" "Window Maker Developers Team" "Philipp Kaeser" \
	"Copyleft 🄯 Rafael Ruscher" "Copyleft 🄯 Barnabé Di Kartola"; do
	grep -qF "$want" <<<"$about" || fail "big-wm-about: sem \"$want\""
done
grep -q 'InfoCommand = "/usr/bin/big-wm-about";' data/wlmaker/State.plist ||
	fail "State.plist: o duplo clique no ladrilho principal do Dock não abre o painel de informações"
grep -q '"big-wm-about")' data/wlmaker/RootMenu.plist.pt_BR || fail "menu Wayland sem o item Sobre"

# 6b0. big-wm-welcome: sessão, atalhos reais, preferências, primeiro login,
#      páginas e todas as cenas da ilustração (tests/welcome).
if python3 -I -m unittest discover -s tests/welcome -t tests/welcome >"${TMPDIR:-/tmp}/welcome-tests.log" 2>&1; then
	pass "big-wm-welcome ($(grep -o 'Ran [0-9]* tests' "${TMPDIR:-/tmp}/welcome-tests.log"))"
else
	cat "${TMPDIR:-/tmp}/welcome-tests.log" >&2
	fail "big-wm-welcome (tests/welcome)"
fi
if command -v msgfmt >/dev/null; then
	msgfmt -c --check-format -o /dev/null welcome/po/pt_BR.po 2>/dev/null ||
		fail "big-wm-welcome: tradução pt_BR inválida"
fi

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
	# A ferramenta própria: com o Spectacle instalado, o padrão seria ele.
	env HOME="$shots" LANG=pt_BR.UTF-8 XDG_SESSION_TYPE="$1" BIG_WM_SCREENSHOT_TOOL=builtin bash -c '
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

# 6b3. big-wm-tray: segue a borda e os lançadores do Dock do wlmaker e
# repete o ladrilho do wmsystemtray (paginação, áreas de clique, balão).
# O script carrega sem GTK (o gi só é importado ao iniciar a bandeja).
tray=$(mktemp -d)
traypy() { python3 -I - "$@" <<'PY'
import sys
from importlib.machinery import SourceFileLoader
from importlib.util import module_from_spec, spec_from_loader
sys.dont_write_bytecode = True
loader = SourceFileLoader("big_wm_tray", "session/bin/big-wm-tray")
t = module_from_spec(spec_from_loader("big_wm_tray", loader))
loader.exec_module(t)
exec(sys.argv[1], {"t": t, "argv": sys.argv[2:]})
PY
}
traylayout() { traypy 'print(*t.dock_layout(argv[0]))' "$1"; }
[ "$(traylayout data/wlmaker/State.plist)" = "RIGHT TOP 2" ] || fail "big-wm-tray: Dock padrão lido errado: $(traylayout data/wlmaker/State.plist)"
sed -e 's/Edge = RIGHT;/Edge = LEFT;/' -e 's/Anchor = TOP;/Anchor = BOTTOM;/' data/wlmaker/State.plist >"$tray/s.plist"
[ "$(traylayout "$tray/s.plist")" = "LEFT BOTTOM 2" ] || fail "big-wm-tray: Dock arrastado lido errado: $(traylayout "$tray/s.plist")"
[ "$(traylayout "$tray/nao-existe")" = "RIGHT TOP 2" ] || fail "big-wm-tray: sem State.plist deve usar o padrão"
# Dockapps acoplados ao Dock (DockappTiles, gravado pelo wlmaker) contam
# como ladrilhos antes da bandeja; o nível do Dock (Lowered, AutoRaiseLower).
sed -e 's/^\(  Dock = {\)$/\1\n    DockappTiles = "2";\n    Lowered = Yes;/' data/wlmaker/State.plist >"$tray/d.plist"
[ "$(traylayout "$tray/d.plist")" = "RIGHT TOP 4" ] || fail "big-wm-tray: DockappTiles não somado: $(traylayout "$tray/d.plist")"
[ "$(traypy 'print(t.dock_level(argv[0]), t.dock_level(argv[1]))' "$tray/d.plist" data/wlmaker/State.plist)" = "normal top" ] ||
	fail "big-wm-tray: nível do Dock (Lowered)"
sed -i 's/Lowered = Yes;/AutoRaiseLower = YES;/' "$tray/d.plist"
[ "$(traypy 'print(t.dock_level(argv[0]))' "$tray/d.plist")" = auto ] || fail "big-wm-tray: nível do Dock (AutoRaiseLower)"
pass "big-wm-tray junto dos lançadores do Dock"

# A bandeja como dockapp adotado pelo wlmaker: a região de entrada (só os
# ícones e as setas: o resto do ladrilho arrasta e abre o menu do wlmaker),
# onde o wlmaker a pôs (Dockapps.plist), "Removed" (o autostart não a
# inicia) e "Ícones pequenos" das Configurações... do wlmaker. Uma gaveta
# no Dock conta como um lançador (os lançadores de dentro dela, não).
cat >"$tray/Dockapps.plist" <<'EOF2'
{
  Dock = (wlmcpugraph, "big-wm-tray");
  Order = (wlmcalclock);
  Removed = ("big-wm-tray", wlmclock);
}
EOF2
printf '{\n  SmallIcons = Yes;\n}\n' >"$tray/big-wm-tray.plist"
sed -e 's/^\(    Launchers = ($\)/\1\n      {Drawer = Yes; Id = 1; CommandLine = ""; Icon = "big-wm-drawer-56x56.png";\n        Launchers = ({CommandLine = a; Icon = b;}, {CommandLine = c; Icon = d;});},/' \
	data/wlmaker/State.plist >"$tray/g.plist"
traypy '
assert t.input_rects(2, 0, False) == [(8, 4, 24, 24), (32, 4, 24, 24), (4, 52, 15, 10), (45, 52, 15, 10)]
assert t.input_rects(0, 0, True) == [(4, 52, 15, 10), (45, 52, 15, 10)]
text = t.read_plist(argv[0])
assert t.dockapp_slot(text) == ("Dock", 1), t.dockapp_slot(text)
assert t.dockapp_slot("{ Order = (\"big-wm-tray\"); }") == ("Clip", 0)
assert t.dockapp_slot("{ Drawer.3 = (x, \"big-wm-tray\"); }") == ("Drawer.3", 1)
assert t.dockapp_slot(None) == ("Dock", 0)
assert t.removed(argv[0]) and not t.removed(argv[1] + "-nao-existe")
assert t.dock_layout(argv[2])[2] == 3, t.dock_layout(argv[2])
' "$tray/Dockapps.plist" "$tray/x" "$tray/g.plist" || fail "big-wm-tray: dockapp adotado (entrada, lugar, Removed, gaveta)"
mkdir -p "$tray/cfg/wlmaker" && cp "$tray/big-wm-tray.plist" "$tray/cfg/wlmaker/"
[ "$(XDG_CONFIG_HOME="$tray/cfg" traypy 'print(t.read_prefs()["small"])')" = True ] ||
	fail "big-wm-tray: Ícones pequenos das Configurações... do wlmaker"
# Ícones da bandeja: um símbolo rasterizado (preto sobre transparente) é
# recolorido pela máscara alfa na cor do modo; um ícone colorido ou cheio
# fica como veio (no automático); o estilo vem das preferências. Precisa do
# PyGObject com os typelibs da bandeja; sem eles, a verificação é pulada.
if ! python3 -I -c 'import gi
for ns, v in (("Gtk", "3.0"), ("GtkLayerShell", "0.1"), ("DbusmenuGtk3", "0.4")):
    gi.require_version(ns, v)' 2>/dev/null; then
	echo "aviso: PyGObject, gtk-layer-shell ou libdbusmenu-gtk3 ausente, cores da bandeja não verificadas" >&2
elif ! traypy '
t.load_gi()
from gi.repository import GdkPixbuf, GLib, Gdk
def pb(w, h, fn):
    data = bytearray()
    for y in range(h):
        for x in range(w):
            data += bytes(fn(x, y))
    return GdkPixbuf.Pixbuf.new_from_bytes(GLib.Bytes.new(bytes(data)), GdkPixbuf.Colorspace.RGB, True, 8, w, h, w * 4)
glyph = pb(16, 16, lambda x, y: (10, 10, 10, 255) if 4 <= x < 12 and 6 <= y < 10 else (0, 0, 0, 0))
color = pb(16, 16, lambda x, y: (220, 40, 40, 255) if 2 <= x < 14 and 2 <= y < 14 else (0, 0, 0, 0))
full = pb(16, 16, lambda x, y: (90, 90, 90, 255))
assert t.glyph_like(glyph), "símbolo rasterizado"
assert not t.glyph_like(color), "ícone colorido"
assert not t.glyph_like(full), "ícone cheio"
fg = Gdk.RGBA(); fg.parse("#e6e6e6")
out = t.recolor(glyph, fg)
px = out.get_pixels()
assert px[(8 * 16 + 6) * 4:(8 * 16 + 6) * 4 + 4] == bytes([230, 230, 230, 255]), "cor do modo escuro"
assert px[3] == 0, "transparência preservada"
'; then
	fail "big-wm-tray: normalização dos ícones (símbolo rasterizado, colorido, cheio)"
fi
mkdir -p "$tray/cfg2/big-wm"
printf 'icons=monochrome\nsmall=false\n' >"$tray/cfg2/big-wm/tray.conf"
[ "$(XDG_CONFIG_HOME="$tray/cfg2" traypy 'print(t.read_prefs(argv[0])["icons"])' "$tray/cfg2/big-wm/tray.conf")" = monochrome ] ||
	fail "big-wm-tray: estilo dos ícones nas preferências"
grep -q '^Exec=.*big-wm-tray --autostart$' session/autostart-wayland/big-wm-wayland-tray.desktop ||
	fail "big-wm-tray: o autostart deve passar --autostart (respeita Removed)"
sed -n '/^  Dock = {$/,/^  };$/p' data/wlmaker/State.plist | tr -d ' \n' |
	grep -qF 'Dockapps=(wmclock,"big-wm-tray",wmix,wmcpuload,wmmemload,wmnet,"wmake-zones-wl",wmcliphist,wmshutdown,wlmeyes);' ||
	fail "State.plist: a bandeja e o ladrilho do wmake-zones vão para o Dock"
grep -q 'Name = "wmake-zones.DockApp";' data/GNUstep/Defaults/WMState ||
	fail "WMState: o ladrilho do wmake-zones no Dock do X11"

# Dockapps padrão (wmcliphist, wmshutdown, wmeyes): entram no Dock uma vez,
# na primeira posição livre, com o WM_CLASS que o Window Maker casa com o
# programa; sem cópia de quem já está no Clip; quem o usuário tirou não
# volta; Dock cheio (a coluna passa da tela) fica de fora, sem marca.
dk=$(mktemp -d)
mkdir -p "$dk/bin" "$dk/d" "$dk/cfg"
for c in wmcliphist wmshutdown wmeyes wmake-zones; do printf '#!/bin/sh\n' >"$dk/bin/$c"; done
# shellcheck disable=SC2016 # o $DK_HEIGHT é do xwininfo falso, não daqui
printf '#!/bin/sh\necho "  Height: ${DK_HEIGHT:-1080}"\n' >"$dk/bin/xwininfo"
chmod +x "$dk"/bin/*
dock_run() {
	# shellcheck disable=SC2016
	PATH=$dk/bin:$PATH config_home=$dk/cfg bash -c '
		log() { :; }
		eval "$(sed -n "/^dock_add_once()/,/^}/p; /^migrate_wmake_zones_dock()/,/^}/p; /^migrate_default_dockapps()/,/^}/p" session/bin/big-wm-firstrun)"
		migrate_wmake_zones_dock "$1"; migrate_default_dockapps "$1"' _ "$dk/d"
}
cp data/GNUstep/Defaults/WMState "$dk/d/WMState"
dock_run
dock_run
for n in wmcliphist.Wmcliphist wmshutdown.Wmshutdown wmeyes.WMeyes; do
	[ "$(grep -c "Name = \"$n\";" "$dk/d/WMState")" = 1 ] || fail "Dock padrão: $n deve entrar uma vez"
done
grep -A1 'Name = "wmeyes.WMeyes";' "$dk/d/WMState" | grep -q 'Command = "wmeyes -w";' ||
	fail "Dock padrão: wmeyes no modo dockapp (-w)"
[ "$(awk '/^  Dock = \{$/ { d = 1 } /^  \};$/ { d = 0 } d' "$dk/d/WMState" | grep -o 'Position = "0,[0-9]*";' | sort | uniq -d)" = "" ] ||
	fail "Dock padrão: duas peças na mesma posição"
python3 tests/plistcheck.py --wings "$dk/d/WMState" >/dev/null 2>&1 || fail "Dock padrão: WMState inválido"
# O usuário tira o wmeyes: não volta.
python3 -I - "$dk/d/WMState" <<-'EOP'
	import re, sys
	p = sys.argv[1]; t = open(p).read()
	t = re.sub(r',\n      \{\n        Name = "wmeyes\.WMeyes";.*?\n      \}', '', t, flags=re.S)
	open(p, "w").write(t)
EOP
dock_run
grep -q 'wmeyes' "$dk/d/WMState" && fail "Dock padrão: o wmeyes tirado pelo usuário voltou"
# Conta antiga com o wmcliphist no Clip: o Dock não ganha outro.
rm -f "$dk"/cfg/big-wm/dock-*
cp data/GNUstep/Defaults/WMState "$dk/d/WMState"
sed -i 's|            Name = thunar.Thunar;|            Name = wmcliphist.Wmcliphist;|' "$dk/d/WMState"
dock_run
[ "$(grep -c 'wmcliphist.Wmcliphist' "$dk/d/WMState")" = 1 ] || fail "Dock padrão: wmcliphist duplicado (já estava no Clip)"
# Tela de 600 linhas: a coluna já vai até a posição 7; cabe só a 8.
rm -f "$dk"/cfg/big-wm/dock-*
cp data/GNUstep/Defaults/WMState "$dk/d/WMState"
DK_HEIGHT=600 dock_run
grep -q 'wmcliphist.Wmcliphist' "$dk/d/WMState" && ! grep -q 'wmshutdown\|wmeyes' "$dk/d/WMState" &&
	[ ! -e "$dk/cfg/big-wm/dock-wmshutdown" ] ||
	fail "Dock padrão: numa tela de 600 linhas só o primeiro cabe (e os outros ficam sem marca)"
rm -rf "$dk"
python3 tests/plistcheck.py --wings data/GNUstep/Defaults/WMState >/dev/null 2>&1 ||
	fail "WMState: proplist válido (um nome com hífen sem aspas quebra o Dock inteiro)"
pass "big-wm-tray: dockapp adotado pelo wlmaker (arrastar, Removed, Configurações...)"

traypy '
# Paginação do wmsystemtray: 4 ícones de 24 (ou 9 de 16) por página, "1/1"
# mesmo sem ícones, número em x=24 (19 a partir da página 10).
assert [t.page_count(n, 4) for n in (0, 1, 4, 5, 8, 9)] == [1, 1, 1, 2, 2, 3]
assert t.page_count(10, 9) == 2
assert t.page_slots(5, 0, False) == [(0, 8, 4), (1, 32, 4), (2, 8, 28), (3, 32, 28)]
assert t.page_slots(5, 1, False) == [(4, 8, 4)]
assert [s[1:] for s in t.page_slots(9, 0, True)][-1] == (40, 36)
assert t.page_label(0, 1) == ("1/1", 24) and t.page_label(9, 12) == ("10/12", 19)
# Áreas de clique: setas (ButtonPress do wmsystemtray), barra, ícones, borda.
assert t.hit_test(10, 56, 5, 0, False) == ("left",) and t.hit_test(50, 56, 5, 0, False) == ("right",)
assert t.hit_test(30, 56, 5, 0, False) == ("bar",) and t.hit_test(1, 30, 5, 0, False) == ("frame",)
assert t.hit_test(9, 5, 5, 1, False) == ("icon", 4) and t.hit_test(40, 40, 5, 1, False) == ("frame",)
# Setas e fonte iguais às do wmsystemtray: XPM 30x30, glifos 5x8.
assert len(t.ARROWS_XPM) == 30 and {len(r) for r in t.ARROWS_XPM} == {30}
assert set("".join(t.ARROWS_XPM)) <= set(t.ARROWS_XPM_COLORS)
assert all(len(g.split("|")) == 8 and {len(r) for r in g.split("|")} == {5} for g in t.FONT_5X8.values())
# Ladrilho no lugar certo e balão do Window Maker para dentro da tela.
# Um ladrilho do Dock a cada 64 px mais a margem do tema (1 px): depois do
# quadrado principal e de dois lançadores, a bandeja começa em 3 × 65.
assert t.tile_origin("RIGHT", "TOP", 2, 1280, 800) == (1216, 195)
assert t.tile_origin("BOTTOM", "LEFT", 2, 1280, 800) == (195, 736)
assert t.tile_origin("RIGHT", "TOP", 3, 1280, 800, 0) == (1216, 256)
assert t.tile_style("data/wlmaker/Theme.plist")["margin"] == 1
w, h, right, below, bx, by = t.balloon_geometry(100, 2, 15, (1216, 192, 64, 64), (1280, 800))
assert right and not below and bx + w == 1216 + 32 and by + h + t.BALLOON_SPACE == 192
w, h, right, below, bx, by = t.balloon_geometry(50, 1, 15, (0, 0, 64, 64), (1280, 800))
assert not right and below and bx == 32 and by == 63
# Texto do balão: ToolTip, senão Title, senão Id.
assert t.item_label({"ToolTip": ("i", [], "Pamac", "<b>4</b> atualizações"), "Title": "x"}) == "Pamac\n4 atualizações"
assert t.item_label({"Title": "Rede", "Id": "nm-applet"}) == "Rede" and t.item_label({"Id": "a"}) == "a"
# Fundo dos ladrilhos do Dock nos dois modos (Theme.plist e Theme-Dark.plist).
light = t.tile_style("data/wlmaker/Theme.plist")
dark = t.tile_style("data/wlmaker/Theme-Dark.plist", "dark")
assert (light["type"], light["from"], light["to"], light["bezel"]) == ("DGRADIENT", (0xa6, 0xa6, 0xb6, 255), (0x51, 0x55, 0x61, 255), 2)
assert (dark["from"], dark["to"]) == ((0x5c, 0x5c, 0x64, 255), (0x1c, 0x1c, 0x22, 255))
assert t.PALETTE["dark"]["from"] == dark["from"] and t.PALETTE["light"]["from"] == light["from"]
assert t.tile_style(None, "dark")["from"] == dark["from"]
' || fail "big-wm-tray: paginação, áreas de clique ou aparência do ladrilho"

# Modo claro/escuro e o tema do Dock: o par do Big WindowMaker segue o modo;
# um ThemeFile do usuário vale nos dois.
mkdir -p "$tray/cfg/big-wm" "$tray/cfg/wlmaker" "$tray/data/wlmaker"
cp data/wlmaker/Theme.plist data/wlmaker/Theme-Dark.plist "$tray/data/wlmaker/"
echo "{ ThemeFile = \"$tray/data/wlmaker/Theme.plist\"; }" >"$tray/cfg/wlmaker/Config.plist"
trayenv() { XDG_CONFIG_HOME=$tray/cfg BIGWM_DATADIR=$tray/data traypy "$@"; }
[ "$(trayenv 'print(t.theme_mode())')" = light ] || fail "big-wm-tray: sem ~/.config/big-wm/theme deve ser claro"
echo dark >"$tray/cfg/big-wm/theme"
[ "$(trayenv 'print(t.theme_mode(), t.theme_path(t.theme_mode()))')" = "dark $tray/data/wlmaker/Theme-Dark.plist" ] ||
	fail "big-wm-tray: modo escuro deve usar o Theme-Dark.plist"
cp data/wlmaker/Theme.plist "$tray/meu.plist"
echo "{ ThemeFile = \"$tray/meu.plist\"; }" >"$tray/cfg/wlmaker/Config.plist"
[ "$(trayenv 'print(t.theme_path("dark"))')" = "$tray/meu.plist" ] || fail "big-wm-tray: ThemeFile do usuário deve valer"
# Theme.plist gerado pelo big-wm-wprefs-sync (aparência do WPrefs sobre o
# tema do modo): é o que o Dock usa, regerado a cada troca de modo.
sed '1i // Tema do wlmaker. Gerado pelo big-wm-wprefs-sync a partir do tema padrão' data/wlmaker/Theme-Dark.plist >"$tray/cfg/wlmaker/Theme.plist"
echo "{ ThemeFile = \"$tray/cfg/wlmaker/Theme.plist\"; }" >"$tray/cfg/wlmaker/Config.plist"
[ "$(trayenv 'p = t.theme_path("dark"); print(p, "#%02x%02x%02x" % t.tile_style(p, "dark")["from"][:3])')" = "$tray/cfg/wlmaker/Theme.plist #5c5c64" ] ||
	fail "big-wm-tray: tema gerado pelo big-wm-wprefs-sync"
printf 'small=true\nfgcolor=#ffffff\n' >"$tray/cfg/big-wm/tray"
[ "$(trayenv 'p = t.read_prefs(); print(p["small"], p["fgcolor"])')" = "True #ffffff" ] || fail "big-wm-tray: preferências"
rm -rf "$tray"
pass "big-wm-tray: ladrilho do wmsystemtray (paginação, setas, balão, modo escuro)"

# Bandeja do Dock no X11: número da página claro no modo escuro, a cor do
# usuário (-c) sempre vale.
st=$(mktemp -d)
mkdir -p "$st/cfg/big-wm"
stray() { BIGWM_DRY_RUN=1 XDG_CONFIG_HOME=$st/cfg BIGWM_LIBDIR=session/lib bash session/bin/big-wm-systemtray "$@"; }
[ "$(stray)" = wmsystemtray ] || fail "big-wm-systemtray: modo claro deve usar a cor padrão"
echo dark >"$st/cfg/big-wm/theme"
[ "$(stray -v)" = "wmsystemtray -c #e6e6e6 -v" ] || fail "big-wm-systemtray: modo escuro sem o texto claro: $(stray -v)"
[ "$(stray -c red)" = "wmsystemtray -c red" ] || fail "big-wm-systemtray: cor do usuário deve valer"
[ "$(stray --command dark)" = "wmsystemtray -c #e6e6e6" ] && [ "$(stray --command light)" = wmsystemtray ] ||
	fail "big-wm-systemtray --command"
rm -rf "$st"
pass "big-wm-systemtray (bandeja do X11 legível no modo escuro)"

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
cond WindowMaker qt-theme-sync && fail "autostart: sync-gnome-theme-to-qt trocaria o QTStep"
cond wlmaker qt-theme-sync wayland && fail "autostart: sync-gnome-theme-to-qt trocaria o QTStep (Wayland)"
# Antes da sessão (default.target): vale o Desktop da sessão do logind.
# shellcheck disable=SC2016 # expandido pelo bash -c
logind_cond() { env -i PATH="$PATH" FAKE_DESKTOP="$1" bash -c \
	'loginctl() { case $1 in show-user) echo 7 ;; show-session) echo "$FAKE_DESKTOP" ;; esac; }
	. session/lib/big-wm-autostart-condition qt-theme-sync'; }
logind_cond big-wm && fail "autostart: sync-gnome-theme-to-qt no login do big-wm"
logind_cond big-wm-wayland && fail "autostart: sync-gnome-theme-to-qt no login do big-wm-wayland"
logind_cond plasma || fail "autostart: sync-gnome-theme-to-qt deve rodar no Plasma"
cond GNOME qt-theme-sync || fail "autostart: sync-gnome-theme-to-qt deve rodar no GNOME"
# pamac: só o pamac-tray.service (com o atraso), nenhum autostart a mais.
cond WindowMaker pamac-servico && fail "autostart: pamac-tray em dobro no X11"
cond wlmaker pamac-servico wayland && fail "autostart: pamac-tray em dobro no Wayland"
cond XFCE pamac-servico || fail "autostart: pamac-tray deve subir no XFCE"
grep -q '^ExecStart=/usr/lib/big-wm/big-wm-pamac-tray$' session/systemd/user-dropins/pamac-tray.service &&
	grep -q '^Type=exec$' session/systemd/user-dropins/pamac-tray.service ||
	fail "pamac-tray.service: drop-in sem o atraso fora da fila de jobs"
grep -q 'WindowMaker:\* | \*:wlmaker:\*) sleep' session/lib/big-wm-pamac-tray ||
	fail "big-wm-pamac-tray: o atraso só vale nas sessões Big WindowMaker"
for f in session/systemd/autostart-conditions/*.service; do
	arg=$(sed -n 's|^ExecCondition=/usr/lib/big-wm/big-wm-autostart-condition ||p' "$f")
	grep -q "^${arg})" session/lib/big-wm-autostart-condition || fail "$f: condição desconhecida '$arg'"
done
pass "condições de autostart"

# 6b. Firefox: o tema Netscape fica em chrome/netscape4 e só vale com o tema
#     padrão do Firefox (outro tema de about:addons vale sozinho). Perfil
#     novo ganha o tema; a cópia antiga, sem alteração, passa para a nova;
#     um chrome/ apagado pelo usuário não volta; o chrome/ do usuário fica.
fx=$(mktemp -d)
mkdir -p "$fx/theme/image" "$fx/home/.mozilla/firefox"
printf '@import "colors.css";\n' >"$fx/theme/userChrome.css"
printf 'slider {}\n' >"$fx/theme/userContent.css"
printf ':root {}\n' >"$fx/theme/colors.css"
printf 'png' >"$fx/theme/image/a.png"
printf '#!/bin/sh\n' >"$fx/firefox" && chmod +x "$fx/firefox"
printf '[Profile0]\nPath=novo\n[Profile1]\nPath=antigo\n[Profile2]\nPath=apagado\n[Profile3]\nPath=%s\n' \
	"$fx/home/.mozilla/firefox/meu" >"$fx/home/.mozilla/firefox/profiles.ini"
for p in novo antigo apagado meu; do mkdir -p "$fx/home/.mozilla/firefox/$p"; done
cp -r "$fx/theme" "$fx/home/.mozilla/firefox/antigo/chrome"
(cd "$fx/home/.mozilla/firefox/antigo/chrome" && find . -type f ! -name .big-wm-theme -print0 | LC_ALL=C sort -z | xargs -0 sha256sum) |
	sha256sum | cut -d' ' -f1 >"$fx/home/.mozilla/firefox/antigo/chrome/.big-wm-theme"
: >"$fx/home/.mozilla/firefox/apagado/.big-wm-firefox"
mkdir -p "$fx/home/.mozilla/firefox/meu/chrome" && printf '/* meu */\n' >"$fx/home/.mozilla/firefox/meu/chrome/userChrome.css"
sed -n '/^firefox_theme=/,/^# Ícone fixo de um atalho do Clip/p' session/bin/big-wm-firstrun |
	sed "s#/usr/bin/firefox#$fx/firefox#g" >"$fx/lib.sh"
(
	# shellcheck disable=SC2329,SC2034 # usados pelo setup_firefox carregado abaixo
	log() { :; }
	# shellcheck disable=SC2329
	pgrep() { return 1; }
	# shellcheck disable=SC2034
	config_home=$fx/home/.config HOME=$fx/home BIGWM_FIREFOX_THEME=$fx/theme
	# shellcheck source=/dev/null
	. "$fx/lib.sh"
	setup_firefox
	setup_firefox
)
fxp=$fx/home/.mozilla/firefox
for p in novo antigo; do
	cond='(-moz-pref("extensions.activeThemeID", "default-theme@mozilla.org"));'
	grep -qF "@import url(\"netscape4/userChrome.css\") $cond" "$fxp/$p/chrome/userChrome.css" &&
		grep -qF "@import url(\"netscape4/userContent.css\") $cond" "$fxp/$p/chrome/userContent.css" &&
		cmp -s "$fx/theme/colors.css" "$fxp/$p/chrome/netscape4/colors.css" &&
		[ -e "$fxp/$p/chrome/netscape4/image/a.png" ] && [ ! -e "$fxp/$p/chrome/colors.css" ] &&
		[ -e "$fxp/$p/.big-wm-firefox" ] || fail "Firefox ($p): tema condicionado ao tema padrão"
done
[ "$(grep -c 'legacyUserProfileCustomizations.stylesheets", true' "$fxp/novo/prefs.js")" = 1 ] ||
	fail "Firefox: preferências do tema no perfil novo"
[ -e "$fxp/antigo/prefs.js" ] && fail "Firefox: perfil com tema antigo não precisa de preferências novas"
[ -e "$fxp/apagado/chrome" ] && fail "Firefox: chrome/ apagado pelo usuário voltou"
[ "$(cat "$fxp/meu/chrome/userChrome.css")" = "/* meu */" ] && [ ! -e "$fxp/meu/chrome/netscape4" ] &&
	[ ! -e "$fxp/meu/.big-wm-firefox" ] || fail "Firefox: chrome/ do usuário foi alterado"
rm -rf "$fx"
pass "Firefox: tema Netscape só com o tema padrão; perfis do usuário respeitados"

# 6c. WebApps do /etc/skel que chamam o Brave: sem o Brave, viram WebApps do
#     Firefox no formato do próprio WebApps; um editado pelo usuário fica.
wa=$(mktemp -d)
mkdir -p "$wa/skel" "$wa/home/.local/share/applications" "$wa/home/.local/share/biglinux-webapps"
for site in web.whatsapp.com__ discord.com__; do
	cat >"$wa/skel/brave-$site-Default.desktop" <<-EOF
		[Desktop Entry]
		Version=1.0
		Terminal=false
		Type=Application
		Name=Site
		Exec=big-webapps-exec filename="brave-$site-Default.desktop" brave --class="${site%__}" --profile-directory=Browser --app="https://${site%__}"
		Icon=webapp-site
		StartupWMClass=${site%__}
		Categories=Webapps;
		StartupNotify=false
		Actions=SoftwareRender;

		[Desktop Action SoftwareRender]
		Name=Software Render
		Exec=SoftwareRender big-webapps-exec filename="brave-$site-Default.desktop" brave --class="${site%__}" --profile-directory=Browser --app="https://${site%__}"
	EOF
	cp "$wa/skel/brave-$site-Default.desktop" "$wa/home/.local/share/applications/"
done
sed -i 's/^Name=Site$/Name=Meu Discord/' "$wa/home/.local/share/applications/brave-discord.com__-Default.desktop"
printf '[{"browser": "brave", "app_file": "brave-web.whatsapp.com__-Default.desktop", "app_profile": "Default"}]\n' \
	>"$wa/home/.local/share/biglinux-webapps/webapps.json"
printf '#!/bin/sh\n' >"$wa/firefox" && chmod +x "$wa/firefox"
sed -n '/^webapps_skel=/,/^# Ícone fixo de um atalho do Clip/p' session/bin/big-wm-firstrun >"$wa/lib.sh"
(
	# shellcheck disable=SC2329
	log() { :; }
	# shellcheck disable=SC2034
	data_home=$wa/home/.local/share BIGWM_WEBAPPS_SKEL=$wa/skel BIGWM_FIREFOX_BIN=$wa/firefox
	# shellcheck source=/dev/null
	. "$wa/lib.sh"
	# shellcheck disable=SC2329
	brave_installed() { return 1; }
	setup_webapps
	setup_webapps
)
wapps=$wa/home/.local/share/applications
new=firefox-web.whatsapp.com__-Browser
{
	[ ! -e "$wapps/brave-web.whatsapp.com__-Default.desktop" ] &&
		[ "$(grep -c "big-webapps-exec filename=\"$new.desktop\" firefox --class=\"$new\" --profile-directory=\"Browser\" --app=\"https://web.whatsapp.com\"" "$wapps/$new.desktop")" = 2 ] &&
		grep -qx "StartupWMClass=$new" "$wapps/$new.desktop" &&
		! grep -q brave "$wapps/$new.desktop"
} || fail "WebApps: WhatsApp do /etc/skel não passou para o Firefox"
python3 -I -c 'import json,sys; d=json.load(open(sys.argv[1]))[0]; assert d == {"browser": "firefox", "app_file": sys.argv[2], "app_profile": "Browser"}, d' \
	"$wa/home/.local/share/biglinux-webapps/webapps.json" "$new.desktop" || fail "WebApps: webapps.json não acompanhou"
grep -q '^Name=Meu Discord$' "$wapps/brave-discord.com__-Default.desktop" && [ ! -e "$wapps/firefox-discord.com__-Browser.desktop" ] ||
	fail "WebApps: WebApp editado pelo usuário foi trocado"
rm -rf "$wa"
pass "WebApps do /etc/skel no Firefox (sem o Brave); os do usuário ficam"

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

# Gerenciador de arquivos padrão: com o mimeapps.list da BigLinux (pastas
# no Dolphin) e Kate, VSCodium, Catfish, Dolphin e Nautilus instalados
# (todos se registram para inode/directory), a sessão abre pastas no Thunar;
# a escolha do usuário (~/.config/mimeapps.list) vale mais.
if command -v gio >/dev/null && command -v update-desktop-database >/dev/null; then
	fm=$(mktemp -d)
	mkdir -p "$fm/data/applications" "$fm/etc" "$fm/home/.config" "$fm/bin"
	fmapp() { # id, categorias
		printf '[Desktop Entry]\nType=Application\nName=%s\nExec=sh -c true %%U\nMimeType=inode/directory;\nCategories=%s\n' \
			"$1" "$2" >"$fm/data/applications/$1.desktop"
	}
	fmapp org.kde.kate 'Qt;KDE;TextEditor;'
	fmapp vscodium 'Development;IDE;'
	fmapp org.xfce.Catfish 'GTK;Utility;'
	fmapp org.kde.dolphin 'Qt;KDE;System;FileTools;FileManager;'
	fmapp org.gnome.Nautilus 'GNOME;GTK;Utility;Core;FileManager;'
	fmapp thunar 'System;Core;GTK;FileTools;FileManager;'
	update-desktop-database -q "$fm/data/applications" 2>/dev/null
	printf '[Default Applications]\ninode/directory=org.kde.dolphin.desktop;\n' >"$fm/etc/mimeapps.list"
	cp data/xdg/windowmaker-mimeapps.list data/xdg/wlmaker-mimeapps.list "$fm/etc/"
	fmenv=(HOME="$fm/home" XDG_CONFIG_HOME="$fm/home/.config" XDG_DATA_HOME="$fm/home/.local/share"
		XDG_CONFIG_DIRS="$fm/etc" XDG_DATA_DIRS="$fm/data")
	for desk in WindowMaker wlmaker; do
		got=$(env "${fmenv[@]}" XDG_CURRENT_DESKTOP=$desk gio mime inode/directory 2>/dev/null | head -n1)
		case $got in *thunar.desktop*) ;; *) fail "pastas ($desk): esperado Thunar, gio diz: $got" ;; esac
		got=$(env "${fmenv[@]}" XDG_CURRENT_DESKTOP=$desk xdg-mime query default inode/directory 2>/dev/null)
		[ "$got" = thunar.desktop ] || fail "pastas ($desk): esperado Thunar, xdg-mime diz: $got"
	done
	# big-wm-open files: o padrão da sessão; um padrão que não é gerenciador
	# de arquivos (Kate) cai no Thunar; a escolha do usuário (Nautilus) vale.
	printf '#!/bin/sh\necho "gtk-launch $*"\n' >"$fm/bin/gtk-launch"
	printf '#!/bin/sh\necho "thunar $*"\n' >"$fm/bin/thunar"
	chmod +x "$fm/bin/gtk-launch" "$fm/bin/thunar"
	openfiles() { env "${fmenv[@]}" XDG_CURRENT_DESKTOP=WindowMaker PATH="$fm/bin:$PATH" \
		BIGWM_LIBDIR=session/lib bash session/bin/big-wm-open files 2>/dev/null; }
	[ "$(openfiles)" = "gtk-launch thunar.desktop $fm/home" ] || fail "big-wm-open files não abriu o Thunar: $(openfiles)"
	printf '[Default Applications]\ninode/directory=org.kde.kate.desktop;\n' >"$fm/home/.config/mimeapps.list"
	[ "$(openfiles)" = "thunar $fm/home" ] || fail "big-wm-open files abriu um editor como gerenciador: $(openfiles)"
	printf '[Default Applications]\ninode/directory=org.gnome.Nautilus.desktop;\n' >"$fm/home/.config/mimeapps.list"
	[ "$(openfiles)" = "gtk-launch org.gnome.Nautilus.desktop $fm/home" ] || fail "big-wm-open files ignorou a escolha do usuário: $(openfiles)"
	rm -rf "$fm"
	pass "pastas no Thunar com Kate/Dolphin/Nautilus instalados; escolha do usuário respeitada"
else
	echo "aviso: gio/update-desktop-database ausentes, padrão de pastas não verificado" >&2
fi

# Papel de parede: no X11 sempre com escala suave e sem deformar por padrão
# (o "wmsetbg -u -s" do menu esticava com o vizinho mais próximo e gravava
# SmoothWorkspaceBack = NO); --mode troca o ajuste e fica guardado; trocar
# de imagem mantém o ajuste.
wp=$(mktemp -d)
mkdir -p "$wp/bin" "$wp/cfg"
printf '#!/bin/sh\necho "$*" >>"%s/calls"\n' "$wp" >"$wp/bin/wmsetbg"
chmod +x "$wp/bin/wmsetbg"
touch "$wp/a b.png" "$wp/c.jpg"
wall() { env -u WAYLAND_DISPLAY PATH="$wp/bin:$PATH" XDG_CONFIG_HOME="$wp/cfg" bash session/bin/big-wm-wallpaper "$@"; }
wall "$wp/a b.png" && wall --mode span && wall "$wp/c.jpg" && wall --restore && wall --mode fit &&
	wall --mode nada 2>/dev/null; true
expected="-u -S -f $wp/a b.png
-u -S -X -f $wp/a b.png
-u -S -X -f $wp/c.jpg
-u -S -X -f $wp/c.jpg
-u -S -a $wp/c.jpg"
[ "$(cat "$wp/calls")" = "$expected" ] || fail "big-wm-wallpaper (X11): $(cat "$wp/calls")"
[ "$(cat "$wp/cfg/big-wm/wallpaper")" = "$wp/c.jpg
fit" ] || fail "big-wm-wallpaper: escolha guardada errada: $(cat "$wp/cfg/big-wm/wallpaper")"
rm -rf "$wp"
pass "papel de parede: escala suave, modos de ajuste guardados"

# big-wm-monitors: o layout salvo no ARandR é aplicado no início da sessão
# só com todas as saídas dele conectadas; se o xrandr falhar, automático.
mon=$(mktemp -d)
mkdir -p "$mon/bin" "$mon/home/.screenlayout"
cat >"$mon/bin/xrandr" <<'EOF2'
#!/bin/sh
if [ "$1" = --query ]; then for o in $CONNECTED; do echo "$o connected 100x100+0+0"; done; exit 0; fi
echo "$*" >>"$LOGF"; [ -n "$FAILX" ] && exit 1; exit 0
EOF2
chmod +x "$mon/bin/xrandr"
printf '#!/bin/sh\nxrandr --output DP-1 --primary --mode 3440x1440 --pos 0x0 --output HDMI-1 --mode 1920x1080 --pos 3440x0 --output VGA-1 --off\n' \
	>"$mon/home/.screenlayout/tres.sh"
monrun() { rm -f "$mon/log"; env HOME="$mon/home" XDG_CONFIG_HOME="$mon/home/.config" DISPLAY=:0 \
	PATH="$mon/bin:$PATH" LOGF="$mon/log" BIGWM_MONITORS_LEGACY=1 "$@" bash session/bin/big-wm-monitors --restore; cat "$mon/log" 2>/dev/null; }
case $(monrun CONNECTED="DP-1 HDMI-1") in *"--output DP-1 --primary"*) ;; *) fail "big-wm-monitors não aplicou o layout com as saídas conectadas" ;; esac
[ -z "$(monrun CONNECTED="DP-1")" ] || fail "big-wm-monitors aplicou um layout com uma saída desconectada"
case $(monrun CONNECTED="DP-1 HDMI-1" FAILX=1) in *"--output HDMI-1 --auto"*) ;; *) fail "big-wm-monitors sem o automático quando o xrandr falha" ;; esac
# Com o Big WM Display Resolution instalado, a restauração é dele (os
# scripts do ARandR viram perfis, sem serem executados).
cat >"$mon/bin/big-wm-display-resolution" <<'EOF2'
#!/bin/sh
echo "bwmdr $*" >>"$LOGF"
EOF2
chmod +x "$mon/bin/big-wm-display-resolution"
out=$(rm -f "$mon/log"; env HOME="$mon/home" XDG_CONFIG_HOME="$mon/home/.config" DISPLAY=:0 PATH="$mon/bin:$PATH" \
	LOGF="$mon/log" CONNECTED="DP-1 HDMI-1" bash session/bin/big-wm-monitors --restore; cat "$mon/log")
[ "$out" = "bwmdr --restore" ] || fail "big-wm-monitors não passou a restauração ao big-wm-display-resolution: $out"
rm -rf "$mon"
pass "big-wm-monitors (layout do ARandR, saídas conferidas, automático na falha; delega ao Display Resolution)"

# Conjuntos de ícones: os temas de ícones do XDG instalados (sistema e
# usuário), sem ocultos, sem temas só de cursor, sem duplicatas (o do
# usuário vale mais), com o nome traduzido; um tema instalado depois aparece
# (cache invalidado pela data do diretório); big-wm-theme icons aplica.
it=$(mktemp -d)
ith() { mkdir -p "$1"; printf '[Icon Theme]\n%s\n' "$2" >"$1/index.theme"; }
ith "$it/sys/icons/Papirus" $'Name=Papirus\nDirectories=48x48/apps'
ith "$it/sys/icons/Oculto" $'Name=Oculto\nHidden=true\nDirectories=a'
ith "$it/sys/icons/SoCursor" $'Name=Só cursor'
ith "$it/sys/icons/Breeze" $'Name=Breeze\nName[pt_BR]=Brisa\nDirectories=a'
ith "$it/home/icons/Papirus" $'Name=Papirus do usuário\nDirectories=a'
mkdir -p "$it/bin"
cat >"$it/bin/gsettings" <<'EOF2'
#!/bin/sh
[ "$1" = get ] && echo "'Breeze'"
exit 0
EOF2
chmod +x "$it/bin/gsettings"
itm() { env PATH="$it/bin:$PATH" HOME="$it/h" XDG_DATA_HOME="$it/home" XDG_DATA_DIRS="$it/sys" XDG_CACHE_HOME="$it/cache" \
	XDG_CONFIG_HOME="$it/cfg" LANG=pt_BR.UTF-8 LC_ALL= python3 -I session/bin/big-wm-appmenu --icon-themes; }
menu=$(itm)
grep -q '"✓ Brisa", EXEC, "big-wm-theme icons Breeze"' <<<"$menu" &&
	grep -q '"    Papirus do usuário", EXEC, "big-wm-theme icons Papirus"' <<<"$menu" &&
	[ "$(grep -c 'icons Papirus' <<<"$menu")" = 1 ] &&
	! grep -q -e Oculto -e 'cursor' <<<"$menu" || fail "big-wm-appmenu --icon-themes: $menu"
sleep 1; ith "$it/sys/icons/Novo" $'Name=Novo\nDirectories=a'; touch "$it/sys/icons"
grep -q 'icons Novo' <<<"$(itm)" || fail "big-wm-appmenu --icon-themes: tema instalado depois não apareceu"
env PATH="$it/bin:$PATH" HOME="$it/h" XDG_DATA_HOME="$it/home" XDG_DATA_DIRS="$it/sys" XDG_CONFIG_HOME="$it/cfg" \
	BIGWM_ICONSDIR="$it/none" BIGWM_LIBDIR="$PWD/session/lib" bash session/bin/big-wm-theme icons Papirus 2>/dev/null &&
	grep -qx 'gtk-icon-theme-name=Papirus' "$it/cfg/gtk-3.0/settings.ini" &&
	grep -qx 'gtk-icon-theme-name=Papirus' "$it/cfg/gtk-4.0/settings.ini" ||
	fail "big-wm-theme icons não gravou o tema nos settings.ini"
# XSETTINGS (sessão X11 com xsettingsd): tema GTK do modo e ícones em uso.
grep -qx 'Net/IconThemeName "Papirus"' "$it/cfg/big-wm/xsettingsd.conf" &&
	grep -qx 'Net/ThemeName "OneStepBack"' "$it/cfg/big-wm/xsettingsd.conf" ||
	fail "big-wm-theme icons: xsettingsd.conf: $(cat "$it/cfg/big-wm/xsettingsd.conf" 2>&1)"
mkdir -p "$it/themes/OneStepBack/gtk-3.0" "$it/themes/OneStepBack-Dark/gtk-3.0" && echo dark >"$it/cfg/big-wm/theme"
env PATH="$it/bin:$PATH" HOME="$it/h" XDG_CONFIG_HOME="$it/cfg" BIGWM_THEMESDIR="$it/themes" \
	BIGWM_LIBDIR="$PWD/session/lib" bash session/bin/big-wm-theme xsettings &&
	grep -qx 'Net/ThemeName "OneStepBack-Dark"' "$it/cfg/big-wm/xsettingsd.conf" &&
	grep -qx 'Net/IconThemeName "Papirus"' "$it/cfg/big-wm/xsettingsd.conf" ||
	fail "big-wm-theme xsettings (escuro): $(cat "$it/cfg/big-wm/xsettingsd.conf" 2>&1)"
# Wayland: GTK_THEME com o nome estável; BIGWM_GTK_THEME=0 desliga; um
# GTK_THEME do usuário fica.
mkdir -p "$it/xsbin" && printf '#!/bin/sh\n' >"$it/xsbin/xsettingsd" && chmod +x "$it/xsbin/xsettingsd"
# shellcheck disable=SC2016 # código para o bash filho
envgtk() { env -u GTK_THEME PATH="$it/xsbin:$PATH" HOME="$it/h" XDG_CONFIG_HOME="$it/cfg" XDG_DATA_HOME="$it/h/.local/share" BIGWM_THEMESDIR="$it/themes" \
	BIGWM_LIBDIR="$PWD/session/lib" XDG_CURRENT_DESKTOP="$1" BIGWM_GTK_THEME="$2" ${3:+GTK_THEME=$3} \
	bash -c '. session/lib/big-wm-env.sh; echo "${GTK_THEME:-}"'; }
# X11 com xsettingsd: sem GTK_THEME (as janelas abertas seguem o XSETTINGS;
# com o GTK_THEME o gtk-nocsd fixaria o tema), mas com o link para o GTK 2;
# BIGWM_GTK_THEME=1 volta ao GTK_THEME.
[ -z "$(envgtk WindowMaker '')" ] || fail "big-wm-env.sh: GTK_THEME com xsettingsd na sessão X11: $(envgtk WindowMaker '')"
[ "$(readlink "$it/h/.local/share/themes/BigWindowMaker")" = "$it/themes/OneStepBack-Dark" ] ||
	fail "big-wm-env.sh: link do tema GTK sem o modo escuro"
[ "$(envgtk WindowMaker 1)" = BigWindowMaker ] || fail "big-wm-env.sh: BIGWM_GTK_THEME=1 não voltou ao GTK_THEME"
[ "$(envgtk wlmaker '')" = BigWindowMaker ] || fail "big-wm-env.sh: Wayland sem o BigWindowMaker: $(envgtk wlmaker '')"
[ -z "$(envgtk wlmaker 0)" ] || fail "big-wm-env.sh: BIGWM_GTK_THEME=0 não desligou o GTK_THEME"
[ "$(envgtk wlmaker '' Adwaita:dark)" = Adwaita:dark ] || fail "big-wm-env.sh: trocou o GTK_THEME do usuário"
env PATH="$it/bin:$PATH" HOME="$it/h" XDG_DATA_HOME="$it/home" XDG_DATA_DIRS="$it/sys" XDG_CONFIG_HOME="$it/cfg" \
	BIGWM_ICONSDIR="$it/none" BIGWM_LIBDIR="$PWD/session/lib" bash session/bin/big-wm-theme icons ../etc 2>/dev/null &&
	fail "big-wm-theme icons aceitou um nome com /"
rm -rf "$it"
pass "conjuntos de ícones: temas do XDG descobertos, cache, aplicação"

# big-wm-terminal: terminal preferido na pasta pedida, sem shell no meio
# (pasta com espaço, aspas e $), linha de comando do exo num sh -c.
tt=$(mktemp -d)
for t in ashyterm xdg-terminal-exec; do
	# shellcheck disable=SC2016 # o script falso expande na hora de rodar
	printf '#!/bin/sh\n[ "$1" = --print-id ] && { echo "$FAKE_ID"; exit 0; }\nprintf "%%s|" "${0##*/}" "$PWD" "$@"\n' >"$tt/$t"
	chmod +x "$tt/$t"
done
tdir="$tt/uma pasta \$HOME 'x'"
mkdir -p "$tdir"
term() { (cd "$tdir" && PATH="$tt:$PATH" FAKE_ID=$1 bash "$OLDPWD/session/bin/big-wm-terminal" "${@:2}" 2>/dev/null); }
[ "$(term org.communitybig.ashyterm.desktop)" = "ashyterm|$tdir|--working-directory|$tdir|" ] ||
	fail "big-wm-terminal (AshyTerm): $(term org.communitybig.ashyterm.desktop)"
[ "$(term org.communitybig.ashyterm.desktop 'ls -l "a b"')" = "ashyterm|$tdir|--working-directory|$tdir|-e|sh|-c|ls -l \"a b\"|" ] ||
	fail "big-wm-terminal (linha de comando do exo): $(term org.communitybig.ashyterm.desktop 'ls -l "a b"')"
[ "$(term kitty.desktop -- echo 'a b')" = "xdg-terminal-exec|$tdir|--dir=$tdir|--|echo|a b|" ] ||
	fail "big-wm-terminal (xdg-terminal-exec): $(term kitty.desktop -- echo 'a b')"
(cd / && PATH="$tt:$PATH" bash "$OLDPWD/session/bin/big-wm-terminal" --dir=/nao/existe >/dev/null 2>&1) &&
	fail "big-wm-terminal aceitou uma pasta que não existe"
rm -rf "$tt"
pass "big-wm-terminal (pasta, AshyTerm, xdg-terminal-exec, linha de comando do exo)"

# Localizar o ponteiro ao sacudir: o detector (movimentos sintéticos, sem
# X), o liga/desliga na configuração e a entrada nos dois menus X11.
lp=$(mktemp -d)
lp_full=0
if command -v cc >/dev/null && pkg-config --exists x11 xi xfixes xext xcursor xrandr 2>/dev/null; then
	read -ra lp_cflags <<<"$(pkg-config --cflags x11 xi xfixes xext xcursor xrandr)"
	read -ra lp_libs <<<"$(pkg-config --libs x11 xi xfixes xext xcursor xrandr)"
	cc -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Werror "${lp_cflags[@]}" \
		-o "$lp/lp" session/locate-pointer/big-wm-locate-pointer.c \
		"${lp_libs[@]}" -lm && lp_full=1 || fail "big-wm-locate-pointer não compila sem avisos"
else
	cc -std=c11 -D_GNU_SOURCE -DSELFTEST_ONLY -O2 -o "$lp/lp" session/locate-pointer/big-wm-locate-pointer.c -lm ||
		fail "big-wm-locate-pointer (só o detector) não compila"
fi
if [ -x "$lp/lp" ]; then
	"$lp/lp" --selftest >"$lp/out" || fail "big-wm-locate-pointer --selftest: $(grep FALHOU "$lp/out")"
	if [ "$lp_full" = 1 ]; then
		printf '# meu comentário\nsensitivity=high\nenabled = true\n' >"$lp/conf"
		mkdir -p "$lp/xdg/big-wm" && cp "$lp/conf" "$lp/xdg/big-wm/locate-pointer.conf"
		XDG_CONFIG_HOME=$lp/xdg DISPLAY='' "$lp/lp" --disable >/dev/null
		[ "$(cat "$lp/xdg/big-wm/locate-pointer.conf")" = "$(printf '# meu comentário\nsensitivity=high\nenabled=false')" ] ||
			fail "big-wm-locate-pointer --disable: $(cat "$lp/xdg/big-wm/locate-pointer.conf")"
		XDG_CONFIG_HOME=$lp/xdg DISPLAY='' "$lp/lp" --toggle >/dev/null
		grep -qx 'enabled=true' "$lp/xdg/big-wm/locate-pointer.conf" || fail "big-wm-locate-pointer --toggle não religou"
	fi
	# Com Xvfb: o daemon amplia o cursor que o servidor mostra e o devolve
	# ao tamanho normal (também com SIGTERM no meio); um daemon novo assume
	# os cursores deixados por um que morreu (SIGKILL); --show sem daemon
	# não troca cursor nenhum (o dono de um cursor trocado é quem o criou).
	xvfb=${BIG_WM_TEST_XVFB:-$(command -v Xvfb || true)}
	if [ "$lp_full" = 1 ] && [ -n "$xvfb" ] && [ -e /usr/share/icons/Adwaita/cursors/left_ptr ] &&
		cc -std=c11 -D_GNU_SOURCE -O2 -Wall -Werror "${lp_cflags[@]}" -o "$lp/probe" tests/locate-pointer-probe.c "${lp_libs[@]}"; then
		"$xvfb" -displayfd 3 -screen 0 800x600x24 -nolisten tcp 3>"$lp/dpy" >/dev/null 2>&1 &
		lp_x=$!
		for _ in $(seq 50); do [ -s "$lp/dpy" ] && break; sleep 0.1; done
		mkdir -m 700 "$lp/rt" "$lp/cfg"
		(
			lp_bad=0
			fail() {
				echo "FALHOU: $*" >&2
				lp_bad=1
			}
			# shellcheck disable=SC2030
			DISPLAY=":$(cat "$lp/dpy")"
			# shellcheck disable=SC2030
			export DISPLAY XCURSOR_THEME=Adwaita XCURSOR_SIZE=24 \
				XDG_RUNTIME_DIR=$lp/rt XDG_CONFIG_HOME=$lp/cfg BIG_WM_LOCATE_POINTER_DEBUG=1
			daemon() {
				"$lp/lp" 2>>"$lp/log" &
				lp_d=$!
				for _ in $(seq 30); do [ -s "$lp/rt/big-wm-locate-pointer$DISPLAY.pid" ] && break; sleep 0.1; done
				sleep 0.2
			}
			grow() { # $1: ms até matar com o sinal $2 (vazio: animação inteira)
				"$lp/probe" max "${1:-1500}" >"$lp/max" &
				sleep 0.05
				"$lp/lp" --show
				if [ -n "${2:-}" ]; then
					sleep "$(printf '0.%03d' "$1")"
					kill "-$2" "$lp_d"
					wait "$lp_d" 2>/dev/null
				fi
				wait $!
			}
			"$lp/probe" hold 20000 2>/dev/null &
			sleep 0.3
			[ "$("$lp/probe" now)" = "24x24 left_ptr" ] || fail "locate-pointer: cursor inicial $("$lp/probe" now)"
			daemon
			grow
			[ "$(cat "$lp/max")" = "72x72" ] || fail "locate-pointer: o cursor não cresceu para 72x72 ($(cat "$lp/max"))"
			[ "$("$lp/probe" now)" = "24x24 left_ptr" ] || fail "locate-pointer: depois da animação: $("$lp/probe" now 2>&1)"
			[ "$(cat "$lp/rt/big-wm-locate-pointer$DISPLAY.names")" = left_ptr ] || fail "locate-pointer: nome trocado não anotado"
			"$lp/lp" 2>/dev/null
			[ "$(wc -l <"$lp/rt/big-wm-locate-pointer$DISPLAY.pid")" = 1 ] && kill -0 "$lp_d" ||
				fail "locate-pointer: segundo daemon no mesmo display"
			grow 450 TERM
			[ "$("$lp/probe" now)" = "24x24 left_ptr" ] || fail "locate-pointer: depois de SIGTERM: $("$lp/probe" now 2>&1)"
			daemon
			grow 450 KILL
			daemon
			grep -q 'assumindo o cursor "left_ptr"' "$lp/log" || fail "locate-pointer: o daemon novo não assumiu os cursores"
			[ "$("$lp/probe" now)" = "24x24 left_ptr" ] || fail "locate-pointer: depois de SIGKILL e de outro daemon: $("$lp/probe" now 2>&1)"
			kill "$lp_d"
			wait "$lp_d"
			# sem daemon: janela por cima, o cursor do servidor não muda de dono
			"$lp/probe" max 1500 >"$lp/max" &
			sleep 0.05
			"$lp/lp" --show 2>>"$lp/log"
			wait $!
			[ "$(cat "$lp/max")" = "24x24" ] || fail "locate-pointer: --show sem daemon trocou o cursor ($(cat "$lp/max"))"
			[ "$("$lp/probe" now)" = "24x24 left_ptr" ] || fail "locate-pointer: --show sem daemon: $("$lp/probe" now 2>&1)"
			exit "$lp_bad"
		) || failures=$((failures + 1))
		kill "$lp_x" 2>/dev/null
		wait "$lp_x" 2>/dev/null
	fi
fi
rm -rf "$lp"
for m in data/GNUstep/Defaults/WMRootMenu data/GNUstep/Defaults/WMRootMenu.pt_BR; do
	grep -q 'EXEC, "big-wm-locate-pointer --toggle")$' "$m" || fail "$m: falta Localizar o ponteiro ao sacudir"
done
pass "big-wm-locate-pointer (detector, liga/desliga, menu; com Xvfb: cursor do servidor, SIGTERM, SIGKILL)"

# Ampliação da tela no X11 (plugin do picom): as contas e a configuração (sem
# X); com Xvfb, picom e xdotool, a tela de verdade: Ctrl+Alt+rolagem amplia
# mantendo sob o ponteiro o que estava sob ele, a rolagem sem os
# modificadores não amplia, Ctrl+Alt+0 volta ao quadro original e, parado,
# o plugin não pede quadros.
zt=$(mktemp -d)
cc -std=c11 -D_GNU_SOURCE -DZOOM_SELFTEST -O2 -Wall -Wextra -Werror -o "$zt/selftest" \
	session/zoom/big-wm-picom-zoom.c -lm || fail "big-wm-picom-zoom (contas) não compila sem avisos"
if [ -x "$zt/selftest" ]; then
	"$zt/selftest" >"$zt/out" || fail "big-wm-picom-zoom: $(grep FALHOU "$zt/out")"
fi
zoom_libs="xcb xcb-xinput xcb-randr xcb-keysyms pixman-1"
zt_full=0
# shellcheck disable=SC2086 # lista de pacotes do pkg-config
if pkg-config --exists $zoom_libs 2>/dev/null && [ -e /usr/include/ev.h ]; then
	read -ra zt_cflags <<<"$(pkg-config --cflags $zoom_libs)"
	read -ra zt_libs <<<"$(pkg-config --libs $zoom_libs)"
	cc -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -fPIC -shared "${zt_cflags[@]}" \
		-o "$zt/zoom.so" session/zoom/big-wm-picom-zoom.c "${zt_libs[@]}" -lev -lm &&
		zt_full=1 || fail "big-wm-picom-zoom.so não compila sem avisos"
fi
xvfb=${BIG_WM_TEST_XVFB:-$(command -v Xvfb || true)}
if [ "$zt_full" = 1 ] && [ -n "$xvfb" ] && command -v picom >/dev/null && command -v xdotool >/dev/null &&
	command -v xprop >/dev/null &&
	cc -std=c11 -O2 -Wall -Werror -o "$zt/probe" tests/zoom-probe.c "${zt_libs[@]}"; then
	"$xvfb" -displayfd 3 -screen 0 800x600x24 -nolisten tcp -noreset 3>"$zt/dpy" >/dev/null 2>&1 &
	zt_x=$!
	for _ in $(seq 50); do [ -s "$zt/dpy" ] && break; sleep 0.1; done
	mkdir -p "$zt/cfg/big-wm"
	printf 'enabled=true\nmax-level=8\nstep=1.25\nanimation-ms=120\n' >"$zt/cfg/big-wm/zoom.conf"
	: >"$zt/picom.conf"
	(
		zt_bad=0
		fail() {
			echo "FALHOU: $*" >&2
			zt_bad=1
		}
		# shellcheck disable=SC2030,SC2031
		DISPLAY=":$(cat "$zt/dpy")"
		# shellcheck disable=SC2030,SC2031
		export DISPLAY XDG_CONFIG_HOME=$zt/cfg
		"$zt/probe" show &
		zt_p=$!
		picom --config "$zt/picom.conf" --backend xrender --plugins "$zt/zoom.so" 2>"$zt/picom.log" &
		zt_c=$!
		serial() { xprop -root _BIG_WM_ZOOM 2>/dev/null | sed -n 's/.* = //p'; }
		for _ in $(seq 50); do [ -n "$(serial)" ] && break; sleep 0.1; done
		[ -n "$(serial)" ] || fail "zoom: o picom não carregou o plugin ($(head -c 300 "$zt/picom.log"))"
		red() { "$zt/probe" pixel "$1" "$2" | cut -d' ' -f1; }
		xdotool mousemove 400 300
		sleep 0.5
		[ "$(red 400 300)" = 100 ] && [ "$(red 600 300)" = 150 ] ||
			fail "zoom: listras antes de ampliar: $(red 400 300) $(red 600 300)"
		xdotool click 4 click 4
		sleep 0.5
		[ "$(red 600 300)" = 150 ] || fail "zoom: a rolagem sem modificadores ampliou"
		xdotool keydown ctrl keydown alt click 4 click 4 click 4 click 4 keyup alt keyup ctrl
		sleep 0.8
		# 1.25^4: sob o ponteiro o mesmo; 200 px à direita, 400 + 200/2.44 = 482.
		r=$(red 400 300)
		[ "$r" -ge 99 ] && [ "$r" -le 101 ] || fail "zoom: o ponto sob o ponteiro mudou ($r, era 100)"
		r=$(red 600 300)
		[ "$r" -ge 119 ] && [ "$r" -le 121 ] || fail "zoom: 200 px à direita mostra $r (esperado ~120)"
		s1=$(serial)
		sleep 1
		[ "$(serial)" = "$s1" ] || fail "zoom: parado, o plugin continua pedindo quadros"
		xdotool mousemove 700 300
		sleep 0.5
		r=$(red 700 300)
		[ "$r" -ge 174 ] && [ "$r" -le 176 ] || fail "zoom: o ponteiro andou e a área não o seguiu ($r, era 175)"
		xdotool key ctrl+alt+0
		sleep 0.8
		[ "$(red 600 300)" = 150 ] && [ "$(red 100 50)" = 25 ] ||
			fail "zoom: Ctrl+Alt+0 não voltou ao quadro original: $(red 600 300) $(red 100 50)"
		kill "$zt_c" "$zt_p" 2>/dev/null
		wait "$zt_c" "$zt_p" 2>/dev/null
		exit "$zt_bad"
	) || failures=$((failures + 1))
	kill "$zt_x" 2>/dev/null
	wait "$zt_x" 2>/dev/null
	zt_msg="com Xvfb e picom: ampliação, rolagem normal, Ctrl+Alt+0, repouso"
else
	zt_msg="sem Xvfb/picom/xdotool: só as contas"
fi
rm -rf "$zt"
pass "big-wm-picom-zoom ($zt_msg)"

# Nome do sistema (big-wm-name) sobre os arquivos como a base os deixa: o
# os-release do filesystem (ID=manjaro), o lsb-release do community-release e
# o /etc/issue do filesystem. O ID passa a ser o da BigCommunity.
nr=$(mktemp -d)
mkdir -p "$nr/usr/lib" "$nr/etc"
cat >"$nr/usr/lib/os-release" <<'EOF'
NAME="Manjaro Linux"
PRETTY_NAME="Manjaro Linux"
ID=manjaro
ID_LIKE=arch
BUILD_ID=rolling
HOME_URL="https://communitybig.org"
LOGO="manjaro"
EOF
ln -s ../usr/lib/os-release "$nr/etc/os-release"
printf 'DISTRIB_ID="BigCommunity based in Manjaro Linux"\nDISTRIB_RELEASE="1.8.0"\nDISTRIB_DESCRIPTION="BigCommunity"\n' >"$nr/etc/lsb-release"
printf 'Manjaro Linux \\r (\\l)\n\n' >"$nr/etc/issue"
for _ in 1 2; do # o hook roda a cada atualização: o resultado não pode acumular
	BIG_WM_NAME_ROOT=$nr bash data/libalpm/big-wm-name || fail "big-wm-name falhou"
done
grep -qx 'PRETTY_NAME="Big WindowMaker, based on BigLinux"' "$nr/usr/lib/os-release" &&
	grep -qx 'NAME="Big WindowMaker"' "$nr/usr/lib/os-release" &&
	grep -qx 'ID=bigcommunity' "$nr/usr/lib/os-release" &&
	grep -qx 'LOGO="big-wm"' "$nr/usr/lib/os-release" &&
	[ "$(grep -c '^NAME=' "$nr/usr/lib/os-release")" = 1 ] ||
	fail "big-wm-name: os-release $(tr '\n' ' ' <"$nr/usr/lib/os-release")"
[ -L "$nr/etc/os-release" ] || fail "big-wm-name trocou o link /etc/os-release por um arquivo"
grep -qx 'DISTRIB_DESCRIPTION="Big WindowMaker, based on BigLinux"' "$nr/etc/lsb-release" &&
	grep -qx 'DISTRIB_RELEASE="1.8.0"' "$nr/etc/lsb-release" ||
	fail "big-wm-name: lsb-release $(tr '\n' ' ' <"$nr/etc/lsb-release")"
[ "$(head -n1 "$nr/etc/issue")" = 'Big WindowMaker, based on BigLinux \r (\l)' ] ||
	fail "big-wm-name: /etc/issue $(head -n1 "$nr/etc/issue")"
grep -rqi 'bigcommunity, based\|communitybig' "$nr" && fail "big-wm-name: sobrou a marca BigCommunity"
rm -rf "$nr"
pass "big-wm-name (os-release, lsb-release, /etc/issue)"

# Tela de bloqueio: o fundo (cada modo, arquivo 0600, captura só pela
# memória), a prévia, o layout do gtklock (ids que ele procura) e, com
# Xvfb, a conversa do módulo de autenticação com um authproto falso: senha
# errada mantém o diálogo, a certa sai com 0 (o xsecurelock desbloqueia).
ls_dir=$(mktemp -d)
ls_libs=(x11 xi xrandr cairo cairo-xlib gdk-pixbuf-2.0 fontconfig)
if command -v cc >/dev/null && pkg-config --exists "${ls_libs[@]}" 2>/dev/null; then
	read -ra ls_cflags <<<"$(pkg-config --cflags "${ls_libs[@]}")"
	read -ra ls_ldlibs <<<"$(pkg-config --libs "${ls_libs[@]}")"
	if cc -std=gnu11 -O2 -Wall -Wextra -Werror "${ls_cflags[@]}" -o "$ls_dir/big-wm-lockscreen" \
		session/lockscreen/big-wm-lockscreen.c "${ls_ldlibs[@]}" -lm; then
		ls=$ls_dir/big-wm-lockscreen
		mkdir -p "$ls_dir/cfg/big-wm"
		# Papel de parede de teste: uma PPM convertida pelo próprio gdk-pixbuf? Não:
		# o modo black/gray não precisa de imagem; o blur vem de uma PPM.
		for mode in black gray; do
			printf '[Lock]\nBackground=%s\n' "$mode" >"$ls_dir/cfg/big-wm/lock.conf"
			XDG_CONFIG_HOME=$ls_dir/cfg "$ls" background "$ls_dir/$mode.png" &&
				[ "$(stat -c %a "$ls_dir/$mode.png")" = 600 ] || fail "big-wm-lockscreen background ($mode): $(stat -c %a "$ls_dir/$mode.png" 2>&1)"
		done
		printf '[Lock]\nBackground=blur\nBlur=4\nDim=45\n' >"$ls_dir/cfg/big-wm/lock.conf"
		{ printf 'P6 64 32 255\n'; head -c $((64 * 32 * 3)) /dev/urandom; } >"$ls_dir/shot.ppm"
		XDG_CONFIG_HOME=$ls_dir/cfg "$ls" background --ppm - "$ls_dir/blur.png" <"$ls_dir/shot.ppm" &&
			python3 -I -c 'import struct,sys; d=open(sys.argv[1],"rb").read(24); w,h=struct.unpack(">II",d[16:24]); sys.exit(0 if (w,h)==(16,8) else 1)' "$ls_dir/blur.png" ||
			fail "big-wm-lockscreen background --ppm: fundo em quarto de resolução"
		{ printf 'P6 64 32 255\n'; head -c 10 /dev/zero; } >"$ls_dir/bad.ppm"
		XDG_CONFIG_HOME=$ls_dir/cfg "$ls" background --ppm "$ls_dir/bad.ppm" "$ls_dir/bad.png" &&
			[ -s "$ls_dir/bad.png" ] || fail "big-wm-lockscreen: captura curta não pode impedir o bloqueio (fundo de reserva)"
		XDG_CONFIG_HOME=$ls_dir/cfg HOME=$ls_dir "$ls" preview "$ls_dir/preview.png" 320x180 && [ -s "$ls_dir/preview.png" ] ||
			fail "big-wm-lockscreen preview"
		grep -q 'capture_x11\|load_ppm' session/lockscreen/big-wm-lockscreen.c && ! grep -q 'cairo_surface_write_to_png(shot' session/lockscreen/big-wm-lockscreen.c ||
			fail "big-wm-lockscreen: a captura bruta não pode ir para o disco"
		xvfb=${BIG_WM_TEST_XVFB:-$(command -v Xvfb || true)}
		if [ -n "$xvfb" ] && command -v xwininfo >/dev/null; then
			mkdir -p "$ls_dir/helpers"
			ln -s "$ls" "$ls_dir/helpers/auth_big_wm"
			cat >"$ls_dir/helpers/authproto_pam" <<-'EOP'
				#!/bin/bash
				printf 'P 10\nPassword: \n'
				read -r hdr || exit 1
				type=${hdr%% *} len=${hdr#* }
				IFS= read -r -N "$len" pw
				read -r _
				[ "$type" = p ] && [ "$pw" = certa ] && exit 0
				printf 'e 22\nAuthentication failure\n'
				exit 1
			EOP
			chmod +x "$ls_dir/helpers/authproto_pam"
			"$xvfb" -displayfd 3 -screen 0 800x600x24 -nolisten tcp 3>"$ls_dir/dpy" >/dev/null 2>&1 &
			ls_x=$!
			for _ in $(seq 50); do [ -s "$ls_dir/dpy" ] && break; sleep 0.1; done
			(
				# shellcheck disable=SC2031
				DISPLAY=":$(cat "$ls_dir/dpy")"
				# shellcheck disable=SC2031
				export DISPLAY XDG_CONFIG_HOME=$ls_dir/cfg
				# Como o xsecurelock: a janela do auth chega sem mapear.
				python3 -c '
import ctypes, ctypes.util, sys, time
X = ctypes.cdll.LoadLibrary(ctypes.util.find_library("X11"))
X.XOpenDisplay.restype = ctypes.c_void_p
X.XCreateSimpleWindow.restype = ctypes.c_ulong
d = ctypes.c_void_p(X.XOpenDisplay(None))
w = X.XCreateSimpleWindow(d, ctypes.c_ulong(X.XDefaultRootWindow(d)), 0, 0, 800, 600, 0, ctypes.c_ulong(0), ctypes.c_ulong(0))
X.XFlush(d)
print(w, flush=True)
time.sleep(30)' >"$ls_dir/authwin" &
				holder=$!
				for _ in $(seq 50); do [ -s "$ls_dir/authwin" ] && break; sleep 0.1; done
				XSCREENSAVER_WINDOW=$(cat "$ls_dir/authwin")
				export XSCREENSAVER_WINDOW
				mkfifo "$ls_dir/keys"
				exec 7<>"$ls_dir/keys"
				"$ls_dir/helpers/auth_big_wm" <"$ls_dir/keys" &
				au=$!
				sleep 0.5
				xwininfo -id "$XSCREENSAVER_WINDOW" | grep -q 'Map State: IsViewable' ||
					{ echo "FALHOU: auth_big_wm não mapeou a janela do auth (diálogo escondido)" >&2; kill "$holder" "$au"; exit 1; }
				printf 'errada\r' >&7
				sleep 0.8
				kill -0 "$au" 2>/dev/null || { echo "FALHOU: auth_big_wm saiu com a senha errada" >&2; exit 1; }
				printf 'certa\r' >&7
				for _ in $(seq 40); do kill -0 "$au" 2>/dev/null || break; sleep 0.1; done
				wait "$au" || { echo "FALHOU: auth_big_wm não saiu com 0 depois da senha certa" >&2; exit 1; }
				kill "$holder" 2>/dev/null
			) || fail "big-wm-lockscreen: conversa do auth_big_wm com o authproto"
			kill "$ls_x" 2>/dev/null
		fi
		# Bloqueio com o picom do Big WindowMaker: sem o aviso "INCOMPATIBLE
		# COMPOSITOR" ao desbloquear e sem a sessão à mostra enquanto
		# bloqueado (BIG_WM_LOCK_CYCLES=30 para a rodada completa).
		tests/integration/lock-compositor.sh "$ls" data/picom.conf "${BIG_WM_LOCK_CYCLES:-3}" >"$ls_dir/lockc.log" 2>&1
		case $? in
		0 | 77) ;;
		*) fail "bloqueio com o picom: $(tail -n4 "$ls_dir/lockc.log")" ;;
		esac
		# Teclado na tela do X11: ⌨ abre, senha errada não desbloqueia, a
		# certa desbloqueia, Cancelar não desbloqueia, VirtualKeyboard=false.
		tests/integration/lock-keyboard.sh "$ls" >"$ls_dir/lockkb.log" 2>&1
		case $? in
		0 | 77) ;;
		*) fail "teclado na tela do X11: $(tail -n4 "$ls_dir/lockkb.log")" ;;
		esac
	else
		fail "big-wm-lockscreen não compila sem avisos"
	fi
fi
for id in window-box info-box clock-label date-label body-revealer body-grid input-label input-field \
	message-revealer message-box warning-label error-label unlock-button; do
	grep -q "id=\"$id\"" data/lock/gtklock-layout.ui.in || fail "gtklock: o layout perdeu o id $id"
done
grep -q 'handler="window_pw_check"' data/lock/gtklock-layout.ui.in || fail "gtklock: Enter não confere a senha"
grep -q '<property name="visibility">0</property>' data/lock/gtklock-layout.ui.in || fail "gtklock: a senha não pode aparecer"
grep -q 'view-reveal' data/lock/gtklock-layout.ui.in && fail "gtklock: sem botão de mostrar a senha"
# O GtkBuilder lê o invisible-char como texto e usa o primeiro caractere:
# "9642" mostrava a senha como "999". Vai o próprio caractere.
grep -q '<property name="invisible-char">■</property>' data/lock/gtklock-layout.ui.in ||
	fail "gtklock: a senha aparece como quadradinhos (■), não como dígitos"
grep -q 'already_locked' session/bin/big-wm-lock || fail "big-wm-lock: bloqueio já ativo conferido por sessão"
# shellcheck disable=SC2016
grep -qF 'XSECURELOCK_AUTH=$helpers/auth_big_wm' session/bin/big-wm-lock ||
	fail "big-wm-lock: o xsecurelock precisa do caminho completo do auth_big_wm (senão usa o auth_x11)"
sed -n '/^install-lockscreen:/,/^$/p' Makefile | grep -q 'ln -s' &&
	fail "install-lockscreen: o xsecurelock recusa módulos que são links simbólicos"
# Teclado na tela do Wayland: módulo do gtklock 4.0 (o gtklock recusa outra
# versão maior), carregado pelo big-wm-lock; texto só pelo campo de senha.
if pkg-config --exists gtk+-3.0 gmodule-2.0 2>/dev/null; then
	# shellcheck disable=SC2046 # flags do pkg-config, uma por palavra
	if cc -std=gnu11 -O2 -Wall -Wextra -Werror -fPIC -shared -o "$ls_dir/kbd.so" session/lockscreen/gtklock-keyboard.c \
		$(pkg-config --cflags --libs gtk+-3.0 gmodule-2.0); then
		syms=$(nm -D --defined-only "$ls_dir/kbd.so" | awk '{print $3}' | tr '\n' ' ')
		for s in module_name module_major_version module_minor_version on_activation on_window_create; do
			[[ " $syms " == *" $s "* ]] || fail "big-wm-keyboard: falta o símbolo $s"
		done
		grep -q 'module_major_version = 4;' session/lockscreen/gtklock-keyboard.c ||
			fail "big-wm-keyboard: versão do gtklock (4)"
		grep -qE 'XSendEvent|xdotool|wtype|ydotool|g_spawn|system\(|printf\(|g_print|g_message' \
			session/lockscreen/gtklock-keyboard.c &&
			fail "big-wm-keyboard: o texto da senha só vai para o campo do gtklock (sem processos, injeção ou log)"
	else
		fail "big-wm-keyboard não compila sem avisos"
	fi
fi
# shellcheck disable=SC2016 # o texto literal do script
grep -qF 'args+=(-m "$keyboard")' session/bin/big-wm-lock || fail "big-wm-lock: carrega o teclado na tela no gtklock"
rm -rf "$ls_dir"
pass "tela de bloqueio (fundo, prévia, layout do gtklock, conversa com o authproto)"

if [ "$failures" -gt 0 ]; then
	echo "$failures verificação(ões) falharam" >&2
	exit 1
fi
echo "todas as verificações passaram"
