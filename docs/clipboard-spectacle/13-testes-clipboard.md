# 13 — Testes da área de transferência

Ferramentas (scratchpad da sessão, não instaladas): um aplicativo GTK de
teste que copia/cola com foco (Wayland nativo ou X11), uma linha do tempo do
dono e dos formatos (Xlib), e a matriz sincronizada (cola só depois do
"copied" do aplicativo). Aplicativos reais: gedit, kate, Firefox.

Achados que não eram defeitos (registrados para não repetir): o laboratório
bloqueou a tela por inatividade (e pelo canto inferior esquerdo) e o
gtklock tomou o teclado; o wmcliphist sem `GDK_BACKEND=x11` caía; o GTK
normaliza CRLF no destino.
