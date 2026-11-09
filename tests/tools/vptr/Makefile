# vptr: ponteiro virtual (wlr-virtual-pointer-unstable-v1) para testes reais
# de arrastar e clicar num wlmaker (aninhado ou não). Uso:
#   vptr extent W H move X Y down sleep MS move X Y up
# O XML traz só os pedidos do protocolo (wlr-protocols, MIT), na ordem dele.
P = wlr-virtual-pointer-unstable-v1
vptr: vptr.c $(P)-client.h $(P)-protocol.c
	$(CC) -O2 -Wall -o $@ vptr.c $(P)-protocol.c $$(pkg-config --cflags --libs wayland-client)
$(P)-client.h: $(P).xml
	wayland-scanner client-header $< $@
$(P)-protocol.c: $(P).xml
	wayland-scanner private-code $< $@
