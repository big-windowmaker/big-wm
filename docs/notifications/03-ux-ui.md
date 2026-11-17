# 3. UX/UI

Estilo OPENSTEP próprio (CSS do processo, acima do tema GTK): painel cinza
NeXT com relevo (claro em cima/esquerda, escuro embaixo/direita), faixa preta
como a barra de título ativa do Window Maker, botões chanfrados. Tema claro e
escuro conforme `~/.config/big-wm/theme` (o `big-wm-theme` grava; a troca vale
na hora, por GFileMonitor).

## Balão (toast)

Organização do GNOME 46 com a cara do Window Maker:

- Cabeçalho (a barra de título preta): ícone 16 px, nome do aplicativo,
  etiqueta com a quantidade quando há várias do mesmo aplicativo, tempo
  relativo (dica: data e hora completas), `×`.
- Conteúdo: ícone do aplicativo grande (40 px) à esquerda; título em negrito
  até 2 linhas, texto até 3 linhas com `…`; a imagem do remetente (foto,
  avatar, capa) à direita, 56 px, com moldura em baixo-relevo.
- Ações do remetente como botões.
- **Clique no balão: leva ao aplicativo** (ver "Clique" abaixo). Clique
  direito: fecha. Ponteiro em cima: o tempo pausa.
- Outra notificação do mesmo aplicativo com o balão aberto entra **no mesmo
  balão** (a mais nova, com a contagem); as anteriores ficam na central.
- Crítico: faixa vermelha, não expira sozinho.
- Até 3 visíveis, empilhados a partir do canto (padrão: superior direito, 76
  px da borda, ao lado do Dock); os demais aguardam.
- Ícone do aplicativo: `app_icon` (nome, caminho ou `file://`) → ícone do
  `desktop-entry` → ícone com o nome do aplicativo → genérico. A imagem do
  remetente (`image-data`, `image-path`, ícone de GNotification) vai à
  direita.

## Clique

Como no GNOME, Plasma e XFCE, um clique leva à notificação:

1. GNotification (aplicativos GTK 3/4 e libadwaita, como os do BigLinux, pelo
   `org.gtk.Notifications`): a ação padrão do aplicativo, ou o próprio
   aplicativo, por D-Bus, com o token de ativação.
2. Ação "default" do remetente: `ActionInvoked` (com o token).
3. Em todos os casos a janela do aplicativo vem à frente (Wayland:
   wlr-foreign-toplevel; X11: `_NET_ACTIVE_WINDOW`), procurada pelo processo
   do remetente, `desktop-entry`, `StartupWMClass` e nome.
4. Sem janela: o aplicativo é aberto (ativação por D-Bus ou pelo `.desktop`).

## Sininho e central

- Sininho: normal; com selo vermelho e o número de não lidas; riscado com
  "não perturbe". Clique: abre/fecha a central. Clique do meio: "não
  perturbe".
- Central: título, "Não perturbe" (switch), **Limpar**, `×`; busca (título,
  texto, aplicativo, sem acentos e sem maiúsculas); 50 cartões por vez
  ("Mostrar mais"); marca azul nas não lidas.
- **Agrupada por aplicativo** (GNOME 46): um grupo recolhido mostra a mais
  nova, a etiqueta com a quantidade e faixas em relevo de "cartões" por
  baixo; clicar (ou `⌄`) expande: cabeçalho com o aplicativo, a quantidade,
  `⌃` (recolher) e `×` (remover o grupo), e cada notificação.
- Clique num cartão: leva ao aplicativo e o cartão sai da central (como no
  GNOME); `⌄` no texto longo mostra o texto inteiro; `×` remove só aquele.
- Lida = vista na central (1,5 s com ela aberta); o balão sozinho não marca.
- Limpar mostra "N notificações limpas · Desfazer" por 6 s.
- Abrir a central recolhe os balões (estão todos nela); com ela aberta, uma
  nova notificação entra na lista sem balão por cima.
- Fecha com Esc, clique fora, `×` ou o sininho.

## Não perturbe

Persistente (`DoNotDisturb=true`). Notificações continuam chegando ao
histórico e ao contador; sem balões, exceto críticas
(`DoNotDisturbAllowsCritical=true`, padrão). Ao desligar, um resumo discreto:
"N notificações chegaram nesse meio-tempo".

## Acessibilidade e idioma

Papéis ATK (`notification`, `panel`), nome e descrição nos balões e nas
linhas; botões com nome acessível. Textos no domínio `big-wm` (pt_BR e es
completos; plural pelo ngettext).
