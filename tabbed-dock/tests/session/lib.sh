# shellcheck shell=bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Funções comuns dos testes do Big Tabbed Dock.
results() { echo "${RESULTS:-/work/results.tsv}"; }
record() { printf '%s\t%s\t%s\n' "$1" "$2" "${3:-}" >>"$(results)"; printf '[%s] %s %s\n' "$2" "$1" "${3:-}"; }
check() { local n=$1 d=$2; shift 2; if "$@" >/dev/null 2>&1; then record "$n" PASS "$d"; else record "$n" FAIL "$d"; fi; }
wait_for() { local t=$1; shift; local end=$((SECONDS + t)); while ((SECONDS < end)); do "$@" >/dev/null 2>&1 && return 0; sleep 0.3; done; return 1; }
state() { big-wm-tabbed-dock --print-state 2>/dev/null; }
# centro absoluto de um slot: slot_xy <tipo> <índice>
slot_xy() {
	local s o
	s=$(state)
	o=$(awk '/^origin=/{sub("origin=","");print}' <<<"$s")
	awk -v k="$1" -v i="$2" -v o="$o" 'BEGIN{split(o,a,",")} $1=="slot" && $2==k && $3==i {printf "%d %d\n", a[1]+$4+$6/2, a[2]+$5+$7/2; exit}' <<<"$s"
}
click_slot() { # tipo índice [botão]
	local xy; xy=$(slot_xy "$1" "$2"); [ -n "$xy" ] || return 1
	# shellcheck disable=SC2086
	xdotool mousemove $xy sleep 0.3 click "${3:-1}"; sleep 0.8
}
item_windows() { state | awk -v t="$1" '$1=="item" && $4==t {sub("windows=","",$5); print $5; exit}'; }
active_tab() { state | awk '$1=="tab" && $2 ~ /^\*/ {sub("^\\*","",$2); print $2}'; }
pss_kb() { awk '/^Pss:/{s+=$2} END{print s}' "/proc/$1/smaps_rollup"; }
# Wayland: o layer-shell não informa a posição; ela é calculada (ancorado embaixo, centralizado)
wl_slot_xy() { # tipo índice — origem x medida na tela (o wlmaker centraliza na área útil)
	local s x0; s=$(state); x0=$(wl_dock_x0)
	awk -v k="$1" -v i="$2" -v W="${LAYOUT_W:-1600}" -v H="${LAYOUT_H:-900}" -v X0="${x0:-0}" '
		/^geometry=/{split(substr($1,10),g,"x"); gw=g[1]; gh=g[2]}
		/^geometry=/{for(f=1;f<=NF;f++) if($f ~ /^align=/) al=substr($f,7)}
		$1=="slot" && $2==k && $3==i {sx=$4; sy=$5; sw=$6; sh=$7}
		END{printf "%d %d\n", X0+sx+sw/2, H-gh+sy+sh/2}' <<<"$s"
}
wl_click() { # tipo índice [botão]
	local xy; xy=$(wl_slot_xy "$1" "$2"); [ -n "$xy" ] || return 1
	# shellcheck disable=SC2086
	wlpointer ${LAYOUT_W:-1600} ${LAYOUT_H:-900} move $xy sleep 150 click ${3:-1}; sleep 0.8
}
pixel() { magick "$1" -format "%[hex:u.p{$2,$3}]" info: 2>/dev/null; }
surface_matches() { local s; s=$(state); [ "$(awk -F'[= ]' '/^surface=/{print $2}' <<<"$s")" = "$(awk -F'[= ]' '/^geometry=/{print $2}' <<<"$s")" ]; }
# x real do dock na tela (Wayland): primeiro pixel diferente do fundo na linha H-3
wl_dock_x0() {
	grim /tmp/x0.png 2>/dev/null
	local bg; bg=$(magick /tmp/x0.png -format "%[hex:u.p{1,$((${LAYOUT_H:-900} - 3))}]" info:)
	magick /tmp/x0.png -crop ${LAYOUT_W:-1600}x1+0+$((${LAYOUT_H:-900} - 3)) +repage txt:- |
		awk -v bg="#$bg" 'NR>1 && index($0, bg)==0 {split($1,a,","); print a[1]; exit}'
}
