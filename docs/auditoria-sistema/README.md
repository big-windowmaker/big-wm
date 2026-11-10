# Auditoria da instalação real — 10/11/2026

Investigação na máquina do desenvolvedor, com a sessão X11 em uso, e no
Wayland em laboratório aninhado. Cada defeito: reprodução, captura, causa,
correção, teste e verificação de novo. Ver [RELATORIO-FINAL.md](RELATORIO-FINAL.md).

As capturas da sessão real (com janelas e arquivos pessoais) ficaram fora
do repositório; as de `img/` são de laboratórios (Xvfb, HOME descartável).

| | |
|---|---|
| [01](01-inventario-maquina.md) | inventário da máquina |
| [02](02-pacotes-binarios.md) | pacotes, binários e o que roda |
| [03](03-autostart-processos.md) | autostart e processos |
| [04](04-dockapps-x11.md) | dockapps no X11 |
| [05](05-dockapps-wayland.md) | dockapps no Wayland |
| [06](06-renderizacao-icones.md) | Dock, Clip e gavetas |
| [07](07-tema-escuro-x11.md) | modo escuro no X11 |
| [08](08-tema-escuro-wayland.md) | modo escuro no Wayland |
| [09](09-reinicio-windowmaker.md) | "Reiniciar o Window Maker" |
| [10](10-logs-erros-alertas.md) | logs, erros e alertas |
| [11](11-multimonitor.md) | monitores |
| [12](12-correcao-pacotes.md) | correções nos pacotes |
| [13](13-testes-regressao.md) | testes de regressão |
| [14](14-performance.md) | desempenho |

O Dock com Abas tem a sua parte no repositório dele:
`docs/16-largura-total-appicons.md` de big-windowmaker/tabbed-dock.
