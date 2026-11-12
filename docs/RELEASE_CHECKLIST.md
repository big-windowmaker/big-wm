# Checklist de publicação — atualização de 11/2026

Os PRs foram abertos, não mesclados. A ordem importa porque os PKGBUILDs
baixam por tag e a ISO baixa artefatos de CI do `main`.

## Ordem

1. **tabbed-dock** — mesclar o PR (0.3.0); criar a tag `v0.3.0` no commit
   mesclado (`git tag -a v0.3.0 -m "0.3.0"`; `git push origin v0.3.0`).
   Conferir o CI do `main` (artefato `tabbed-dock-packages`).
2. **wmake-zones** — mesclar (0.4.0); tag `v0.4.0`; conferir o CI.
3. **packages** — mesclar (wlmaker 34, windowmaker 25, big-dockapps 12,
   wmake-zones 0.4.0-1, wmpulsemixer). Depende da tag `v0.4.0`. O CI compila
   os diretórios alterados; conferir `namcap` e `check()` nos logs.
4. **big-wm** — mesclar (tira a cópia do tabbed-dock; `big-wm-session`
   passa a depender de `big-wm-tabbed-dock>=1:0.2.0`). Só depois do passo 1:
   sem o artefato do tabbed-dock, a ISO ficaria sem o dock.
5. **iso-profiles** e **build-iso** — mesclar; rodar o workflow "Build ISO"
   à mão e conferir o smoke test (BIOS e UEFI, sessão live, instalação pelo
   Calamares, login nas duas sessões).
6. **biglinux-livecd**, **.github** — mesclar (texto).
7. Release de teste no build-iso só com a ISO aprovada no passo 5,
   marcada como pré-release (Alpha).

## Antes de cada merge

- [ ] CI do PR verde.
- [ ] Revisão humana (nenhum PR desta atualização foi mesclado pelo agente).
- [ ] `git diff` sem binários, logs ou dados pessoais (o `packages` ignora
  `*.pkg.tar` e `*.pkg.tar.*`).

## Reversão

### Neste computador

Instalados em 11/11 (registro em
`~/.cache/bwm-backup/2026-11-11-geral/instalados-nesta-missao.txt`, lista
completa anterior em `pacman-Q-antes.txt`, configurações em
`config-wlmaker-bigwm.tar.gz`):

```sh
# wlmaker 34 -> 33
sudo pacman -U ~/.cache/bwm-pkg/wlmaker-33/wlmaker-1:0.8.r80.g7481afe-33-x86_64.pkg.tar
# wmake-zones 0.4.0 -> 0.3.1-6
sudo pacman -U ~/.cache/bwm-pkg/zones/wmake-zones-0.3.1-6-x86_64.pkg.tar
# xorg-server-xephyr (só para testes de vários monitores)
sudo pacman -R xorg-server-xephyr
# configurações (sessão encerrada):
tar -xzf ~/.cache/bwm-backup/2026-11-11-geral/config-wlmaker-bigwm.tar.gz -C ~
```

As mudanças valem a partir do próximo login; nada foi reiniciado.

### No GitHub

Nenhum merge foi feito. Para desfazer um merge: `git revert -m 1 <merge>`
num PR novo (nunca *force push* no `main`). Tags: `git push --delete origin
v0.3.0` só se nenhum pacote publicado depender dela.
