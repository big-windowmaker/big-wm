# Propostas fora deste repositório

Escritas como diffs para revisão. Os três patches do wlmaker foram adotados
pelo pacote `wlmaker` de big-windowmaker/packages (pkgrel 7), que o CI deste
repositório já usa; a lista da ISO segue como proposta.

## 1. wlmaker: wlr-foreign-toplevel-management, título e zona exclusiva

`0042-toplevel_tracker-wlr-foreign-toplevel-management.patch`,
`0043-xdg_toplevel-forward-the-title-not-the-app_id-to-the.patch` e
`0044-layer_panel-Recomputes-the-usable-area-when-a-layer-.patch`, gerados
sobre o branch `big-wm/protocols` em `f11a08a` (aplicam limpos depois do
0041 do pacote). Verificação feita numa cópia exportada (`git archive`):

- compila; 20/20 testes unitários do wlmaker passam (com e sem os patches);
- com o 0042, o Big Tabbed Dock usa o backend `wayland-wlr-foreign-toplevel`
  (caps=31) e passa nos testes de estado ativo, minimizar, restaurar e
  alternar janelas no wlmaker headless ([../testes.md](../testes.md));
- o 0043 corrige `set_title`, que repassava o `app_id` ao rastreador (docks e
  barras recebiam o app_id como título);
- o 0044 corrige uma **regressão do `f11a08a`**: a zona exclusiva de uma
  superfície layer-shell só entra na área útil se o painel estiver visível,
  mas a área era recalculada no configure, antes do map — janelas
  maximizadas passavam por baixo do dock. Medido com uma janela GTK
  maximizada (altura do conteúdo, tela de 900 px, dock de 97 px, moldura do
  wlmaker de 33 px): `f11a08a` 867 (moldura até a borda da tela, sob o dock);
  com o 0044, 770 (moldura termina em 803, o topo do dock). Afeta qualquer painel
  layer-shell que reserve espaço, não só este dock.

Diff para `big-wm-packages/wlmaker/PKGBUILD` (copiar os dois `.patch` para a
pasta do pacote e subir o `pkgrel`):

```diff
@@ _patches=(
   0041-dock-Moves-the-Dock-and-Clip-like-Window-Maker-and-a.patch
+  0042-toplevel_tracker-wlr-foreign-toplevel-management.patch
+  0043-xdg_toplevel-forward-the-title-not-the-app_id-to-the.patch
+  0044-layer_panel-Recomputes-the-usable-area-when-a-layer-.patch
 )
@@ sha256sums=(
             '8daeb7fb4babd8f9a678f68792a2979dbbd1dd977d3e05da596642ea921b3db6'
-            'e311266e75394b597998186e2044a8cfbaeeac785610bf6e21b40e65249b169d')
+            'e311266e75394b597998186e2044a8cfbaeeac785610bf6e21b40e65249b169d'
+            'd042c1c8fe655664c3d221b4a46e7c43a1b73769287b20a4b984cb3c863557e6'
+            '9e75d14029b5134a907ade61717ad24e4128d19574cd655c5a2225132836fdc5'
+            'c5783bce1c9349f6024642c6253d20fac164dd3afce9c0fb3cd6e7bb5d680799')
```

O `tests/integration/wayland.sh` deste branch já registra
`zwlr_foreign_toplevel_manager_v1` como XFAIL (limitação conhecida) enquanto o
wlmaker empacotado não o tiver; com os patches, passa a PASS.

## 2. ISO: incluir o pacote

`big-wm-iso-profiles/bigcommunity/wmaker/Packages-Desktop`:

```diff
 # Big WindowMaker
 big-wm-session
 big-wm-themes
 big-wm-wayland
+big-wm-tabbed-dock
 windowmaker
 wlmaker
```

Instalado, ele sobe junto com o Dock clássico (modo "both"); quem não quiser
escolhe nas preferências ou com `big-wm-tabbed-dock --dock-mode=classic`. Se a
equipe preferir que a ISO comece só com o Dock clássico, a alternativa é não
listar o pacote e deixá-lo opcional (o `big-wm-session` já o tem em
`optdepends`).

## 3. Pacote

Nenhuma mudança em `big-wm-packages` é necessária para o dock: o
`pkgbuild/PKGBUILD` deste repositório gera `big-wm-tabbed-dock` (x86_64) junto
com os pacotes arch=any, e o CI deste repositório já instala as dependências de
build.
