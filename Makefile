# SPDX-License-Identifier: GPL-2.0-or-later
#
# Instalação do Big WindowMaker. Cada alvo corresponde a um pacote:
#
#   make install-session  DESTDIR=...   big-wm-session  (sessão X11 e base comum)
#   make install-wayland  DESTDIR=...   big-wm-wayland  (sessão Wayland experimental)
#   make install-themes   DESTDIR=...   big-wm-themes   (temas, ícones e wallpapers)
#   make install-tabbed-dock DESTDIR=... big-wm-tabbed-dock (dock com abas, C + GTK 3)
#   make install-grub-theme     DESTDIR=...   grub-theme-big-wm
#   make install-plymouth-theme DESTDIR=...   plymouth-theme-big-wm
#   make grub-theme-live DIR=...        tema do GRUB da ISO live (iso-profiles)
#   make check                          testes estáticos (tests/run.sh)

PREFIX     ?= /usr
SYSCONFDIR ?= /etc
BINDIR      = $(PREFIX)/bin
LIBDIR      = $(PREFIX)/lib
DATADIR     = $(PREFIX)/share
PKGDATADIR  = $(DATADIR)/big-wm

INSTALL      = install
INSTALL_BIN  = $(INSTALL) -Dm755
INSTALL_DATA = $(INSTALL) -Dm644

SESSION_BIN = startbigwm big-wm-firstrun big-wm-session-ctl big-wm-open big-wm-notifyd big-wm-clock big-wm-mixer \
              big-wm-appmenu big-wm-lock big-wm-wallpaper big-wm-launcher \
              big-wm-screenshot big-wm-kill big-wm-theme
WAYLAND_BIN = startbigwm-wayland big-wm-tray big-wm-dockapps

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

# Dock com abas: compilado com meson (ver tabbed-dock/meson.build).
TABBED_DOCK_BUILD ?= tabbed-dock/build
MESON             ?= meson

# Temas de boot (themes/boot): gerados no build, sem binários no repositório.
GRUB_THEME_DIR     = /boot/grub/themes/big-wm
PLYMOUTH_THEME_DIR = $(DATADIR)/plymouth/themes/big-wm

.PHONY: all install install-session install-wayland install-themes install-tabbed-dock tabbed-dock \
	install-grub-theme install-plymouth-theme grub-theme-live check

all:

install: install-session install-wayland install-themes install-grub-theme install-plymouth-theme

install-session:
	for f in $(SESSION_BIN); do $(INSTALL_BIN) session/bin/$$f $(DESTDIR)$(BINDIR)/$$f; done
	$(INSTALL_DATA) session/lib/big-wm-env.sh $(DESTDIR)$(LIBDIR)/big-wm/big-wm-env.sh
	$(INSTALL_DATA) session/lib/big-wm-keyboard.sh $(DESTDIR)$(LIBDIR)/big-wm/big-wm-keyboard.sh
	$(INSTALL_DATA) session/lib/big-wm-theme.sh $(DESTDIR)$(LIBDIR)/big-wm/big-wm-theme.sh
	$(INSTALL_BIN) session/lib/big-wm-wmstyle $(DESTDIR)$(LIBDIR)/big-wm/big-wm-wmstyle
	$(INSTALL_BIN) session/lib/big-wm-autostart-condition $(DESTDIR)$(LIBDIR)/big-wm/big-wm-autostart-condition
	$(INSTALL_BIN) session/lib/big-wm-x11-repaint $(DESTDIR)$(LIBDIR)/big-wm/big-wm-x11-repaint
	$(INSTALL_BIN) session/lib/big-wm-dockapps-autolaunch $(DESTDIR)$(LIBDIR)/big-wm/big-wm-dockapps-autolaunch
	$(INSTALL_BIN) session/lib/big-wm-clip-add $(DESTDIR)$(LIBDIR)/big-wm/big-wm-clip-add
	# Drop-ins que cortam, só nas sessões Big WindowMaker, autostarts de outros
	# pacotes que lá não servem (ver big-wm-autostart-condition).
	for f in session/systemd/autostart-conditions/*.service; do \
		$(INSTALL_DATA) "$$f" "$(DESTDIR)$(LIBDIR)/systemd/user/$${f##*/}.d/big-wm.conf"; \
	done
	$(INSTALL_DATA) session/systemd/big-wm-session.target $(DESTDIR)$(LIBDIR)/systemd/user/big-wm-session.target
	for f in session/autostart/*.desktop; do $(INSTALL_DATA) $$f $(DESTDIR)$(SYSCONFDIR)/xdg/autostart/$${f##*/}; done
	$(INSTALL_DATA) session/xsessions/big-wm.desktop $(DESTDIR)$(DATADIR)/xsessions/big-wm.desktop
	for f in $(GNUSTEP_DEFAULTS); do $(INSTALL_DATA) data/GNUstep/Defaults/$$f $(DESTDIR)$(PKGDATADIR)/GNUstep/Defaults/$$f; done
	$(INSTALL_DATA) data/dunstrc $(DESTDIR)$(PKGDATADIR)/dunstrc
	# Modo escuro (big-wm-theme dark): o mesmo dunstrc com as cores trocadas.
	sed -f data/dunstrc-dark.sed data/dunstrc >$(DESTDIR)$(PKGDATADIR)/dunstrc-dark
	chmod 644 $(DESTDIR)$(PKGDATADIR)/dunstrc-dark
	$(INSTALL_DATA) data/picom.conf $(DESTDIR)$(PKGDATADIR)/picom.conf
	$(INSTALL_DATA) data/fastfetch/ascii-art $(DESTDIR)$(PKGDATADIR)/fastfetch/ascii-art
	$(INSTALL_DATA) data/fastfetch/config.jsonc $(DESTDIR)$(PKGDATADIR)/fastfetch/config.jsonc
	$(INSTALL_DATA) data/rofi/big-wm.rasi $(DESTDIR)$(PKGDATADIR)/rofi/big-wm.rasi
	$(INSTALL_DATA) data/rofi/big-wm-dark.rasi $(DESTDIR)$(PKGDATADIR)/rofi/big-wm-dark.rasi
	$(INSTALL_DATA) data/xdg-desktop-portal/windowmaker-portals.conf $(DESTDIR)$(DATADIR)/xdg-desktop-portal/windowmaker-portals.conf

install-wayland:
	for f in $(WAYLAND_BIN); do $(INSTALL_BIN) session/bin/$$f $(DESTDIR)$(BINDIR)/$$f; done
	for f in session/lib/wayland-bin/*; do $(INSTALL_BIN) $$f $(DESTDIR)$(LIBDIR)/big-wm/wayland-bin/$${f##*/}; done
	$(INSTALL_DATA) session/wayland-sessions/big-wm-wayland.desktop $(DESTDIR)$(DATADIR)/wayland-sessions/big-wm-wayland.desktop
	for f in session/autostart-wayland/*.desktop; do $(INSTALL_DATA) $$f $(DESTDIR)$(SYSCONFDIR)/xdg/autostart/$${f##*/}; done
	for f in $(WLMAKER_DATA); do $(INSTALL_DATA) data/wlmaker/$$f $(DESTDIR)$(PKGDATADIR)/wlmaker/$$f; done
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

tabbed-dock:
	[ -d $(TABBED_DOCK_BUILD) ] || $(MESON) setup --prefix=$(PREFIX) --sysconfdir=$(SYSCONFDIR) \
		--buildtype=release $(TABBED_DOCK_BUILD) tabbed-dock
	$(MESON) compile -C $(TABBED_DOCK_BUILD)

install-tabbed-dock: tabbed-dock
	$(MESON) install -C $(TABBED_DOCK_BUILD) --destdir "$(DESTDIR)" --no-rebuild

# Tema do GRUB no estilo da tela "Window Maker Live". O pacote não o ativa:
# quem escolhe é a edição (/etc/default/grub.d no iso-profiles) ou o usuário
# (ver themes/boot/README.md).
install-grub-theme:
	themes/boot/grub/gerar.sh $(DESTDIR)$(GRUB_THEME_DIR) instalado

# Tema do Plymouth e os hooks do pacman que o mantêm quando outro pacote de
# tema tenta trocá-lo (só se o big-wm já era o tema ativo).
install-plymouth-theme:
	themes/boot/plymouth/gerar.sh $(DESTDIR)$(PLYMOUTH_THEME_DIR)
	$(INSTALL_BIN) themes/boot/plymouth/libalpm/plymouth-theme-big-wm \
		$(DESTDIR)$(DATADIR)/libalpm/scripts/plymouth-theme-big-wm
	for h in themes/boot/plymouth/libalpm/*.hook; do \
		$(INSTALL_DATA) $$h $(DESTDIR)$(DATADIR)/libalpm/hooks/$${h##*/}; \
	done

# A ISO live só copia o diretório themes/bigcommunity-live do GRUB: o tema da
# live (título "Big WindowMaker Live", textos traduzidos pelo GRUB) é gerado
# aqui e copiado para bigcommunity/wmaker/live-overlay no iso-profiles.
grub-theme-live:
	@test -n "$(DIR)" || { echo "uso: make grub-theme-live DIR=..." >&2; exit 1; }
	themes/boot/grub/gerar.sh $(DIR) live

check:
	tests/run.sh
