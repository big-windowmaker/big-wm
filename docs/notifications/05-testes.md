# 5. Testes

## Automáticos (tests/run.sh, CI)

- `tests/notifications/test-util.c`: markup (tags permitidas, `<script>`,
  links, imagens, entidades válidas e inválidas, aninhamento de 200 níveis,
  UTF-8 inválido, sobrescrita bidi), texto, tempo relativo (agora, minutos,
  horas, ontem, dias, semanas, data), busca sem acentos, chaves de ação.
- `tests/notifications/test_server.py` (barramento e Xvfb próprios):
  informações e capacidades exatas; `CloseNotification` → motivo 3;
  `replaces_id`; expiração → motivo 1; crítica não expira; image-data
  gigante/curta/negativa/16 bits; corpo de 1 MB; caracteres de controle;
  `desktop-entry` com `../`; 80 notificações seguidas (≤ 3 balões, limite por
  remetente, todas no histórico); "não perturbe" persistido; clique real numa
  ação (xdotool) → `ActionInvoked` + motivo 2; segunda instância sai;
  histórico 0600 em pasta 0700, arquivo danificado, releitura; Limpar.
- Escolha do servidor: `big-wm-notifyd` (com e sem dunstrc do usuário) e o
  `dunst.service` cortado só nas sessões Big WindowMaker.

## Lab de foco (tests/notifications/focus-lab/run.sh)

wlmaker headless aninhado, ponteiro e teclado virtuais, app GTK de prova.
Resultado (17/11/2026):

| Critério | wlmaker 43 + big-wm-notifications | wlmaker 44 + big-wm-notifications | wlmaker 44 + dunst |
|---|---|---|---|
| digitar com notificação no meio da palavra | passa | passa | passa |
| menu aberto quando chegam 2 notificações | passa | passa | passa |
| balão sob o botão apertado (sem leave, com release) | **falha** | passa | passa |
| arrastar para fora da janela recebe o release | **falha** | passa | passa |
| roda depois disso | passa | passa | passa |
| app se apresentando sem entrada do usuário | **falha** | passa | passa |

O lab também confere o clique (wlmaker 44): balão sem ação "default" traz a
janela do aplicativo; balão de GNotification executa a ação padrão do
aplicativo. No X11 (Xvfb + Window Maker) o mesmo foi verificado à mão com os
dois aplicativos de teste (`fdoapp.py`, `gapp.py`).

Também verificado no lab (scripts de cenário): XWayland (GTK com
`GDK_BACKEND=x11`) igual antes e depois; token de clique numa ação do balão
traz o app à frente; token com serial antigo sem superfície recusado; ctest
do wlmaker 30/30.

## X11 (Xvfb + Window Maker + xdotool)

Com big-wm-notifications: digitação contínua com notificação; clicar no `×`
de um balão e continuar digitando no mesmo campo; menu aberto durante duas
notificações (fecha só no Esc); arraste e roda sem interrupção. O Xvfb tem
um monitor; vários monitores e escala não foram testados.

## Sininho e tema

Os três estados do sininho (normal, contador, "não perturbe") verificados no
`big-wm-tray` (Wayland) e no snixembed + wmsystemtray (X11, Xvfb). Tema
escuro e troca ao vivo para o claro com a central aberta (Wayland).

## Não verificado

- Hardware real (o OptiPlex de teste estava fora da rede); Inkscape, GIMP e
  Firefox reais (o app de prova reproduz o mesmo protocolo GTK).
- Leitor de tela.
