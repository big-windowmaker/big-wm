# 11 — Região retangular

O Spectacle captura cada tela (`CaptureScreen`) e mostra a seleção numa
janela em tela cheia por monitor (o wlmaker respeita a saída pedida). Sem a
saída nas respostas, a imagem congelada de cada monitor era a de outro
(capturado antes/depois: pedaços misturados). Com `screen`, cada monitor
mostra a sua imagem; a região de 200×200 desenhada na X11-2 saiu 197×201
(precisão do ponteiro aninhado), com o conteúdo daquele ponto.
