# Propostas para fora do Big WindowMaker

Enviados em 2026-11-15: BigLinux
[biglinux-driver-manager#52](https://github.com/biglinux/biglinux-driver-manager/issues/52)
e [auto-unlock-pacman#3](https://github.com/biglinux/auto-unlock-pacman/issues/3); picom [yshui/picom#1558](https://github.com/yshui/picom/issues/1558)
(com o que não reproduz sob demanda: só logo depois da partida do picom).

## picom (yshui/picom): renderiza a cada vblank depois de RRSetOutputPrimary

> **picom 13 (xrender, vsync) keeps rendering every vblank after the RandR
> primary output changes**
>
> Setup: picom 13 (Arch `picom 13-2`), `backend = "xrender"`, `vsync = true`,
> `use-damage = true`, Xorg 21.1.24 with xf86-video-intel (SNA) on an Intel
> G45, one monitor (VGA, 1920x1080@60).
>
> Steps: start picom; then run `xrandr --output VGA1 --primary` (the
> primary was unset). In our session this happens at login, about 0.5 s
> after picom starts, and it reproduced in 5 of 5 logins.
>
> Result: with the screen idle and no damage, picom renders a frame on
> every vblank (`--log-level=verbose`: ~60 "Render start" per second, the
> windows "will be painted" each frame), using ~6 % of a Core 2 Duo core
> plus ~2 % in Xorg. Each frame ends with a `PresentCompleteNotify`, and
> `ev_handle()` queues a redraw for every event, so the loop feeds itself.
> Sending SIGUSR1 (session reset) stops it. Setting `fading = false` does not.
>
> Expected: no rendering when nothing is damaged.

## BigLinux: `big-driver-manager-cli gpu boot`

> `big-driver-manager-gpu.service` runs before the display manager. `_boot()`
> calls `gpu.installed_packages()` (`pacman -Qq`) before checking the
> hardware. On a hard disk, cold, this held the display manager until ~20 s
> after boot (Dell OptiPlex 760); warm, it takes 0.35 s. Without an NVIDIA
> card and outside the live media, the function changes nothing. Proposal:
> scan the PCI devices first and return early when no NVIDIA display device
> is present and the live marker is absent. Measured with that condition as
> a drop-in: wdm started 6.2 s earlier (3 boots each).

## BigLinux: `pacman-fix-keys` a cada boot

> `auto-unlock-pacman.service` runs `pacman-fix-keys` (`pacman-key --init`
> and key population) on every boot, ~10 s on a hard disk, in parallel
> with the display manager and the first login. Proposal: run it only
> when the keyring needs it (for example, a stamp of the keyring package
> version), or at least with `Nice=19` and `IOSchedulingClass=idle`.
