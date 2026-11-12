# 07 — Spectacle no wlmaker

Tecla Print: `big-wm-screenshot full` → `spectacle --fullscreen` ("Todas as
telas"). O Spectacle, no Wayland, captura pelo KWin (org.kde.KWin.ScreenShot2);
fora do Plasma, o `big-wm-kwin-screenshot` oferece essa interface com o grim
(screencopy) e o slurp.

Métodos chamados pelo Spectacle 6.7.5 (medido com o serviço registrando só
nome e parâmetros):

| Modo | Chamada |
|---|---|
| Todas as telas (`-f`) e Região retangular (`-r`) | `CaptureScreen(nome)` uma vez por tela; o Spectacle junta |
| Tela atual (`-m`) | `CaptureActiveScreen` |
| Janela ativa (`-a`) | `CaptureActiveWindow` |
| Selecionar janela (`-u`) | `CaptureInteractive(kind=0)` (1 = tela) |
