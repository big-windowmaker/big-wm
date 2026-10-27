# Temas de boot do Big WindowMaker

A tela de boot no estilo do "Window Maker Live": cinza chapado, título grande
em Helvetica (Nimbus Sans), o logo do Big WindowMaker à direita, menu em fonte
de terminal (Nimbus Mono PS) e a barra de seleção azul-aço com sombra. O
Plymouth segue o mesmo visual: o logo num ladrilho do Dock, barra de progresso
afundada e, para disco criptografado, um painel de senha como os do Window
Maker.

| Pacote | Conteúdo |
|---|---|
| `grub-theme-big-wm` | `/boot/grub/themes/big-wm` |
| `plymouth-theme-big-wm` | `/usr/share/plymouth/themes/big-wm` e os hooks do pacman `80-plymouth-theme-big-wm-*` |

Nada binário fica no repositório. No build:

- `grub/gerar.sh` monta o tema do GRUB a partir de `grub/theme.txt.in`: as
  fontes `.pf2` saem do `grub-mkfont` (URW Nimbus Sans Bold e Nimbus Mono PS
  Bold, do pacote `gsfonts`), o logo de `themes/logo/big-wm.svg` pelo
  `rsvg-convert` e as peças da barra de seleção e da rolagem do `pecas.py`;
- `plymouth/gerar.sh` copia `big-wm.plymouth` e `big-wm.script` e gera o logo
  e as imagens do ladrilho, da barra de título e das cores.

As posições do tema do GRUB são em porcentagem com deslocamento em pixels:
o mesmo `theme.txt` serve de 800x600 a 1920x1080.

## Quem escolhe o tema

Os pacotes **não** trocam o tema de ninguém ao serem instalados: outras
edições (ou um usuário que só quer olhar) continuam com o tema que tinham.

Na edição Big WindowMaker, a escolha vem do `iso-profiles`
(`bigcommunity/wmaker/root-overlay`), que vai para o sistema instalado:

- `/etc/default/grub.d/big-wm-theme.cfg` com o `GRUB_THEME`. O
  `grub-mkconfig` lê os arquivos de `/etc/default/grub.d` **depois** do
  `/etc/default/grub`, então o hook do `grub-theme-community`, que reescreve o
  `GRUB_THEME` do `/etc/default/grub` a cada atualização do GRUB, não desfaz
  a escolha;
- `/etc/plymouth/plymouthd.conf` com `Theme=big-wm`; o Calamares gera o
  initramfs com ele. A edição não instala o `plymouth-theme-community`.

A ISO live usa a variante "live" do tema do GRUB (título "Big WindowMaker
Live" e textos traduzidos pelo próprio GRUB), gerada com
`make grub-theme-live DIR=...` e guardada no `iso-profiles` em
`bigcommunity/wmaker/live-overlay/usr/share/grub/themes/bigcommunity-live`.

## Escolher à mão

GRUB:

```sh
echo 'GRUB_THEME="/boot/grub/themes/big-wm/theme.txt"' | sudo tee /etc/default/grub.d/big-wm-theme.cfg
sudo grub-mkconfig -o /boot/grub/grub.cfg
```

Para voltar ao tema anterior, apague `/etc/default/grub.d/big-wm-theme.cfg`
e rode o `grub-mkconfig` de novo.

Plymouth (o hook `plymouth` precisa estar no `HOOKS` do `/etc/mkinitcpio.conf`
e o boot precisa de `splash` na linha do kernel):

```sh
sudo plymouth-set-default-theme -R big-wm
```

Com o big-wm escolhido, os hooks `80-plymouth-theme-big-wm-*` o mantêm quando
outro pacote de tema tenta trocá-lo numa atualização (o
`plymouth-theme-community` roda `plymouth-set-default-theme -R community` a
cada atualização dele). Se o tema ativo era outro, os hooks não fazem nada.
Para voltar a outro tema: `sudo plymouth-set-default-theme -R <tema>`.
