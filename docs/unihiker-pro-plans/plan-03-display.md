# Plan 03 - Display

## Objetivo
Separar UI/Canvas da lógica de câmera e do estado global.

## Entregas
- Display HAL
- Canvas service instanciável
- API de texto, formas e imagem com lock interno

## Fases
1. Adaptar init e background
2. Migrar operações de canvas
3. Adicionar controle de sessão e limpeza segura

## Validação
- Suite de exemplos gráficos renderizando igual ao legado

## Progresso desta implementação
- `DisplayService` passou a ter lock interno (mutex recursivo) para operações de canvas
- Controle de sessão de canvas adicionado com `createCanvas()`/`destroyCanvas()` e limpeza segura
- Compatibilidade preservada para canvas criada no boot legado (adoção automática da primeira sessão)

## Status de fase
- Fase 2 considerada concluída com a suíte gráfica mínima em `unihiker-pro/tests/display_smoke/src/main.cpp`
- Cobertura mínima da suíte: texto/fontes, linhas/formas, ponto/bitmap/clear e ciclo `destroyCanvas()`/`createCanvas()`
- Pendências para retorno posterior: ampliar comparação visual detalhada com legado e adicionar casos de imagem via SD
