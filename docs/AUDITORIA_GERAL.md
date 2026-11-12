# Auditoria geral do Big WindowMaker — 11/11/2026

Inventário da organização [big-windowmaker](https://github.com/big-windowmaker),
do trabalho local ainda não publicado e deste computador; problemas achados
e o que foi feito. Ordem de publicação e reversão em
[RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md).

## Ambiente

Big WindowMaker (base BigLinux/Manjaro), kernel 6.18.55-1-MANJARO x86_64;
sessão ativa **Wayland (wlmaker)**, entrada pelo `wdm`; GPUs AMD Navi 44
(RX 9060 XT) e Cezanne (Vega), driver `amdgpu`; três monitores (HDMI-A-1
3440×1440, DP-1 3440×1440, DP-2 2560×1080, escala 1). Pacotes do Big
WindowMaker instalados de builds locais ("Unknown Packager": o `PACKAGER`
do `makepkg.conf` não está definido).

## Repositórios

| Repositório | Papel | Padrão no GitHub | Trabalho local não publicado (11/11) |
|---|---|---|---|
| big-wm | sessões, configuração, temas, ferramentas, testes | `main` (07/11) | ~95 commits (missões de 08 a 11/11: tabbed-dock em repositório próprio, i18n, áudio, monitores, tema/Dock no Wayland, área de transferência/Spectacle, menus) → `chore/atualizacao-geral-2026-11` |
| tabbed-dock | dock com abas e o controlador | `main` (10/11) | 0.2.1 e o redesenho do controlador → PR #1 (0.3.0) |
| wmake-zones | zonas e mosaico | `main` (11/11, PR #9) | pt_BR (5 commits) → 0.4.0 |
| packages | PKGBUILDs e patches | `main` (07/11: wlmaker 23, windowmaker 14) | wlmaker 31-34, windowmaker 24-25, big-dockapps 10-12, wmpulsemixer → `integ/atualizacao-geral-2026-11` |
| build-iso | ISO, testes em VM, releases | `main` (06/11) | artefato do tabbed-dock, wmpulsemixer |
| iso-profiles (fork) | perfil `bigcommunity/wmaker` | `main` (07/11) | lista de aplicativos, wmpulsemixer |
| biglinux-livecd (fork) | sessão live, Calamares | `main` (07/11) | só o e-mail |
| .github | perfil da organização | `main` (09/11) | — |
| wmaker (fork) | Window Maker | `master` = upstream | os patches ficam no `packages` (30) |
| big-wm-display-resolution | monitores (X11 e Wayland) | **não existia no GitHub** até 11/11 (só local); criado em 11/11 | hotplug, pt_BR e rolagem segura em ramos divergentes → PR #1 |
| wlmaker (fork) | compositor | `main` = upstream (8 atrás) | os patches ficam no `packages` (138 + libbase) |

Os forks do Window Maker e do wlmaker não são a fonte dos pacotes: os
PKGBUILDs baixam o upstream num commit fixo e aplicam os patches do
`packages`. Os ramos `big-wm/*` dos forks guardam a origem dos patches.

## Problemas achados

| # | Problema | Situação |
|---|---|---|
| 1 | Grande parte do trabalho de 08 a 11/11 só existia em ramos locais (dezenas de worktrees); o GitHub e a ISO estavam dias atrás do que está instalado | Consolidado em um ramo por repositório e aberto em PR (não mesclado) |
| 2 | `big-wm-tabbed-dock` com duas origens no GitHub (cópia antiga em `big-wm/tabbed-dock/`, versão por data, e o repositório próprio, `1:0.2.x`); a ISO usava a cópia | O ramo do big-wm tira a cópia; o build-iso passa a pegar o artefato do tabbed-dock |
| 3 | tabbed-dock e wmake-zones: o PKGBUILD pede `#tag=v$pkgver`, mas não há tag do tabbed-dock e a do wmake-zones está em 0.3.1 com 13 commits depois | Versões 0.3.0 e 0.4.0 preparadas; as tags entram depois do merge (checklist) |
| 4 | Metainfo do wmake-zones parado em 0.2.0 | 0.3.0, 0.3.1 e 0.4.0 e o contato; `appstreamcli validate` ok |
| 5 | `.SRCINFO` do wlmaker (pkgrel 23 contra 33) e do windowmaker (14 contra 25) desatualizados nos ramos locais | Regenerados; os 11 conferidos com `makepkg --printsrcinfo` |
| 6 | big-dockapps: dois ramos subiram o pkgrel para 10 com conteúdos diferentes | pkgrel 12 com os dois |
| 7 | namcap: `glib2` (wlmaker) e `libxinerama` (wmake-zones) ligados e não declarados | Declarados |
| 8 | E-mail `rruscher@gmail.com.br` (errado) em 9 arquivos do GitHub | `rruscher@gmail.com` em todos os ramos |
| 9 | "Sobre" do tabbed-dock sem o Barnabé, dock sem "Sobre", "Sobre" do wmake-zones sem autores | Janelas com versão, mantenedores, agradecimentos, licença e repositório (capturas no tabbed-dock) |
| 10 | Tabela de versões do `packages/README.md` desatualizada e incompleta | Atualizada |
| 11 | `EXPECTED_PACKAGES` da ISO sem `big-wm-name` e `wmeyes` (estão no perfil) | Incluídos |
| 12 | `.gitignore` do `packages` não pegava o `.pkg.tar` sem compressão do makepkg local | Corrigido |
| 13 | `big-wm-display-resolution` só existia neste computador, mas o `big-wm-session` depende dele; o pacote instalado (26.11.10-1347) não tinha a tradução nem a rolagem segura, que estavam noutro ramo | Repositório criado (autorizado), ramos juntados, Sobre, CI com artefato; build-iso e integração do big-wm passam a buscá-lo |
| 14 | CI do big-wm: clone raso (os testes do firstrun usam o histórico), sem o locale pt_BR, sem `libxres` nas makedepends; integração só com artefatos do `packages` | Corrigido no PR #14 (`test-and-build` verde) |
| 15 | Menus do XWayland longe do ponteiro (GParted) e popups do Wayland cortados | wlmaker 34 (0147), [wayland-popup-positioning-audit.md](wayland-popup-positioning-audit.md) |

## Pendências e decisões dos mantenedores

- `big-wm/CREDITS.md`: 6 itens de arte com licença "a confirmar".
- biglinux-livecd: o "Sobre" do Calamares diz "1.0.0" e "© 2024 BigLinux
  Team" (arquivo do upstream BigLinux, mantido); o README aponta para um
  `LIVE-STATE.md` que não existe (também no upstream).
- iso-profiles e .github sem arquivo de licença.
- Ramos remotos antigos (big-wm 31, packages 23, iso-profiles 14): limpar
  depois dos merges.
- Workflows herdados sem execução: wlmaker (7, do upstream) e
  biglinux-livecd (`build-package.yml` só faz checkout).
- `PACKAGER` não definido no `makepkg.conf` desta máquina.
- Não verificados nesta rodada: ISO nova em VM, sessão real depois do
  próximo login (wlmaker 34, wmake-zones 0.4.0), GParted real, escala
  fracionária.
