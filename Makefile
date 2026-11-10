# SPDX-License-Identifier: GPL-2.0-or-later
#
# Instalação do Big WindowMaker. Cada alvo corresponde a um pacote:
#
#   make install-session  DESTDIR=...   big-wm-session  (sessão X11 e base comum)
#   make install-wayland  DESTDIR=...   big-wm-wayland  (sessão Wayland experimental)
#   make install-themes   DESTDIR=...   big-wm-themes   (temas, ícones e wallpapers)
#   make install-locate-pointer DESTDIR=... big-wm-locate-pointer (sacudir o
#                                       mouse aumenta o ponteiro; forçar o
#                                       fechamento com a caveira, big-wm-xkill
#                                       e o tema de cursor big-wm-skull; C + Xlib)
#   make install-zoom DESTDIR=...       big-wm-zoom (ampliação da tela na sessão
#                                       X11: plugin do picom, C + xcb)
#   make install-lockscreen DESTDIR=... big-wm-lockscreen (tela de bloqueio
#                                       OPENSTEP: fundo e módulos do xsecurelock)
#   make install-grub-theme     DESTDIR=...   grub-theme-big-wm
#   make install-plymouth-theme DESTDIR=...   plymouth-theme-big-wm
#   make install-name           DESTDIR=...   big-wm-name     (nome do sistema)
#   make install-welcome        DESTDIR=...   big-wm-welcome  (boas-vindas, GTK 3)
#   make grub-theme-live DIR=...        tema do GRUB da ISO live (iso-profiles)
#   make check                          testes estáticos (tests/run.sh)
#
# Traduções (gettext, docs/i18n): domínio "big-wm" das ferramentas da sessão
# (po/, fontes em po/POTFILES, .mo no big-wm-session), "big-wm-welcome"
# (welcome/po). O dock com abas tem repositório próprio
# (big-windowmaker/tabbed-dock).
#   make pot                            regenera po/big-wm.pot e o do welcome
#   make update-po                      pot + msgmerge dos catálogos (.po)
#   make mo                             compila em po/build (para testar:
#                                       BIGWM_LOCALEDIR=$PWD/po/build)

PREFIX     ?= /usr
SYSCONFDIR ?= /etc
BINDIR      = $(PREFIX)/bin
LIBDIR      = $(PREFIX)/lib
DATADIR     = $(PREFIX)/share
PKGDATADIR  = $(DATADIR)/big-wm

INSTALL      = install
INSTALL_BIN  = $(INSTALL) -Dm755
INSTALL_DATA = $(INSTALL) -Dm644

SESSION_BIN = startbigwm big-wm-firstrun big-wm-session-ctl big-wm-open big-wm-notifyd big-wm-clock big-wm-mixer big-wm-systemtray \
              big-wm-appmenu big-wm-lock big-wm-wallpaper big-wm-launcher \
              big-wm-screenshot big-wm-kill big-wm-doctor big-wm-theme big-wm-terminal big-wm-dock-reset big-wm-monitors \
              big-wm-lock-settings big-wm-clipboard big-wm-zoom \
              big-wm-hp big-wm-scrcpy big-wm-airplay big-wm-mic-tray
WAYLAND_BIN = startbigwm-wayland big-wm-tray big-wm-dockapps big-wm-dock-settings big-wm-wprefs-sync big-wm-about big-wm-kwin-screenshot

GNUSTEP_DEFAULTS = WindowMaker WMGLOBAL WMRootMenu WMRootMenu.pt_BR WMState WMWindowAttributes

WLMAKER_DATA = Config.plist.in State.plist RootMenu.plist RootMenu.plist.pt_BR Theme.plist Theme-Dark.plist

LOGO       = themes/logo/big-wm.svg
ICON_SIZES = 16 22 24 32 48 64 128 256 512
RSVG       = rsvg-convert

# Ícones do Window Maker redesenhados em SVG (themes/icons): tema XDG
# BigWindowMaker e, gerados aqui, os PNG do Window Maker (64 px, o ladrilho
# inteiro) e do wlmaker (56 px, o conteúdo do ladrilho sem a margem).
ICONS      = $(basename $(notdir $(wildcard themes/icons/svg/*.svg)))
ICON_THEME = $(DATADIR)/icons/BigWindowMaker

MESON             ?= meson

# Localizar o ponteiro ao sacudir o mouse (X11): um arquivo C, Xlib e
# extensões, sem GTK.
LOCATE_POINTER      = session/locate-pointer/big-wm-locate-pointer
LOCATE_POINTER_LIBS = x11 xi xfixes xext xcursor xrandr
PKG_CONFIG         ?= pkg-config
# Forçar o fechamento no X11 (Ctrl+Alt+Esc): como o xkill, com a caveira
# vermelha do tema de cursor big-wm-skull (também usado pelo wlmaker).
XKILL      = session/kill/big-wm-xkill
XKILL_LIBS = x11 xcursor xres

# Ampliação da tela na sessão X11 (Ctrl+Alt+rolagem): plugin do picom, um
# arquivo C com xcb e libev (o laço do picom). Na sessão Wayland quem amplia
# é o wlmaker.
ZOOM_PLUGIN = session/zoom/big-wm-picom-zoom.so
ZOOM_LIBS   = xcb xcb-xinput xcb-randr xcb-keysyms pixman-1

# Tela de bloqueio (X11 e fundo do Wayland): um arquivo C, Xlib + cairo +
# gdk-pixbuf; os módulos do xsecurelock são links para ele.
LOCKSCREEN      = session/lockscreen/big-wm-lockscreen
LOCKSCREEN_LIBS = x11 xi xrandr cairo cairo-xlib gdk-pixbuf-2.0 fontconfig
XSECURELOCK_DIR = $(LIBDIR)/xsecurelock
# Teclado na tela do bloqueio no Wayland: módulo do gtklock.
KEYBOARD_MODULE = session/lockscreen/big-wm-keyboard.so
KEYBOARD_LIBS   = gtk+-3.0 gmodule-2.0

# Temas de boot (themes/boot): gerados no build, sem binários no repositório.
GRUB_THEME_DIR     = /boot/grub/themes/big-wm
PLYMOUTH_THEME_DIR = $(DATADIR)/plymouth/themes/big-wm

.PHONY: pot update-po mo
.PHONY: all install install-session install-wayland install-themes \
	locate-pointer install-locate-pointer zoom install-zoom lockscreen install-lockscreen \
	install-grub-theme install-plymouth-theme install-name grub-theme-live check \
	install-welcome

all:

install: install-session install-wayland install-themes install-grub-theme install-plymouth-theme install-name

locate-pointer: $(LOCATE_POINTER) $(XKILL)

$(XKILL): $(XKILL).c
	$(CC) -std=c11 -D_GNU_SOURCE $(CPPFLAGS) $(CFLAGS) -Wall -Wextra \
		$$($(PKG_CONFIG) --cflags $(XKILL_LIBS)) -o $@ $< \
		$(LDFLAGS) $$($(PKG_CONFIG) --libs $(XKILL_LIBS))

$(LOCATE_POINTER): $(LOCATE_POINTER).c
	$(CC) -std=c11 -D_GNU_SOURCE $(CPPFLAGS) $(CFLAGS) -Wall -Wextra \
		$$($(PKG_CONFIG) --cflags $(LOCATE_POINTER_LIBS)) -o $@ $< \
		$(LDFLAGS) $$($(PKG_CONFIG) --libs $(LOCATE_POINTER_LIBS)) -lm

install-locate-pointer: $(LOCATE_POINTER) $(XKILL)
	$(INSTALL_BIN) $(LOCATE_POINTER) $(DESTDIR)$(BINDIR)/big-wm-locate-pointer
	$(INSTALL_BIN) $(XKILL) $(DESTDIR)$(BINDIR)/big-wm-xkill
	$(INSTALL_DATA) data/icons/big-wm-skull/index.theme $(DESTDIR)$(DATADIR)/icons/big-wm-skull/index.theme
	$(INSTALL_DATA) data/icons/big-wm-skull/cursors/pirate $(DESTDIR)$(DATADIR)/icons/big-wm-skull/cursors/pirate
	$(INSTALL_DATA) session/locate-pointer/big-wm-locate-pointer.desktop \
		$(DESTDIR)$(SYSCONFDIR)/xdg/autostart/big-wm-locate-pointer.desktop
	$(INSTALL_DATA) session/locate-pointer/locate-pointer.conf $(DESTDIR)$(PKGDATADIR)/locate-pointer.conf

zoom: $(ZOOM_PLUGIN)

$(ZOOM_PLUGIN): session/zoom/big-wm-picom-zoom.c
	$(CC) -std=c11 -D_GNU_SOURCE $(CPPFLAGS) $(CFLAGS) -Wall -Wextra -fPIC -shared \
		$$($(PKG_CONFIG) --cflags $(ZOOM_LIBS)) -o $@ $< \
		$(LDFLAGS) $$($(PKG_CONFIG) --libs $(ZOOM_LIBS)) -lev -lm

install-zoom: $(ZOOM_PLUGIN)
	$(INSTALL) -Dm755 $(ZOOM_PLUGIN) $(DESTDIR)$(LIBDIR)/big-wm/big-wm-picom-zoom.so
	$(INSTALL_BIN) session/zoom/big-wm-picom $(DESTDIR)$(LIBDIR)/big-wm/big-wm-picom
	$(INSTALL_DATA) session/zoom/zoom.conf $(DESTDIR)$(PKGDATADIR)/zoom.conf
	# O picom do autostart da sessão passa a carregar o plugin.
	$(INSTALL_DATA) session/zoom/app-picom@autostart.service \
		"$(DESTDIR)$(LIBDIR)/systemd/user/app-picom@autostart.service.d/big-wm-zoom.conf"

lockscreen: $(LOCKSCREEN) $(KEYBOARD_MODULE)

$(KEYBOARD_MODULE): session/lockscreen/gtklock-keyboard.c
	$(CC) -std=gnu11 $(CPPFLAGS) $(CFLAGS) -Wall -Wextra -fPIC -shared \
		$$($(PKG_CONFIG) --cflags $(KEYBOARD_LIBS)) -o $@ $< \
		$(LDFLAGS) $$($(PKG_CONFIG) --libs $(KEYBOARD_LIBS))

$(LOCKSCREEN): $(LOCKSCREEN).c
	$(CC) -std=gnu11 $(CPPFLAGS) $(CFLAGS) -Wall -Wextra \
		$$($(PKG_CONFIG) --cflags $(LOCKSCREEN_LIBS)) -o $@ $< \
		$(LDFLAGS) $$($(PKG_CONFIG) --libs $(LOCKSCREEN_LIBS)) -lm

install-lockscreen: $(LOCKSCREEN) $(KEYBOARD_MODULE)
	$(INSTALL_BIN) $(LOCKSCREEN) $(DESTDIR)$(BINDIR)/big-wm-lockscreen
	$(INSTALL) -Dm755 $(KEYBOARD_MODULE) $(DESTDIR)$(LIBDIR)/gtklock/big-wm-keyboard.so
	$(INSTALL) -d $(DESTDIR)$(XSECURELOCK_DIR)
	# O xsecurelock só aceita módulos que sejam arquivos comuns (recusa
	# links simbólicos e cai no auth_x11): cópias, não links.
	$(INSTALL_BIN) $(LOCKSCREEN) $(DESTDIR)$(XSECURELOCK_DIR)/auth_big_wm
	$(INSTALL_BIN) $(LOCKSCREEN) $(DESTDIR)$(XSECURELOCK_DIR)/saver_big_wm

# --- Traduções ----------------------------------------------------------------

PO_LINGUAS   = $(shell sed -n '/^[a-z]/p' po/LINGUAS)
WELCOME_SRC  = welcome/big-wm-welcome $(wildcard welcome/bigwmwelcome/*.py)
# Saídas do "make pot" (os testes geram em outro lugar e comparam).
POT         ?= po/big-wm.pot
WELCOME_POT ?= welcome/po/big-wm-welcome.pot
XGETTEXT     = xgettext --from-code=UTF-8 --add-comments=TRANSLATORS \
	--msgid-bugs-address=https://github.com/big-windowmaker/big-wm/issues

# Uma chamada do xgettext por linguagem (os scripts não têm extensão): a
# linguagem de cada arquivo do POTFILES vem da extensão ou da primeira linha.
# Arquivo do POTFILES que não existe é erro (nada de POT incompleto calado).
pot:
	@set -e; py=; sh=; c=; \
	for f in $$(grep -v '^#' po/POTFILES); do \
		[ -f "$$f" ] || { echo "po/POTFILES: $$f não existe" >&2; exit 1; }; \
		case $$f in \
		*.c) c="$$c $$f" ;; \
		*) case $$(head -n1 "$$f") in *python*) py="$$py $$f" ;; *) sh="$$sh $$f" ;; esac ;; \
		esac; \
	done; \
	$(XGETTEXT) --package-name=big-wm -o $(POT) -L Python -k_ -kN_ -kngettext:1,2 $$py; \
	$(XGETTEXT) --package-name=big-wm -o $(POT) -j -L Shell -k_t -k_t_layout $$sh; \
	$(XGETTEXT) --package-name=big-wm -o $(POT) -j -L C -k_ -kN_ $$c
	@# O welcome guarda o POT sem posições (e o pt_BR.po sem quebra de linha).
	$(XGETTEXT) --package-name=big-wm-welcome --no-location -o $(WELCOME_POT) \
		-L Python -k_ -kN_ -kngettext:1,2 $(WELCOME_SRC)

update-po: pot
	for l in $(PO_LINGUAS); do \
		msgmerge --quiet --update --backup=none po/$$l.po po/big-wm.pot || exit 1; \
	done
	for po in welcome/po/*.po; do \
		msgmerge --quiet --update --backup=none --no-location --no-wrap "$$po" welcome/po/big-wm-welcome.pot || exit 1; \
	done

mo:
	for l in $(PO_LINGUAS); do \
		$(INSTALL) -d po/build/$$l/LC_MESSAGES; \
		msgfmt --check -o po/build/$$l/LC_MESSAGES/big-wm.mo po/$$l.po || exit 1; \
	done

install-welcome:
	$(INSTALL_BIN) welcome/big-wm-welcome $(DESTDIR)$(BINDIR)/big-wm-welcome
	for f in welcome/bigwmwelcome/*.py; do \
		$(INSTALL_DATA) "$$f" "$(DESTDIR)$(PKGDATADIR)/welcome/bigwmwelcome/$${f##*/}"; \
	done
	$(INSTALL_DATA) welcome/data/big-wm.svg $(DESTDIR)$(PKGDATADIR)/welcome/big-wm.svg
	$(INSTALL_DATA) welcome/data/org.bigwindowmaker.Welcome.desktop \
		$(DESTDIR)$(DATADIR)/applications/org.bigwindowmaker.Welcome.desktop
	$(INSTALL_DATA) welcome/data/big-wm-welcome-autostart.desktop \
		$(DESTDIR)$(SYSCONFDIR)/xdg/autostart/big-wm-welcome.desktop
	for po in welcome/po/*.po; do \
		l=$${po##*/}; l=$${l%.po}; \
		$(INSTALL) -d "$(DESTDIR)$(DATADIR)/locale/$$l/LC_MESSAGES"; \
		msgfmt -c -o "$(DESTDIR)$(DATADIR)/locale/$$l/LC_MESSAGES/big-wm-welcome.mo" "$$po" || exit 1; \
	done

install-session:
	for f in $(SESSION_BIN); do $(INSTALL_BIN) session/bin/$$f $(DESTDIR)$(BINDIR)/$$f; done
	# Traduções do domínio big-wm (ferramentas das duas sessões, do
	# big-wm-locate-pointer e do big-wm-lockscreen).
	for l in $(PO_LINGUAS); do \
		$(INSTALL) -d "$(DESTDIR)$(DATADIR)/locale/$$l/LC_MESSAGES"; \
		msgfmt --check -o "$(DESTDIR)$(DATADIR)/locale/$$l/LC_MESSAGES/big-wm.mo" po/$$l.po || exit 1; \
	done
	$(INSTALL_DATA) session/lib/big-wm-env.sh $(DESTDIR)$(LIBDIR)/big-wm/big-wm-env.sh
	$(INSTALL_DATA) session/lib/big-wm-keyboard.sh $(DESTDIR)$(LIBDIR)/big-wm/big-wm-keyboard.sh
	$(INSTALL_DATA) session/lib/big-wm-theme.sh $(DESTDIR)$(LIBDIR)/big-wm/big-wm-theme.sh
	$(INSTALL_BIN) session/lib/big-wm-wmstyle $(DESTDIR)$(LIBDIR)/big-wm/big-wm-wmstyle
	$(INSTALL_BIN) session/lib/big-wm-autostart-condition $(DESTDIR)$(LIBDIR)/big-wm/big-wm-autostart-condition
	$(INSTALL_BIN) session/lib/big-wm-pamac-tray $(DESTDIR)$(LIBDIR)/big-wm/big-wm-pamac-tray
	$(INSTALL_BIN) session/lib/big-wm-x11-repaint $(DESTDIR)$(LIBDIR)/big-wm/big-wm-x11-repaint
	$(INSTALL_BIN) session/lib/big-wm-dockapps-autolaunch $(DESTDIR)$(LIBDIR)/big-wm/big-wm-dockapps-autolaunch
	$(INSTALL_BIN) session/lib/big-wm-dock-audio $(DESTDIR)$(LIBDIR)/big-wm/big-wm-dock-audio
	$(INSTALL_BIN) session/lib/big-wm-dock-controller $(DESTDIR)$(LIBDIR)/big-wm/big-wm-dock-controller
	$(INSTALL_BIN) session/lib/big-wm-dock-commands $(DESTDIR)$(LIBDIR)/big-wm/big-wm-dock-commands
	$(INSTALL_BIN) session/lib/big-wm-clip-add $(DESTDIR)$(LIBDIR)/big-wm/big-wm-clip-add
	# Aplicativos de terceiros que falhavam calados nas sessões (HPLIP sem
	# PyQt5, scrcpy sem aparelho, UxPlay sem Avahi/firewall): entradas de
	# menu próprias (OnlyShowIn=WindowMaker;wlmaker) que chamam os wrappers;
	# as do pacote ficam ocultas pelos menus das sessões. Ver
	# docs/aplicativos-hp-scrcpy-uxplay.md.
	for f in Scrcpy AirPlay HPDeviceManager HPScan; do \
		$(INSTALL_DATA) data/applications/org.bigwindowmaker.$$f.desktop $(DESTDIR)$(DATADIR)/applications/org.bigwindowmaker.$$f.desktop; \
	done
	# Parte com root do Receptor AirPlay (pkexec, só quando o usuário pede) e
	# o perfil do ufw com as portas fixas do "uxplay -p".
	$(INSTALL_BIN) session/lib/big-wm-airplay-setup $(DESTDIR)$(LIBDIR)/big-wm/big-wm-airplay-setup
	$(INSTALL_DATA) data/ufw/big-wm-uxplay $(DESTDIR)$(SYSCONFDIR)/ufw/applications.d/big-wm-uxplay
	$(INSTALL_BIN) session/lib/big-wm-menu-migrate $(DESTDIR)$(LIBDIR)/big-wm/big-wm-menu-migrate
	# Drop-ins que cortam, só nas sessões Big WindowMaker, autostarts de outros
	# pacotes que lá não servem (ver big-wm-autostart-condition).
	for f in session/systemd/autostart-conditions/*.service; do \
		$(INSTALL_DATA) "$$f" "$(DESTDIR)$(LIBDIR)/systemd/user/$${f##*/}.d/big-wm.conf"; \
	done
	# Drop-ins de unidades de outros pacotes que mudam só o comportamento nas
	# sessões Big WindowMaker (o script chamado confere a sessão).
	for f in session/systemd/user-dropins/*.service; do \
		$(INSTALL_DATA) "$$f" "$(DESTDIR)$(LIBDIR)/systemd/user/$${f##*/}.d/big-wm.conf"; \
	done
	$(INSTALL_DATA) session/systemd/big-wm-session.target $(DESTDIR)$(LIBDIR)/systemd/user/big-wm-session.target
	for f in session/autostart/*.desktop; do $(INSTALL_DATA) $$f $(DESTDIR)$(SYSCONFDIR)/xdg/autostart/$${f##*/}; done
	$(INSTALL_DATA) session/xsessions/big-wm.desktop $(DESTDIR)$(DATADIR)/xsessions/big-wm.desktop
	for f in $(GNUSTEP_DEFAULTS); do $(INSTALL_DATA) data/GNUstep/Defaults/$$f $(DESTDIR)$(PKGDATADIR)/GNUstep/Defaults/$$f; done
	# Tema GTK de nome estável (GTK_THEME=BigWindowMaker, ver big-wm-theme.sh):
	# cada conta tem o link dela em ~/.local/share/themes, que segue o modo;
	# este, do sistema, é o claro, para quem não tem o link (root, instalador).
	$(INSTALL_DATA) data/themes/BigWindowMaker/index.theme $(DESTDIR)$(DATADIR)/themes/BigWindowMaker/index.theme
	for d in gtk-2.0 gtk-3.0 gtk-4.0 img; do ln -sfn ../OneStepBack/$$d $(DESTDIR)$(DATADIR)/themes/BigWindowMaker/$$d; done
	$(INSTALL_DATA) data/dunstrc $(DESTDIR)$(PKGDATADIR)/dunstrc
	# Modo escuro (big-wm-theme dark): o mesmo dunstrc com as cores trocadas.
	sed -f data/dunstrc-dark.sed data/dunstrc >$(DESTDIR)$(PKGDATADIR)/dunstrc-dark
	chmod 644 $(DESTDIR)$(PKGDATADIR)/dunstrc-dark
	$(INSTALL_DATA) data/picom.conf $(DESTDIR)$(PKGDATADIR)/picom.conf
	$(INSTALL_DATA) data/fastfetch/ascii-art $(DESTDIR)$(PKGDATADIR)/fastfetch/ascii-art
	$(INSTALL_DATA) data/fastfetch/config.jsonc $(DESTDIR)$(PKGDATADIR)/fastfetch/config.jsonc
	$(INSTALL_DATA) data/identity/big-wm.conf $(DESTDIR)$(DATADIR)/desktop-identity/big-wm.conf
	$(INSTALL_DATA) data/rofi/big-wm.rasi $(DESTDIR)$(PKGDATADIR)/rofi/big-wm.rasi
	$(INSTALL_DATA) data/rofi/big-wm-dark.rasi $(DESTDIR)$(PKGDATADIR)/rofi/big-wm-dark.rasi
	$(INSTALL_DATA) data/xdg-desktop-portal/windowmaker-portals.conf $(DESTDIR)$(DATADIR)/xdg-desktop-portal/windowmaker-portals.conf
	# Estilo "kvantum" do Qt Quick Controls (QT_STYLE_OVERRIDE=kvantum da sessão)
	$(INSTALL_DATA) data/qt6-qml/kvantum/qmldir $(DESTDIR)$(LIBDIR)/qt6/qml/kvantum/qmldir
	# Aplicativos padrão só das sessões Big WindowMaker (pastas no Thunar):
	# o XDG lê <sessão>-mimeapps.list antes do mimeapps.list do sistema.
	$(INSTALL_DATA) data/xdg/windowmaker-mimeapps.list $(DESTDIR)$(SYSCONFDIR)/xdg/windowmaker-mimeapps.list
	$(INSTALL_DATA) data/xdg/wlmaker-mimeapps.list $(DESTDIR)$(SYSCONFDIR)/xdg/wlmaker-mimeapps.list
	# Ajustes de .desktop de outros pacotes só no menu e no Buscar aplicativos
	# das sessões (big-wm-appmenu): fora do XDG_DATA_DIRS.
	for f in data/desktop-overrides/*.desktop; do \
		$(INSTALL_DATA) "$$f" "$(DESTDIR)$(PKGDATADIR)/desktop-overrides/$${f##*/}"; \
	done
	# Terminal padrão das sessões (AshyTerm) para o xdg-terminal-exec.
	$(INSTALL_DATA) data/xdg/windowmaker-xdg-terminals.list $(DESTDIR)$(SYSCONFDIR)/xdg/windowmaker-xdg-terminals.list
	$(INSTALL_DATA) data/xdg/wlmaker-xdg-terminals.list $(DESTDIR)$(SYSCONFDIR)/xdg/wlmaker-xdg-terminals.list
	# Aparência do xterm (reserva leve), carregada pelo startbigwm.
	$(INSTALL_DATA) data/xresources/XTerm $(DESTDIR)$(PKGDATADIR)/xresources/XTerm
	# Terminal preferido para o Thunar/exo ("Abrir terminal aqui").
	$(INSTALL_DATA) data/xfce4-helpers/big-wm-terminal.desktop $(DESTDIR)$(DATADIR)/xfce4/helpers/big-wm-terminal.desktop

install-wayland:
	for f in $(WAYLAND_BIN); do $(INSTALL_BIN) session/bin/$$f $(DESTDIR)$(BINDIR)/$$f; done
	for f in session/lib/wayland-bin/*; do $(INSTALL_BIN) $$f $(DESTDIR)$(LIBDIR)/big-wm/wayland-bin/$${f##*/}; done
	$(INSTALL_DATA) session/wayland-sessions/big-wm-wayland.desktop $(DESTDIR)$(DATADIR)/wayland-sessions/big-wm-wayland.desktop
	# Ícone do painel de informações (appicon do big-wm-about), fora do menu.
	$(INSTALL_DATA) data/applications/org.communitybig.BigWindowMaker.About.desktop $(DESTDIR)$(DATADIR)/applications/org.communitybig.BigWindowMaker.About.desktop
	for f in session/autostart-wayland/*.desktop; do $(INSTALL_DATA) $$f $(DESTDIR)$(SYSCONFDIR)/xdg/autostart/$${f##*/}; done
	# Vigia do WPrefs sem processo residente (big-wm-wprefs-sync --watch a liga).
	$(INSTALL_DATA) -t $(DESTDIR)$(LIBDIR)/systemd/user session/systemd/big-wm-wprefs-sync.path session/systemd/big-wm-wprefs-sync.service
	for f in $(WLMAKER_DATA); do $(INSTALL_DATA) data/wlmaker/$$f $(DESTDIR)$(PKGDATADIR)/wlmaker/$$f; done
	# Tela de bloqueio do Wayland (gtklock): layout e estilo OPENSTEP.
	$(INSTALL_DATA) -t $(DESTDIR)$(PKGDATADIR)/lock data/lock/gtklock-layout.ui.in data/lock/gtklock.css data/lock/gtklock-dark.css
	$(INSTALL_DATA) data/xdg-desktop-portal/wlmaker-portals.conf $(DESTDIR)$(DATADIR)/xdg-desktop-portal/wlmaker-portals.conf
	$(INSTALL) -d $(DESTDIR)$(DATADIR)/wlmaker/icons
	for i in $(ICONS); do \
		$(RSVG) -w 64 -h 64 --page-width=56 --page-height=56 --left=-4 --top=-4 \
			themes/icons/svg/$$i.svg -o $(DESTDIR)$(DATADIR)/wlmaker/icons/big-wm-$$i-56x56.png; \
	done

install-themes:
	cd themes/WindowMaker && find Themes Styles -type f -exec $(INSTALL) -Dm644 {} $(DESTDIR)$(DATADIR)/WindowMaker/{} \;
	# Tema de ícones BigWindowMaker: cada desenho uma vez em apps/big-wm-*,
	# os nomes padrão (themes/icons/nomes) como links para ele. Os PNG de
	# 64 px vão para os ícones do Window Maker (Clip, Dock, atributos).
	$(INSTALL_DATA) themes/icons/index.theme $(DESTDIR)$(ICON_THEME)/index.theme
	$(INSTALL) -d $(DESTDIR)$(DATADIR)/WindowMaker/Icons
	for i in $(ICONS); do \
		$(INSTALL_DATA) themes/icons/svg/$$i.svg $(DESTDIR)$(ICON_THEME)/scalable/apps/big-wm-$$i.svg; \
		$(RSVG) -w 64 -h 64 themes/icons/svg/$$i.svg -o $(DESTDIR)$(DATADIR)/WindowMaker/Icons/big-wm-$$i.png; \
	done
	grep -v -e '^#' -e '^$$' themes/icons/nomes | while read -r nome icone; do \
		[ -e themes/icons/svg/$$icone.svg ] || { echo "themes/icons/nomes: $$icone.svg não existe" >&2; exit 1; }; \
		$(INSTALL) -d $(DESTDIR)$(ICON_THEME)/scalable/$${nome%/*}; \
		ln -sfn ../apps/big-wm-$$icone.svg $(DESTDIR)$(ICON_THEME)/scalable/$$nome.svg; \
	done
	# Logo: ícone do Dock (56 px cabe no ladrilho de 64 px), tema hicolor e
	# emblema das sessões no slick-greeter (badges/<nome da sessão>.svg; o da
	# sessão Wayland leva um selo "W" para as duas não parecerem iguais).
	$(INSTALL) -d $(DESTDIR)$(DATADIR)/WindowMaker/Icons
	$(RSVG) -w 56 -h 56 $(LOGO) -o $(DESTDIR)$(DATADIR)/WindowMaker/Icons/big-wm-logo.png
	$(INSTALL_DATA) $(LOGO) $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/big-wm.svg
	for s in $(ICON_SIZES); do \
		$(INSTALL) -d $(DESTDIR)$(DATADIR)/icons/hicolor/$${s}x$${s}/apps; \
		$(RSVG) -w $$s -h $$s $(LOGO) -o $(DESTDIR)$(DATADIR)/icons/hicolor/$${s}x$${s}/apps/big-wm.png; \
	done
	$(INSTALL_DATA) $(LOGO) $(DESTDIR)$(DATADIR)/slick-greeter/badges/big-wm.svg
	$(INSTALL_DATA) themes/logo/big-wm-wayland.svg $(DESTDIR)$(DATADIR)/slick-greeter/badges/big-wm-wayland.svg
	for f in themes/backgrounds/*.jpg; do \
		if [ -L $$f ]; then \
			$(INSTALL) -d $(DESTDIR)$(DATADIR)/backgrounds/big-wm; \
			cp -P $$f $(DESTDIR)$(DATADIR)/backgrounds/big-wm/; \
		else \
			$(INSTALL_DATA) $$f $(DESTDIR)$(DATADIR)/backgrounds/big-wm/$${f##*/}; \
		fi; \
	done

# Tema do GRUB no estilo da tela "Window Maker Live". O pacote não o ativa:
# quem escolhe é a edição (/etc/default/grub.d no iso-profiles) ou o usuário
# (ver themes/boot/README.md).
install-grub-theme:
	themes/boot/grub/gerar.sh $(DESTDIR)$(GRUB_THEME_DIR) instalado
	$(INSTALL_BIN) themes/boot/grub/grub-resolucao $(DESTDIR)$(BINDIR)/grub-resolucao
	$(INSTALL_DATA) themes/boot/grub/big-wm-grub-resolucao.service \
		$(DESTDIR)$(LIBDIR)/systemd/system/big-wm-grub-resolucao.service

# Tema do Plymouth e os hooks do pacman que o mantêm quando outro pacote de
# tema tenta trocá-lo (só se o big-wm já era o tema ativo).
install-plymouth-theme:
	themes/boot/plymouth/gerar.sh $(DESTDIR)$(PLYMOUTH_THEME_DIR)
	$(INSTALL_BIN) themes/boot/plymouth/libalpm/plymouth-theme-big-wm \
		$(DESTDIR)$(DATADIR)/libalpm/scripts/plymouth-theme-big-wm
	for h in themes/boot/plymouth/libalpm/*.hook; do \
		$(INSTALL_DATA) $$h $(DESTDIR)$(DATADIR)/libalpm/hooks/$${h##*/}; \
	done

# Nome do sistema (big-wm-name): os-release, lsb-release e /etc/issue com
# "Big WindowMaker", reaplicado pelo hook quando a base os traz de volta.
install-name:
	$(INSTALL_BIN) data/libalpm/big-wm-name $(DESTDIR)$(DATADIR)/libalpm/scripts/big-wm-name
	$(INSTALL_DATA) data/libalpm/big-wm-name.hook $(DESTDIR)$(DATADIR)/libalpm/hooks/big-wm-name.hook

# A ISO live só copia o diretório themes/bigcommunity-live do GRUB: o tema da
# live (título "Big WindowMaker Live", textos traduzidos pelo GRUB) é gerado
# aqui e copiado para bigcommunity/wmaker/live-overlay no iso-profiles.
grub-theme-live:
	@test -n "$(DIR)" || { echo "uso: make grub-theme-live DIR=..." >&2; exit 1; }
	themes/boot/grub/gerar.sh $(DIR) live

check:
	tests/run.sh
