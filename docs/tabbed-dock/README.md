# Big Tabbed Dock

Dock horizontal **com abas** para o Big WindowMaker, nas duas sessões: Window
Maker (X11) e wlmaker (Wayland). Recria, com interpretação moderna, a
"prateleira com abas" que a NeXT experimentou numa versão de pré-lançamento do
OPENSTEP 4.0 (fim de 1995) e abandonou antes do 4.0 final.

<p align="center">
  <img src="../screenshots/tabbed-dock/x11-real-02-running.png" alt="Captura real: sessão X11 com o dock com abas na borda inferior" width="100%">
</p>

> **Capturas reais**, feitas pelos testes automatizados: X11 = Xvfb + Window
> Maker 0.96 com a configuração do Big WindowMaker; Wayland = wlmaker (branch
> `big-wm/protocols`) no backend headless com renderização por software.
> Nenhuma é montagem. A VM de testes do projeto **não** foi usada (ver
> [testes.md](testes.md)).

## O que faz

- **Abas**: Geral, Desenvolvimento, Documentos e Temporário (identificadores
  fixos `general`, `developer`, `documents`, `temporary`; nomes traduzidos).
  Criar, renomear, excluir (as criadas), reordenar (arrastando a aba ou pelo
  menu), restaurar o padrão. Sem limite de quatro.
- **Atalhos** de aplicativos (`.desktop`, inclusive Flatpak e AppImage
  registrados), AppImages soltos, documentos e pastas; separadores; ícone e
  rótulo próprios por item.
- **Em execução**: indicador sob o ícone (um "pino" por janela, até três),
  ladrilho destacado para o aplicativo ativo, seção à direita com aplicativos
  abertos que não estão na aba (arraste para fixar).
- **Clique**: abre; se já está aberto, ativa a janela; de novo, minimiza
  (configurável); com várias janelas, alterna entre elas. Botão do meio ou
  Shift+clique: nova instância. Rolagem sobre um aplicativo: alterna janelas;
  sobre o resto: troca de aba.
- **Arrastar**: itens entre abas (mover; Ctrl = copiar), reordenar, fixar um
  aplicativo em execução; arquivos e pastas do gerenciador de arquivos (X11);
  arquivos sobre o ícone de um aplicativo abrem com ele. Abas abrem "por mola"
  quando o arrasto para sobre elas. **Tirar um atalho nunca apaga o arquivo.**
- **Teclado**: `Super+Alt+1…9`, `Super+Alt+PgUp/PgDn` (X11, captura global);
  no Wayland, `big-wm-tabbed-dock --tab next|prev|N` num atalho do compositor.
- **Posição**: embaixo (padrão) ou em cima; centro, esquerda, direita ou
  largura total; monitor escolhido; reserva de espaço opcional; ocultar
  automaticamente; nunca fora da tela após mudança de monitores.
- **Estilos**: Clássico (padrão: abas NeXT, relevo, cores do tema BigLinux) e
  Moderno (opcional, plano e escuro).
- **Coexistência**: só o Dock clássico, só o dock com abas, os dois ou nenhum,
  sem tocar no conteúdo do Dock clássico.

## Documentação

| Documento | Conteúdo |
|---|---|
| [referencias.md](referencias.md) | Pesquisa histórica: fatos documentados × interpretação; GWorkspace TabbedShelf e licença |
| [arquitetura.md](arquitetura.md) | Arquitetura, decisões (C + GTK 3, medido), segurança, desempenho |
| [x11-wayland.md](x11-wayland.md) | Diferenças entre as sessões, limitações honestas e o que falta no wlmaker |
| [uso-e-configuracao.md](uso-e-configuracao.md) | Compilar, instalar, configurar, linha de comando, coexistência, reverter |
| [testes.md](testes.md) | Como testar, resultados reais, medições antes/depois e método |
| [propostas/](propostas/) | Patches propostos para o wlmaker e diffs para pacotes/ISO (não aplicados) |

## Uso rápido

```sh
big-wm-tabbed-dock                 # inicia (o autostart já faz isso no login)
big-wm-tabbed-dock --preferences   # preferências
big-wm-tabbed-dock --tab next      # próxima aba (para atalhos do compositor)
big-wm-tabbed-dock --dock-mode=status|classic|tabbed|both|none
```
