# 7. Segurança e privacidade

- **Markup**: só `<b>`, `<i>`, `<u>` (balanceadas) e `<br>`; links e imagens
  ficam só com o texto (nenhuma URL é aberta a partir de uma notificação;
  `body-hyperlinks` e `body-images` não são anunciados). Entidades inválidas
  e `<` solto viram texto. O resultado é validado pelo Pango; se falhasse,
  iria escapado.
- **Texto**: UTF-8 inválido corrigido; caracteres de controle e sobrescritas
  bidirecionais (falsificação de texto) removidos; título até 512 caracteres,
  corpo até 4096.
- **Imagens**: `image-data` conferido (dimensões até 2048, 8 bits, 3 ou 4
  canais, stride e tamanho dos dados) e reduzido a 128 px; caminhos só
  absolutos ou `file://`, carregados já em 128 px.
- **Ações**: as chaves voltam ao remetente em `ActionInvoked`; nunca são
  executadas. Chaves vazias, longas ou com controle são descartadas; até 8.
- **Abuso**: 20 notificações a cada 10 s por remetente com balão (as demais só
  no histórico); até 256 abertas no protocolo; `replaces_id` só do mesmo
  remetente ou aplicativo; `desktop-entry` só com `[A-Za-z0-9._-]`.
- **Foco**: o token de ativação entregue a um aplicativo vem do clique do
  usuário numa ação; o compositor (wlmaker 0159) recusa os demais.
- **Histórico**: `$XDG_STATE_HOME/big-wm/notifications-history`, modo 0600 em
  pasta 0700, gravação atômica; sem imagens nem ações; notificações
  `transient` nunca são gravadas; retenção por dias e quantidade;
  `History=false` apaga o arquivo. Arquivo danificado é ignorado.
- **Tela bloqueada**: sem balões enquanto o logind indica `LockedHint`
  (exceto críticas); as notificações vão para o histórico.
- **Logs**: o servidor não registra o conteúdo das notificações.
- Sem privilégios: roda como o usuário; o único sinal enviado a outro
  processo é o SIGTERM a um `dunst` que tomou o nome na sessão Big
  WindowMaker.
