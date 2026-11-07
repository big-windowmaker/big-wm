# Instalador: perfil bigwindowmaker do Calamares

Rodada de 2026-11-07. Repositórios: `biglinux-livecd` (perfil),
`iso-profiles` (marcador da imagem), `packages` (empacotamento).

## Antes

A imagem escolhia o perfil `bigwm`, gerado no empacotamento como cópia do
`bigcommunity` com só o nome e o `logo.svg` trocados: a ilustração de
boas-vindas, o `big-welcome`, a apresentação (os jogos BigPulse e
BigBlocks, "Powered by BigCommunity") e o painel lateral azul-escuro eram
do BigCommunity.

## Agora

`usr/share/biglinux/calamares-profiles/bigwindowmaker/` é um perfil de
verdade:

- `profile.json`: id `bigwindowmaker`, nome "Big WindowMaker", `wizard_logo`
  (o assistente do live também mostra o logo).
- `branding/bigwindowmaker/branding.desc`: nome, endereços
  (github.com/big-windowmaker), pasta EFI `Big`, janela 1000x600 (cabe
  acima do Dock com abas numa tela de 768 de altura), barra lateral cinza.
- `logo.svg`, `welcome-illustration.svg`, `big-welcome.svg` (+ `.png`):
  da logomarca oficial `big-wm.svg` (viewBox 270,933; o desenho ocupa a
  área inteira, sem margem a cortar), sem distorção. O logo entra num `<g>`
  com escala: o QtSvg (SVG Tiny 1.2) ignora `<svg>` aninhado.
- `welcomeq.qml`: o logo parado, sem as órbitas animadas.
- `calamares-sidebar.qml`: painel cinza com relevo e cantos retos, nas
  cores da paleta do Qt (Kvantum OPENSTEP), marcadores quadrados.
- `BigWindowMakerSlides.qml`: apresentação própria (cinco painéis com
  barra de título do Window Maker, pt/en, parada quando não está à vista).
- `finishedq.qml`: botão Reiniciar clássico (o texto branco sobre o
  highlight branco do OPENSTEP deixava o botão vazio).

O `bigcommunity` fica intacto. O gerador `tools/make-bigwm-profile` saiu;
o `iso-profiles` grava `bigwindowmaker` em `/etc/biglinux-livecd/profile`.

## Testes

- `tests/test_bigwindowmaker_profile.py` e `test_calamares_configuration.py`
  (43 passaram); suíte inteira do biglinux-livecd: 157 passaram (falham só
  por falta de `babel` e `mypy` no ambiente).
- `iso-profiles`: 74 testes passaram.
- VM descartável `bwm-calamares-test` (disco virtual próprio de 30 GB; a VM
  "Big WindowMaker" não foi tocada): ISO de 2026-11-06 com o pacote novo
  instalado na memória do live e o marcador trocado. Todas as etapas
  (boas-vindas, localização, teclado, partições, usuários, resumo,
  instalação com a apresentação, conclusão) com o nome e o logo do Big
  WindowMaker; instalação completa em ~4 min; o sistema instalado inicia no
  login, a sessão Wayland (wlmaker) abre com Dock e Clip.

## Pendente

- ISO nova com o pacote (exige publicar os repositórios).
- O assistente GTK que vem antes do Calamares ("Instalação do Big
  WindowMaker") e o diálogo de EFI são GTK 4/libadwaita, com o visual
  moderno do libadwaita, não o OPENSTEP.
- O `ID` do os-release do sistema instalado continua `bigcommunity`
  (técnico, usado por repositórios; o nome mostrado é Big WindowMaker).
