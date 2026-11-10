# 11 — Monitores

- A configuração mudou durante a auditoria: de três monitores (9440×1440)
  para um só, 1920×1080 (ver [01](01-inventario-maquina.md)). Isso expôs os
  defeitos do Clip e das gavetas ([06](06-renderizacao-icones.md)), agora
  corrigidos no Window Maker 24.
- Dock com Abas: o defeito da Largura total foi medido com os três monitores
  (`wm_left=48` no DisplayPort-0, antes da correção); a correção foi
  verificada no monitor único (`wm_left=0`; `wm_right=64` porque, em 1080 de
  altura, a coluna do Dock chega à prateleira) e, no Xvfb, com 1024×768.
- **Não testado:** troca de monitor a quente com a sessão aberta,
  escala fracionária.
