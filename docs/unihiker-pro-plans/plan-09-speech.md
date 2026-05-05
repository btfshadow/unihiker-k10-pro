# Plan 09 - Speech

## Objetivo
Padronizar reconhecimento e síntese com fila e prioridades.

## Entregas
- Speech HAL
- Speech service com comandos e callbacks
- Config de idioma/velocidade/wakeup

## Fases
1. Inicialização ASR/TTS
2. Registro de comandos
3. Execução assíncrona de `speak`

## Validação
- Reconhecimento de comandos e resposta por TTS

## Status
- Próximo passo ativo.
- Foco imediato: Fase 1 (inicialização ASR/TTS) com smoke dedicado para validação em hardware.

## Progresso desta implementação
- Criado smoke dedicado em `unihiker-pro/tests/speech_smoke` para validar inicialização ASR/TTS no hardware.
- Smoke registra dois comandos ASR (`hello unihiker`, `status unihiker`) e valida resposta por TTS.
- Build validado no ambiente `unihiker_k10_smoke`.
