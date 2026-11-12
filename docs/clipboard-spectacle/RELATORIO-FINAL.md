# Relatório final

## Área de transferência

**Causa-raiz:** o wmcliphist (histórico no Dock) tomava a seleção de toda
cópia feita num aplicativo X11 e a servia como texto convertido: CRLF virava
LF, os formatos ricos (arquivos, HTML, imagens) sumiam, e no XWayland o
`wl-copy` ainda substituía a seleção do lado Wayland. Na sessão X11 isso
valia para toda cópia. Medido: 16/16 cópias X11 alteradas antes; 0/90
depois. Cópias de aplicativos Wayland (Firefox, VSCodium, gedit, kate) já
chegavam íntegras.

| Critério | Estado |
|---|---|
| Ctrl+C / Copiar consistentes, conteúdo exato | Corrigido e validado (laboratório, aplicativos reais e de teste) |
| Espaços, quebras, Unicode, CRLF | Corrigido e validado |
| Cópias rápidas | Validado (40 duplas rápidas) |
| X11, Wayland, XWayland | Corrigido e validado (laboratório; X11 puro no Xvfb) |
| Histórico não corrompe | Corrigido e validado |
| Conteúdo mantido ao fechar a origem | Validado (bytes exatos) |
| Sensíveis | Sem mudança (já respeitados) |
| Teste na sessão real | Pendente (instalado e em execução) |

## Spectacle

| Problema | Causa | Estado |
|---|---|---|
| Todas as telas só com uma | resposta sem `screen`: imagens empilhadas em 0,0 | Corrigido e validado (laboratório e real: 9440×1440) |
| Tela atual com todas | `CaptureActiveScreen` capturava a área toda | Corrigido e validado (laboratório pelo ponteiro; real pela janela ativa até o próximo login) |
| Região no monitor errado | a imagem de cada monitor vinha trocada (mesma causa) | Corrigido e validado no laboratório |
| Selecionar janela | caixas negativas inválidas, ordem | Corrigido e validado no laboratório |
| Teclado preso | slurp vivo 60 s depois do Spectacle desistir | Corrigido e validado |
| Pipe | descritor preso na mensagem | Corrigido e validado (teste) |
| Janela do Spectacle no monitor do ponteiro | já correto no wlmaker | Validado no laboratório |
| Copiar a imagem | — | Pendente |

## Pacotes (instalados, não publicados)

- big-dockapps 11 (wmcliphist 20260412-11): branch
  `fix/clipboard-spectacle-2026-11-11` do worktree `cliphist`.
- big-wm 26.11.11-1222: branch `fix/auditoria-2026-11-10`.
- wlmaker 1:0.8.r80.g7481afe-33 (patch 0146): vale no próximo login.
