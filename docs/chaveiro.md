# Chaveiro (Secret Service)

O Big WindowMaker usa o **GNOME Keyring** como chaveiro nas duas sessões
(Window Maker no X11 e wlmaker no Wayland). É nele que os programas guardam
senhas e chaves pela API Secret Service (libsecret): senhas de Wi-Fi do
NetworkManager, navegadores, Big Remote Play (senha do Sunshine, chaves do
Headscale, Tailscale e ZeroTier) e qualquer programa GTK ou Qt que use a
libsecret.

## Por que este, e não outro

| Opção | Situação no Big WindowMaker |
|---|---|
| **GNOME Keyring** (escolhido) | leve (só roda quando um programa pede), GTK como o resto da edição, aberto pela senha do login via `pam_gnome_keyring` (o `/etc/pam.d/wdm` e o do LightDM já o carregam), o mesmo das edições Cinnamon e GNOME |
| KWallet | já instalado como dependência de programas KDE, mas fora do Plasma nada inicia o `ksecretd`, precisaria do `kwallet-pam` e o assistente de carteira é Qt |
| KeePassXC | bom gerenciador de senhas, mas não abre com o login e fica aberto na bandeja |

## Como funciona

1. **No login** (wdm ou LightDM com senha), o `pam_gnome_keyring` recebe a senha
   digitada e abre o chaveiro *login*. Na primeira vez ele cria esse chaveiro
   com a mesma senha. Nada é perguntado.
2. **Quando um programa pede uma senha**, o D-Bus inicia o
   `gnome-keyring-daemon` pelo serviço `org.freedesktop.secrets` (ou pelo
   `gnome-keyring-daemon.socket` do systemd do usuário). Não há serviço novo
   na partida da sessão.
3. **Se for preciso perguntar algo** (chaveiro trancado, criado sem login por
   senha), a janela do `gcr-prompter` abre na tela da sessão: o
   `big-wm-session-ctl` passa `DISPLAY`/`WAYLAND_DISPLAY` ao ambiente do D-Bus.

## Casos especiais

- **Login automático** (sessão live): sem senha digitada, o PAM não abre o
  chaveiro. O primeiro programa que guardar uma senha faz o chaveiro pedir uma
  senha nova para ele. Em instalação com login por senha isso não acontece.
- **Trocar a senha do usuário** com `passwd` não troca a do chaveiro *login*
  (o `/etc/pam.d/passwd` não chama o `pam_gnome_keyring`): no próximo login
  o chaveiro fica trancado e pede a senha antiga. Digitá-la só destranca o
  chaveiro naquela sessão; a senha dele continua a antiga e o pedido volta
  no login seguinte. Para alinhar as duas: instalar o **Senhas e chaves**
  (`seahorse`, fora da ISO), clicar com o botão direito no chaveiro *Login*
  → *Alterar senha*, digitar a antiga e depois a nova (a do usuário). Sem
  ele, apagar `~/.local/share/keyrings/login.keyring` faz o PAM criar outro
  com a senha nova no próximo login, mas as senhas guardadas se perdem.
- **Plasma na mesma máquina**: o `ksecretd` do Plasma pega o nome
  `org.freedesktop.secrets` ao iniciar a sessão KDE; o GNOME Keyring só é
  ativado pelo D-Bus quando ninguém tem o nome.

## Verificado (2026-11-17, Big WindowMaker/wlmaker, gnome-keyring 50.0)

Num D-Bus isolado com HOME temporário, o chaveiro *login* foi aberto com uma
senha como o PAM faz; o Big Remote Play passou a ver o chaveiro, moveu a senha
do Sunshine que estava no arquivo sem chaveiro (`sunshine/api-password`) para
ele, apagou o arquivo e uma nova sessão leu a senha do chaveiro. O login real
pelo wdm precisa sair e entrar na sessão.
