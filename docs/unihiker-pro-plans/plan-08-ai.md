# Plan 08 - AI

## Objetivo
Refatorar IA para máquina de estados por modo e contratos de dados estáveis.

## Entregas
- Vision HAL
- Vision service com modos (`face`, `cat`, `move`, `qr`)
- Eventos e structs tipadas para resultados

## Fases
1. Isolar troca de modo
2. Modo live de mira (camera preview + overlay)
3. Modo captura hi-res e review de imagem salva
4. Modo reader de input (arquivo/texto/binário) + entrada OCR com fallback

## Validação
- Alternância de modos sem leak de task/fila

## Progresso desta implementação
- `VisionService` agora possui estado explícito de inicialização e proteção por mutex para troca de modo.
- `setMode(...)` ficou idempotente (troca para o mesmo modo retorna `ok` sem reinicializar pipeline).
- Adicionados contadores/telemetria de alternância (`mode()`, `modeSwitchCount()`) para diagnóstico em runtime.
- Guardrail implementado: `setMode(...)` antes de `init()` retorna `NotInitialized`.
- Adicionados modos de workflow facial no nível de service: `FaceRecognize`, `FaceEnroll`, `FaceDeleteAll`.
- `VisionService` agora expõe `recognized()`, `recognitionId()` e `setMotionThreshold(...)`.
- `VisionService` expandido com workflows explícitos: `LiveAim`, `CaptureReview`, `InputReader`, `Ocr`.
- Fase 2 implementada com `startLiveAim(...)` e `stopLiveAim()` usando preview da câmera para ajudar na mira.
- Fase 2 evoluída com feedback live contínuo: retículo de mira + resumo em tempo real do que a IA está vendo (`QR`, `rosto`, `movimento`, `cat/objeto`), com fallback explícito para texto (`OCR indisponível`).
- Fase 2 também suporta live sem overlay de feedback (`setLiveFeedbackEnabled(false)`), mantendo preview da câmera ativo; no smoke de teste o perfil segue com feedback ligado.
- Refresh do feedback live passou para loop interno do `VisionService` (task dedicada), garantindo rastreio contínuo e escrita sobre a tela da câmera sem depender do `loop()` da aplicação.
- Regra operacional do smoke de workflow: enquanto `live` estiver ativo, mantém feedback+scan+overlay contínuos e bloqueia troca de workflow até sair do live.
- Estabilidade live: corrigido crash de stack canary no task `vision_feedback` com aumento de stack e remoção de refresh manual concorrente no smoke (overlay passa a ser atualizado por uma única fonte: task interna do service).
- Estabilidade retorno do live: corrigido bug de fila no framework `unihiker_k10` (`initBgCamerImage` criava `cameDisPlayTask` com `xQueueAI` em vez de `xQueueCamer`), que podia causar `assert failed: xQueueReceive ((pxQueue))` ao reentrar no live.
- Fase 3 implementada com `captureAndReview(...)` para captura hi-res e visualização da imagem salva.
- Fase 4 implementada com reader genérico (`analyzeInputText`, `analyzeInputFile`, `analyzeInputBinary`, `analyzeInputAny`) e entrada OCR (`runOcrOnInput`) com retorno claro de `NotSupported` quando engine OCR não está disponível.
- Criados smokes: `vision_smoke` (troca de modo) e `vision_workflow_smoke` (workflows 2/3/4), com refresh de overlay no modo live.

## Status de fase
- Fase 1 concluída: troca de modo isolada com mutex + smoke de stress.
- Fase 2 concluída: modo live de mira.
- Fase 3 concluída: captura hi-res + review.
- Fase 4 concluída: input reader + OCR fallback.
