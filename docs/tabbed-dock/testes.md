# Testes, resultados e medições

**Onde rodou**: só em contêineres podman nesta máquina de desenvolvimento.
A VM de testes do projeto (`bigwm`) **não** foi usada: as instruções desta
tarefa proíbem acessá-la. Nada aqui vem de hardware real nem de uma sessão
com login pelo greeter; o que depende disso está marcado como pendente.

| Camada | Como | Resultado |
|---|---|---|
| Unitários do núcleo (GLib) | `meson test` — 5 programas, 25 casos: modelo, configuração (ida e volta, valores inválidos, recuperação de corrompido, .bak), índice .desktop e associação de janelas, instalador do live primeiro na aba Geral (marcador do live falso, uma vez por boot), coexistência (com `wdwrite` falso), lançamento (Exec com `%F`, nome de arquivo com `;$()'"`: nada executado por shell) | 5/5 ✅; também sob AddressSanitizer + LeakSanitizer + UBSan ✅ |
| Estáticos do repositório | `tests/run.sh` (shellcheck, plists, .desktop, caminhos instalados, firstrun…) + passo novo do dock (build, testes, caminhos, tradução, proibição de `system`/`popen`/`sh -c`) | todos ✅ |
| Pacote | `makepkg` do branch num contêiner `manjarolinux/base` limpo, como o CI | ✅ 4 pacotes (ver abaixo) |
| Integração do CI | `tests/integration/session-test.sh` no contêiner Manjaro fixado pelo CI, com os pacotes deste branch e os artefatos de big-windowmaker/packages (wlmaker pkgrel 7, que já traz 0042–0044) | 86 ✅, 0 ❌ ([resultados-integracao.tsv](resultados-integracao.tsv)) |
| Sessão X11 | Xvfb + Window Maker 0.96 + configuração do Big WindowMaker; mouse e teclado por `xdotool` | 43 ✅ |
| Sessão Wayland | wlmaker `big-wm/protocols` `f11a08a` **sem** patches, headless (pixman); ponteiro por `zwlr_virtual_pointer_v1` (`tests/tools/wlpointer.c`), teclado por `wtype` | 34 ✅, 1 ❌ (regressão do wlmaker, corrigida pelo 0044), 2 pendentes |
| Wayland com 0042–0044 (= wlmaker empacotado hoje) | mesmo, wlmaker com os patches | 28 ✅ |
| Memória (ASan) em uso real | dock compilado com `-Db_sanitize=address,undefined`, trocas de aba, lançamento, menu, arrasto, recarregar, preferências, sair (X11 e Wayland) | nenhum erro nem vazamento do código do dock (só alocações suprimidas de fontconfig/glib) |

Resultados brutos das sessões: [resultados-sessoes.tsv](resultados-sessoes.tsv)
(114 linhas: 105 PASS, 1 FAIL, 2 SKIP, 6 INFO).

**Falha do CI corrigida (PR #3, run 40449635574)**: `wayland: dock com abas
troca de aba (--tab)`. Reproduzida com o mesmo fluxo do CI: o `x11.sh` e o
`wayland.sh` usam a mesma conta, o X11 deixava a aba Desenvolvimento ativa e
o dock, corretamente, a lembrava no Wayland; o `--tab next` levava então a
Documentos. O teste agora verifica essa memória entre sessões e troca a
partir de uma aba conhecida (`--tab 1`, `--tab next`). Nenhuma mudança no
dock foi necessária.

## Como reproduzir

```sh
cd tabbed-dock/tests/session
podman build -t localhost/tabbed-dock-img -f Containerfile .   # base: localhost/wlmaker-dev-img
# wlmaker: DESTDIR de um build do fork em ~/.cache/tabbed-dock/<nome>
WLINST=wlmaker-head-plain-inst WLINST_WLR=wlmaker-head-wlr-inst ./run-all.sh
./run-in.sh x11-env.sh measure.sh x11          # medições
./run-in.sh wl-env.sh  measure.sh wayland
PODMAN_ARGS="-e DOCK_BUILD=asan" ./run-in.sh x11-env.sh x11-asan.sh
```

O laboratório fica em `~/.cache/tabbed-dock` (o `/tmp` desta máquina é
noexec). `x11-env.sh`/`wl-env.sh` instalam o big-wm e o dock no contêiner como
num sistema real (`make install-*`, `meson install`) e aplicam o
`big-wm-firstrun`.

## O que foi verificado em cada sessão

**X11 (43 ✅)**: tipo dock e strut parcial; de ponta a ponta por padrão (x=0,
1600 px, strut de 0 a 1599); a seta recolhe a prateleira (e tira a reserva),
tocar a borda a traz de volta e ela fica (sem auto-ocultar); 4 abas;
troca de aba por clique, rolagem, `Super+Alt+1`, `Super+Alt+PgDn` e `--tab`;
lançar o xterm pelo dock; indicador e janela ativa; clique minimiza, clique
restaura; botão do meio abre 2ª janela; clique alterna entre as janelas;
aplicativo não fixado na seção "em execução"; arrastar item para outra aba;
nova aba pelo "+" (diálogo); persistência após reiniciar; arquivo corrompido
guardado à parte e `.bak` usado; reposicionar em 1280×720 (xrandr); soltar
arquivo e pasta vindos de uma fonte XDND (ação COPY, original intacto);
remover atalho não apaga o arquivo; borda superior + direita + estilo
moderno + strut no topo; auto-ocultar (faixa, sem strut, reaparece na borda);
preferências; `GDK_SCALE=2`; `--dock-mode=tabbed` grava `DisableDock` pelo
`wdwrite` sem tocar o WMState e `both` reverte.

**Wayland (34 ✅)**: de ponta a ponta (âncora esquerda+direita; o wlmaker dá
a largura útil, 1536 px ao lado do Clip); a seta recolhe e a borda traz de
volta; cliente nativo em layer-shell; rastreamento por
`ext-foreign-toplevel-list`; ancorado na borda; abas por clique, rolagem e
`--tab`; lançar a calculadora (GTK 4) com indicador por app_id; clique num app
que não pode ser ativado abre o menu de janelas (no pressionar do botão) em vez de duplicar; app
DBusActivatable ativado por `org.freedesktop.Application.Activate` com token
xdg-activation real (o token chegou ao app) sem segunda instância; app Qt
nativo rastreado; superfície encolhe quando a seção some; menu fecha sozinho
(contorno do wlmaker); arrastar entre abas e reordenar; persistência;
reposicionar em 1280×720 (wlr-randr); auto-ocultar; borda superior + moderno;
preferências; coexistência com o `wlmdock`; atalhos no `Config.plist` padrão;
duas saídas: principal, `Monitor=HEADLESS-2` (nome por xdg-output), saída
desligada → volta para a outra, processo sobrevive.

**❌ zona exclusiva ao maximizar** no wlmaker `f11a08a`: o dock pede a zona
certa, o compositor não a aplica (regressão do wlmaker; com a proposta 0044 o
mesmo teste passa). **Pendentes**: tecla `Logo+Alt+N` no wlmaker (o `wtype` não
aciona atalhos de dois modificadores no headless — o comando que ela chama é
testado); religar uma saída (o headless do wlroots remove a saída desligada).

**Com as propostas 0042–0044 (28 ✅)**: backend
`wayland-wlr-foreign-toplevel` (caps=31); estado ativo; clique minimiza;
clique restaura e ativa; alternar entre duas janelas; zona exclusiva respeitada;
e os demais testes funcionais iguais.

## Medições

Método (`tabbed-dock/tests/session/measure.sh`, igual nas duas sessões):
PSS/USS de `/proc/<pid>/smaps_rollup`; CPU de `/proc/<pid>/stat` (utime+stime)
num intervalo de 60 s sem interação; tempo até o primeiro quadro com
`BTD_TIMING=1` (relógio monotônico do `main()` ao primeiro `draw`), 3
execuções; sessão = soma do PSS de todos os processos do usuário antes e 5 s
depois de iniciar o dock, com os caches descartados antes.

| | X11 | Wayland |
|---|---|---|
| Primeiro quadro (1ª execução, fria / seguintes) | 85,5 ms / 24,3 e 25,4 ms | 83,7 ms / 27,9 e 27,8 ms |
| PSS / USS / RSS do dock | 25,5 / 18,0 / 41,4 MB | 26,4 / 17,1 / 42,3 MB |
| PSS no fim do teste funcional, visual de ponta a ponta (mármore 2× 256×256, nomes) | 28,4 MB | 25,6 MB |
| Threads | 7 | 8 |
| CPU ociosa em 60 s | 0 ticks | 0 ticks |
| PSS da sessão antes → depois | 77,5 → 124,1 MB | 30,6 → 82,0 MB |
| …dos quais o dock | 25,5 MB | 26,4 MB |
| …dos quais o xdg-desktop-portal ativado pelo GTK | 21,4 MB | 26,2 MB |
| Equivalente Python + GTK 3: primeiro quadro / PSS | 140–148 ms / 43,4 MB | 137–147 ms / 44,0 MB |

Notas honestas:

- No contêiner o portal não estava rodando e o GTK o ativou ao iniciar o dock;
  numa sessão real ele já roda (big-wm usa xdg-desktop-portal-gtk), então o
  custo adicional do dock é o PSS dele.
- O PSS cai numa sessão real porque as bibliotecas GTK 3 são divididas com
  outros aplicativos; no contêiner o dock é praticamente o único usuário delas.
- **Antes da correção dos ícones**, o dock carregava ícones pelo gdk-pixbuf
  2.44, que abre cada formato num processo glycin isolado: a sessão ganhava
  processos `bwrap`, `glycin-svg` e `glycin-image-rs` (delta da sessão X11 de
  51,3 MB contra 46,6 MB depois). Hoje PNG vai pelo cairo e SVG pelo librsvg.
- Medição numa sessão real (VM ou hardware): **pendente**.

## Pacotes

`makepkg --nodeps` do branch num `manjarolinux/base` limpo, com as
dependências de build do CI (o `build()` usa `arch-meson`, o `check()` roda
`meson test` e `tests/run.sh`): gera `big-wm-session`, `big-wm-wayland`,
`big-wm-themes` (any) e `big-wm-tabbed-dock` (x86_64). O nome da unidade do
autostart (`app-big\x2dwm\x2dtabbed\x2ddock@autostart.service`) foi conferido
com o próprio `systemd-xdg-autostart-generator`, para o drop-in valer.
Teste do pacote numa sessão logada pelo greeter: **pendente**.
