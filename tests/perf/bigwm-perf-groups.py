#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# bigwm-perf-groups.py - custo por função (som, rede, bandeja...) a partir dos
# boots medidos: soma do PSS dos processos de cada função, mediana entre os
# boots. Cada processo cai na primeira regra que casa (ordem da tabela).
#
#   bigwm-perf-groups.py RESULTADOS PERFIL SESSÃO T
import gzip
import json
import os
import re
import statistics
import sys

# (função, regex sobre "comm cmdline")
RULES = [
    ("Servidor gráfico e WM: Xorg", r"^Xorg "),
    ("Servidor gráfico e WM: wmaker", r"^wmaker "),
    ("Servidor gráfico e WM: wlmaker", r"^wlmaker "),
    ("Servidor gráfico e WM: Xwayland", r"^Xwayland "),
    ("Compositor X11 (picom + big-wm-x11-repaint)", r"^picom |big-wm-x11-repaint"),
    ("Papel de parede Wayland (swaybg)", r"^swaybg "),
    ("Dock/Clip Wayland (wlmdock)", r"^wlmdock "),
    ("Dock com abas (big-wm-tabbed-dock)", r"big-wm-tabbed-dock"),
    ("Dockapps X11 (wmclock, wmix, wmcpuload, wmmemload, wmnet)", r"^(wmclock|wmix|wmcpuload|wmmemload|wmnet|wmsystemtray) "),
    ("Dockapps Wayland (wlm*)", r"^wlm(calclock|cpugraph|memgraph|netgraph|mixer|battery|clock)"),
    ("Bandeja X11 de apps modernos (snixembed)", r"snixembed"),
    ("Bandeja Wayland (big-wm-tray)", r"big-wm-tray"),
    ("Sincronia WPrefs→wlmaker (big-wm-wprefs-sync --watch)", r"big-wm-wprefs-sync"),
    ("Som: indicator-sound-switcher", r"indicator-sound"),
    ("Som: PipeWire + WirePlumber + pipewire-pulse", r"^(pipewire|wireplumber|pipewire-pulse|pw-dump) "),
    ("Som: redução de ruído do microfone (biglinux-microphone)", r"biglinux-microphone"),
    ("Rede: NetworkManager + nm-applet + ModemManager + dnsmasq", r"^(NetworkManager|nm-applet|ModemManager|dnsmasq) "),
    ("Notificações (dunst)", r"^dunst "),
    ("Autenticação (polkit-gnome + polkitd)", r"polkit"),
    ("Portais (xdg-desktop-portal*, documentos, permissões)", r"xdg-(desktop|document)-portal|xdg-permission-store"),
    ("Atualizações (pamac-tray, pamac-session, pamac-broker)", r"pamac"),
    ("Bloqueio de tela (xss-lock/swayidle + big-wm-lock + i3lock/swaylock)", r"^(xss-lock|swayidle|i3lock|swaylock) |big-wm-lock"),
    ("Acessibilidade (at-spi)", r"at-spi"),
    ("GVfs", r"gvfs"),
    ("Samba (smbd, nmbd)", r"^(smbd|nmbd|smbd-[a-z]+|samba-[a-z-]+) "),
    ("Impressão (cupsd)", r"^cupsd "),
    ("Discos (udisksd)", r"^udisksd "),
    ("Recursos (uresourced)", r"uresourced"),
    ("Tela de login (wdm)", r"^wdm "),
    ("systemd --user + D-Bus da sessão + sd-pam", r"^(systemd|\(sd-pam\)|dbus-broker|dbus-broker-lau|dbus-daemon) .*(--user|--scope user|sd-pam|\.pid|session)"),
    ("Carregadores de imagem (glycin/bwrap)", r"^(glycin-|bwrap )"),
]


def load(path):
    for p in (path, path + ".gz"):
        if os.path.exists(p):
            with (gzip.open(p, "rt") if p.endswith(".gz") else open(p)) as f:
                return json.load(f)
    return None


def classify(p):
    key = f"{p['comm']} {p['cmd']}"
    for name, rx in RULES:
        if re.search(rx, key):
            return name
    return "Outros (sessão)" if p["group"] == "sessao" else "Outros (sistema: systemd, journald, udevd, logind...)"


def main(argv):
    root, prof, sess, t = argv[1], argv[2], argv[3], argv[4]
    per = {}
    boots = 0
    pdir = os.path.join(root, prof)
    for b in sorted(os.listdir(pdir)):
        if not b.startswith(sess + "-"):
            continue
        s = load(os.path.join(pdir, b, "guest", f"sample-{t}.json"))
        if not s:
            continue
        boots += 1
        acc = {}
        for p in s["processes"]:
            if p.get("kthread") or p["group"] == "ferramenta" or "mem" not in p:
                continue
            k = classify(p)
            e = acc.setdefault(k, [0, 0, 0])
            e[0] += p["mem"]["Pss"]
            e[1] += 1
            e[2] += p.get("cpu_delta_ticks", 0)
        for k, v in acc.items():
            per.setdefault(k, []).append(v)
    hz = 100
    print(f"| Função ({prof}, {sess}, {t} s, {boots} boots) | PSS mediana (MiB) | mín–máx | processos | CPU (ms/s) |")
    print("|---|---:|---:|---:|---:|")
    tot = 0
    for k, v in sorted(per.items(), key=lambda kv: -statistics.median(x[0] for x in kv[1])):
        pss = [x[0] / 1024 for x in v] + [0.0] * (boots - len(v))
        med = statistics.median(pss)
        tot += med
        cpu = statistics.median([x[2] for x in v]) * 1000 / hz / 30
        print(f"| {k} | {med:.1f} | {min(pss):.1f}–{max(pss):.1f} | {statistics.median(x[1] for x in v):.0f} | {cpu:.2f} |")
    print(f"| **Soma das medianas** | **{tot:.0f}** | | | |")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
