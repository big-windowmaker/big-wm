# 3. UX/UI

Estilo OPENSTEP próprio (CSS do processo, acima do tema GTK): painel cinza
NeXT com relevo (claro em cima/esquerda, escuro embaixo/direita), faixa preta
como a barra de título ativa do Window Maker, botões chanfrados. Tema claro e
escuro conforme `~/.config/big-wm/theme` (o `big-wm-theme` grava; a troca vale
na hora, por GFileMonitor).

## Balão (toast)

- Cabeçalho: ícone 16 px, nome do aplicativo, tempo relativo (dica: data e
  hora completas), `×`.
- Conteúdo: imagem do remetente (48 px, proporção preservada), título em
  negrito até 2 linhas, texto até 3 linhas com `…` (Pango, quebra por
  palavra/caractere: Unicode, acentos e emojis inteiros).
- Ações do remetente como botões; "default" é o clique no balão.
- Clique direito: fecha. Ponteiro em cima: o tempo pausa (não captura
  teclado nem rolagem).
- Crítico: faixa vermelha, não expira sozinho.
- Até 3 visíveis, empilhados a partir do canto (padrão: superior direito, 76
  px da borda, ao lado do Dock); os demais aguardam.
- Ícone: imagem do remetente → `app_icon` (nome, caminho ou `file://`) →
  ícone do `desktop-entry` → ícone com o nome do aplicativo → genérico.

## Sininho e central

- Sininho: normal; com selo vermelho e o número de não lidas; riscado com
  "não perturbe". Clique: abre/fecha a central. Clique do meio: "não
  perturbe".
- Central: título, "Não perturbe" (switch), **Limpar**, `×`; busca (título,
  texto, aplicativo, sem acentos e sem maiúsculas); lista do mais novo ao mais
  antigo, 50 por vez ("Mostrar mais"); clique na entrada expande o texto;
  `×` remove só aquela; marca azul nas não lidas.
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
