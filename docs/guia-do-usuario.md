# Guia do usuário do Big WindowMaker

Este guia é para quem usa o Big WindowMaker no dia a dia. Os detalhes
técnicos e a tabela completa de atalhos estão no [README](../README.md).

## Primeiros passos

- **Escolha a sessão na tela de login.** **Big WindowMaker** é a sessão X11
  com o Window Maker, a padrão e a mais completa. **Big WindowMaker
  (Wayland)** é a sessão com o wlmaker, ainda experimental. As duas usam a
  mesma aparência, os mesmos atalhos principais e a mesma configuração da
  conta.
- **No primeiro login** abre a janela **Boas-vindas**, que apresenta o menu, as
  janelas, o Dock e o Clip. Ela pode ser aberta de novo pelo menu raiz,
  em **Sessão > Boas-vindas**.
- **O menu raiz** é onde fica quase tudo: clique com o botão direito numa
  área vazia da área de trabalho, ou aperte **F12**.

## Atalhos essenciais

Super é a tecla do Windows. Os mesmos atalhos valem nas duas sessões.

| Para | Aperte |
| --- | --- |
| Buscar e abrir um aplicativo | Alt+Espaço |
| Abrir o terminal | Ctrl+Alt+T |
| Abrir os arquivos | Super+E |
| Histórico da área de transferência | Super+V |
| Bloquear a tela | Super+L |
| Capturar a tela / a janela / uma área | Print / Alt+Print / Shift+Print |
| Alternar entre janelas | Alt+Tab |
| Fechar a janela | Alt+F4 |
| Fechar um programa travado | Ctrl+Alt+Esc e clique na janela (Esc cancela) |
| Gerenciador de tarefas | Ctrl+Shift+Esc |
| Área de trabalho anterior / seguinte | Ctrl+Alt+← / Ctrl+Alt+→ |
| Ampliar a tela | Ctrl+Alt+roda do mouse |
| Ligar / desligar o leitor de tela (Orca) | Super+Alt+S (também em **Configurações**) |

A lista completa, com as diferenças entre X11 e Wayland, está em
[Atalhos de teclado](../README.md#atalhos-de-teclado). No X11, os atalhos se
mudam no WPrefs (menu **Configurações**).

## Dock, Clip e dockapps

- O **Dock** fica à direita e guarda os programas que você usa sempre. O
  **Clip**, no alto à esquerda, guarda os lançadores da área de trabalho.
- **Para fixar um programa,** arraste o ícone dele (no canto de baixo, com o
  programa aberto) para o Dock ou o Clip. **Para tirar,** arraste o ícone
  para fora.
- **Dockapps** são os quadradinhos que mostram relógio, volume, música, CPU,
  memória e rede. Outros (calendário, bateria, clima, Wi-Fi...) entram por
  **Configurações > Dockapps**.
- **Dock com Abas:** um dock horizontal com abas (Geral, Desenvolvimento,
  Documentos...). Super+Alt+1…9 escolhem a aba. As preferências ficam em
  **Configurações > Preferências do Dock com Abas**.

## Organizar as janelas

- **Arraste uma janela** e as regiões do layout aparecem: solte a janela
  numa delas para encaixá-la. Esc cancela.
- **Super+Z** escolhe a zona pelo teclado. **Super+Shift+Z** liga ou desliga o
  mosaico automático, que arruma as janelas sozinho. **Super+Shift+R** liga a
  Rolagem: as janelas ficam lado a lado numa faixa que rola.
- O ladrilho **Z / M / R** no Dock mostra o modo atual; as setas dele trocam
  o layout. As opções ficam em **Configurações > Organização de janelas**.

## Aparência

- **Tema escuro:** **Aparência > Tema escuro** (e **Tema claro** para voltar).
  A escolha muda o Window Maker, os aplicativos GTK e Qt, o Dock com Abas e
  as notificações, e vale nas duas sessões.
- **Papel de parede e ícones** ficam em **Aparência**. Em computadores antigos, **Configurações > Efeitos
  visuais** desliga as sombras e transparências e gasta menos CPU.
- **Monitores:** **Configurações > Monitores**. O arranjo escolhido volta no
  próximo login e quando um monitor é ligado ou desligado.

## Bloqueio de tela

- **Super+L** ou o menu **Sessão > Bloquear tela.** A tela de bloqueio aceita
  o teclado na tela (ícone do teclado) para quem não tem teclado físico.
- O fundo e o bloqueio por inatividade ficam em **Configurações > Tela de
  bloqueio...**.

## Manter o sistema atualizado

As correções do Big WindowMaker chegam pelo repositório assinado
[`[big-wm]`](https://github.com/big-windowmaker/repo), junto com as
atualizações do sistema:

```sh
sudo pacman -Syu
```

(ou pelo gerenciador de programas). As instalações feitas a partir de uma
ISO antiga, sem o repositório, precisam ativá-lo uma vez: os passos estão no
[README do repositório](https://github.com/big-windowmaker/repo#ativar).

## Relatar um problema

Use **menu raiz > Sessão > Relatar problema**. Ele monta um relatório com as versões,
a placa de vídeo, os monitores e os erros recentes, **sem nome de usuário,
nome do computador, pastas, IPs ou e-mails**, e mostra o texto inteiro para
você conferir. **Abrir no GitHub** abre o formulário do problema já
preenchido. Lá, conte o que você fez, o que esperava e o que aconteceu; uma
captura de tela ajuda muito. Nada é enviado sem você.

Sem a sessão gráfica, num terminal:

```sh
big-wm-doctor --report --print
```

Falhas de segurança não devem virar issue pública: veja
[SECURITY.md](https://github.com/big-windowmaker/.github/blob/main/SECURITY.md).

## Perguntas frequentes

**Um atalho não funciona.** Confira se outro programa usa a mesma tecla. No
X11 os atalhos estão no menu raiz e no WPrefs; um item do menu pode ter
sido editado.

**O Dock, o Clip ou um dockapp sumiu.** No X11, **menu raiz > Sessão > Reiniciar o
Window Maker**. No Wayland, **Sessão > Recarregar a configuração**: os painéis que
tinham caído voltam. Se continuar, use **Relatar problema**.

**Um programa travou a tela inteira.** Ctrl+Alt+Esc e clique na janela dele.
Se nem o mouse responde, Ctrl+Alt+F3 abre um terminal de texto, onde dá para
entrar e encerrar o programa.

**Instalei por uma ISO antiga e o pacman não atualiza o Window Maker.** As
ISOs antigas tinham a linha `IgnorePkg = windowmaker` no
`/etc/pacman.conf`. Apague essa linha e ative o repositório `[big-wm]` (veja
[Manter o sistema atualizado](#manter-o-sistema-atualizado)).

**O que não existe na sessão Wayland?** Miniaturizar para a área de ícones,
a lista de janelas, ícones antigos (XEmbed) na bandeja e métodos de entrada
(IBus, Fcitx5), entre outros. A lista está em
[X11 × Wayland](../README.md#x11--wayland-o-que-muda).
