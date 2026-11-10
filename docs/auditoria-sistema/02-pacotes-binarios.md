# 02 — Pacotes, binários e o que roda

## O que foi conferido

`command -v`, `/proc/<pid>/exe` de cada processo da sessão, `pacman -Q` e
`pacman -Qo`, `/usr/local` e `~/.local/bin`.

- Nada em `/usr/local` ou `~/.local/bin` encobre os programas do Big
  WindowMaker; todos vêm de `/usr/bin`.
- Processos com o binário substituído (`/proc/<pid>/exe ... (deleted)`):
  depois de cada instalação, até serem reiniciados. O Window Maker, por
  exemplo, rodava o binário antigo depois de instalar a versão 23 e passou
  ao novo com o reinício (mesmo PID, `SIGUSR1`). O `big-wm-doctor` agora
  mostra isso ("Programas rodando um binário antigo").

## Inconsistência achada: o Window Maker do repositório `packages`

O Window Maker instalado era o pacote **22** (patches 0001–0027), mas o
repositório `packages` só tinha o **14** (0001–0019): as versões 15 a 22 foram
compiladas e instaladas sem ir para o git. Uma ISO feita do repositório
sairia sem as correções de molduras de jogos, do comando do Dock e das
traduções. Corrigido: os patches 0020–0027 foram para o `packages`, iguais
aos do 22 instalado, mais 0028 e 0029 desta auditoria (versão 24).

## Versões no fim da auditoria

| Pacote | Versão |
|---|---|
| windowmaker | 0.96.0.r130.g558e67f-24 |
| wlmaker | 1:0.8.r80.g7481afe-31 |
| big-wm-* | 26.11.10-0721 (branch `fix/auditoria-2026-11-10`) |
| big-wm-tabbed-dock | 1:0.2.1-1 |
