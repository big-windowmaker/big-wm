# SPDX-License-Identifier: GPL-2.0-or-later
#
# Layout de teclado do sistema para as sessões do Big WindowMaker.
# Carregado com "."; define XKB_LAYOUT, XKB_MODEL, XKB_VARIANT e XKB_OPTIONS.
#
# Ordem: configuração X11 do systemd-localed (localectl); se ela não existir
# (comum no BigLinux: o Plasma guarda o layout por conta própria e o
# localectl mostra "(unset)"), o teclado do console (/etc/vconsole.conf)
# traduzido pela tabela do systemd (kbd-model-map); por fim, "us".
# BIGWM_VCONSOLE e BIGWM_KBD_MAP trocam os arquivos lidos (testes).

big_wm_keyboard() {
	local status keymap line
	XKB_LAYOUT='' XKB_MODEL='' XKB_VARIANT='' XKB_OPTIONS=''

	status=$(LC_ALL=C localectl status 2>/dev/null)
	XKB_LAYOUT=$(sed -n 's/^ *X11 Layout: //p' <<<"$status")
	XKB_MODEL=$(sed -n 's/^ *X11 Model: //p' <<<"$status")
	XKB_VARIANT=$(sed -n 's/^ *X11 Variant: //p' <<<"$status")
	XKB_OPTIONS=$(sed -n 's/^ *X11 Options: //p' <<<"$status")
	[ "$XKB_LAYOUT" = "(unset)" ] && XKB_LAYOUT=

	if [ -z "$XKB_LAYOUT" ]; then
		keymap=$(sed -n 's/^KEYMAP=//p' "${BIGWM_VCONSOLE:-/etc/vconsole.conf}" 2>/dev/null | tr -d '"')
		local map=${BIGWM_KBD_MAP:-/usr/share/systemd/kbd-model-map}
		if [ -n "$keymap" ] && [ -r "$map" ]; then
			line=$(awk -v k="$keymap" '$1 == k {print $2, $3, $4, $5; exit}' "$map")
			if [ -n "$line" ]; then
				read -r XKB_LAYOUT XKB_MODEL XKB_VARIANT XKB_OPTIONS <<<"$line"
			fi
		fi
	fi

	# "-" é o "vazio" da kbd-model-map.
	[ "$XKB_MODEL" = - ] && XKB_MODEL=
	[ "$XKB_VARIANT" = - ] && XKB_VARIANT=
	[ "$XKB_OPTIONS" = - ] && XKB_OPTIONS=
	XKB_LAYOUT=${XKB_LAYOUT:-us}
	XKB_MODEL=${XKB_MODEL:-pc105}
}
