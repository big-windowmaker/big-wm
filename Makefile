# SPDX-License-Identifier: GPL-2.0-or-later
#
# Instalação do Big WindowMaker. Cada alvo corresponde a um pacote:
#
#   make install-session  DESTDIR=...   big-wm-session  (sessão X11 e base comum)
#   make install-wayland  DESTDIR=...   big-wm-wayland  (sessão Wayland experimental)
#   make install-themes   DESTDIR=...   big-wm-themes   (temas, ícones e wallpapers)
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
              big-wm-appmenu big-wm-lock big-wm-wallpaper
WAYLAND_BIN = startbigwm-wayland big-wm-screenshot big-wm-tray big-wm-dockapps

GNUSTEP_DEFAULTS = WindowMaker WMGLOBAL WMRootMenu WMRootMenu.pt_BR WMState WMWindowAttributes

WLMAKER_DATA = Config.plist.in State.plist RootMenu.plist RootMenu.plist.pt_BR Theme.plist

LOGO       = themes/logo/big-wm.svg
ICON_SIZES = 16 22 24 32 48 64 128 256 512
RSVG       = rsvg-convert

.PHONY: all install install-session install-wayland install-themes check

all:

install: install-session install-wayland install-themes

install-session:
	for f in $(SESSION_BIN); do $(INSTALL_BIN) session/bin/$$f $(DESTDIR)$(BINDIR)/$$f; done
	$(INSTALL_DATA) session/lib/big-wm-env.sh $(DESTDIR)$(LIBDIR)/big-wm/big-wm-env.sh
	$(INSTALL_DATA) session/lib/big-wm-keyboard.sh $(DESTDIR)$(LIBDIR)/big-wm/big-wm-keyboard.sh
	$(INSTALL_BIN) session/lib/big-wm-autostart-condition $(DESTDIR)$(LIBDIR)/big-wm/big-wm-autostart-condition
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
	$(INSTALL_DATA) data/picom.conf $(DESTDIR)$(PKGDATADIR)/picom.conf
	$(INSTALL_DATA) data/rofi/big-wm.rasi $(DESTDIR)$(PKGDATADIR)/rofi/big-wm.rasi
	$(INSTALL_DATA) data/xdg-desktop-portal/windowmaker-portals.conf $(DESTDIR)$(DATADIR)/xdg-desktop-portal/windowmaker-portals.conf

install-wayland:
	for f in $(WAYLAND_BIN); do $(INSTALL_BIN) session/bin/$$f $(DESTDIR)$(BINDIR)/$$f; done
	$(INSTALL_DATA) session/wayland-sessions/big-wm-wayland.desktop $(DESTDIR)$(DATADIR)/wayland-sessions/big-wm-wayland.desktop
	for f in session/autostart-wayland/*.desktop; do $(INSTALL_DATA) $$f $(DESTDIR)$(SYSCONFDIR)/xdg/autostart/$${f##*/}; done
	for f in $(WLMAKER_DATA); do $(INSTALL_DATA) data/wlmaker/$$f $(DESTDIR)$(PKGDATADIR)/wlmaker/$$f; done
	$(INSTALL_DATA) data/xdg-desktop-portal/wlmaker-portals.conf $(DESTDIR)$(DATADIR)/xdg-desktop-portal/wlmaker-portals.conf
	for f in themes/wlmaker-icons/*.png; do $(INSTALL_DATA) $$f $(DESTDIR)$(DATADIR)/wlmaker/icons/$${f##*/}; done

install-themes:
	cd themes/WindowMaker && find Themes Styles Icons -type f -exec $(INSTALL) -Dm644 {} $(DESTDIR)$(DATADIR)/WindowMaker/{} \;
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

check:
	tests/run.sh
