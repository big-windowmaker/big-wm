# 08 — Contrato do ScreenShot2

Do código do Spectacle (`ImagePlatformKWin`): ele lê `type`, `width`,
`height`, `format`, `scale`, `windowId` e **`screen`**; posiciona cada
imagem de "Todas as telas" pela tela de nome `screen`. Prazo: 4 s para os
métodos diretos, **60 s** para `CaptureInteractive`.

Correções em `session/bin/big-wm-kwin-screenshot`:
- `screen` e a escala da saída na resposta de `CaptureScreen`,
  `CaptureActiveScreen` e da escolha de tela;
- `CaptureActiveScreen` = uma tela (a do ponteiro), não a área toda;
- descritores: os da mensagem passam a ser do serviço (`steal_fds`) e o do
  pipe é fechado ao fim da escrita; antes o original ficava preso na
  mensagem e quem lia até o fim do arquivo esperava (o teste novo trava com
  o serviço antigo);
- `CaptureWorkspace` continua sendo a área toda.
