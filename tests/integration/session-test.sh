#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Teste de integração das sessões Big WindowMaker com os pacotes reais.
#
# Roda como root dentro de um contêiner Manjaro DESCARTÁVEL:
#   session-test.sh <dir-com-pacotes> <dir-de-saída>
#
# <dir-com-pacotes> deve ter os .pkg.tar.zst de big-wm-* e dos pacotes de
# big-windowmaker/packages (windowmaker, wlmaker, dockapps). Instala tudo,
# cria o usuário "tester" e exercita a sessão X11 (Xvfb + wmaker) e a sessão
# Wayland (wlmaker no backend headless). Grava results.tsv, logs e capturas
# de tela em <dir-de-saída>; sai com erro se algum teste falhar.
#
# Os repositórios BigLinux/BigCommunity entram sem verificação de assinatura
# porque este contêiner não tem os chaveiros deles e é destruído no fim.
set -u

pkgs=${1:?uso: session-test.sh <dir-com-pacotes> <dir-de-saída>}
out=${2:?uso: session-test.sh <dir-com-pacotes> <dir-de-saída>}
mkdir -p "$out"
chmod 777 "$out"
results=$out/results.tsv
printf 'teste\tresultado\tdetalhe\n' >"$results"
chmod 666 "$results"

cat >>/etc/pacman.conf <<'EOF'

# Somente contêiner de teste descartável.
[community-stable]
SigLevel = Optional TrustAll
Server = https://repo.communitybig.org/$repo/$arch
[community-extra]
SigLevel = Optional TrustAll
Server = https://repo.communitybig.org/$repo/$arch
[biglinux-stable]
SigLevel = Optional TrustAll
Server = https://packages.biglinux.com.br/$repo/$arch
EOF

echo "== instalação dos pacotes"
pacman -Syu --noconfirm >"$out/pacman-upgrade.log" 2>&1
if ! pacman -U --noconfirm "$pkgs"/*.pkg.tar.zst >"$out/pacman-install.log" 2>&1; then
	printf 'instalação dos pacotes\tFAIL\tver pacman-install.log\n' >>"$results"
	tail -20 "$out/pacman-install.log"
	exit 1
fi
printf 'instalação dos pacotes\tPASS\t%s pacotes\n' "$(find "$pkgs" -maxdepth 1 -name '*.pkg.tar.zst' | wc -l)" >>"$results"

# Ferramentas usadas só pelos testes.
pacman -S --noconfirm --needed xorg-server-xvfb xorg-xwininfo xorg-xprop xdotool \
	imagemagick xterm xclip picom xorg-xeyes noto-fonts wl-clipboard wlr-randr wayland-utils \
	gnome-calculator featherpad thunar ashyterm grim xdg-desktop-portal pipewire wireplumber >"$out/pacman-tools.log" 2>&1

# Locales usados pelos testes, como num sistema instalado.
sed -i -E 's/^#(pt_BR|en_US|es_ES)\.UTF-8/\1.UTF-8/' /etc/locale.gen
locale-gen >/dev/null

id tester >/dev/null 2>&1 || useradd -m tester
install -m 755 "$(dirname "$0")/lib.sh" /home/tester/lib.sh
install -m 755 "$(dirname "$0")/x11.sh" /home/tester/x11.sh
install -m 755 "$(dirname "$0")/wayland.sh" /home/tester/wayland.sh

echo "== sessão X11"
su tester -c "OUT='$out' dbus-run-session -- bash /home/tester/x11.sh"
echo "== sessão Wayland"
su tester -c "OUT='$out' dbus-run-session -- bash /home/tester/wayland.sh"

echo
column -t -s $'\t' "$results"
if grep -q $'\tFAIL\t' "$results"; then
	echo "Há testes reprovados." >&2
	exit 1
fi
echo "Nenhum teste reprovado."
