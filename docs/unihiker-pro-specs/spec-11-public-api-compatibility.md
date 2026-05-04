# Spec 11 - Public API Compatibility

## Objetivo
Garantir migração segura de sketches que hoje usam `unihiker_k10`, `AIRecognition` e `ASR`.

## APIs mais usadas nos exemplos
- `k10.begin()`
- `k10.initScreen()`
- `k10.creatCanvas()`
- `k10.initBgCamerImage()`
- `k10.setBgCamerImage()`
- `k10.buttonA/buttonB` callbacks
- `k10.rgb->write(...)`
- `canvas->canvasText(...)` + `updateCanvas()`

## Estratégia de compatibilidade
- Facade `UniHikerPro` com métodos equivalentes
- Adapters opcionais para legado (shim)
- Enumerações e tipos de evento padronizados

## Quebras planejadas (major)
- Remoção de globais públicas
- Retornos tipados para erro/status
- Controle de ciclo de vida explícito (`begin`, `start`, `stop`)

## Critério de pronto
- Exemplo legado portado com mínimo ajuste
- Recursos críticos (display, botão, RGB, SD, foto) funcionando
- Documentação de migração por feature
