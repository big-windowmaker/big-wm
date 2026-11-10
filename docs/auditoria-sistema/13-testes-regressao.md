# 13 — Testes de regressão

| Onde | Teste | O que prende |
|---|---|---|
| tabbed-dock | `tests/test-geometry.c` (6) | appicons sobre a prateleira não tiram a largura; colunas do Dock/Clip dentro dela sim |
| tabbed-dock | `tests/test-fullwidth.sh` (6, Xvfb) | o mesmo com janelas X de verdade; o binário 0.2.0 falha 4 de 6 |
| big-wm | `tests/run.sh` (estilo) | arquivo salvo pelo WPrefs sem o `IconBack`: "light", e o modo escuro vale |
| big-wm | `tests/doctor/test_doctor.py` (2) | o diagnóstico acha os problemas da auditoria; `--repair` só move |
| big-wm | `tests/run.sh` (traduções) | pt_BR completo (349/349), textos marcados |

Resultados no fim: tabbed-dock `meson test` 12/12; big-wm `tests/run.sh`
"todas as verificações passaram".

Manuais (laboratório): IconSize 48/64, Clip em três posições, gaveta com o
WMState real copiado, tela de bloqueio clara/escura (X11 por prévia, Wayland
no compositor aninhado), modo escuro no X11 e no Wayland.
