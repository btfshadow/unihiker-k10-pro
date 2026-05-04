# Plan 06 - Audio

## Objetivo
Separar reprodução/gravação e criar arbitragem de barramento I2S.

## Entregas
- Audio HAL com lock de sessão
- Music service (tones/melodias)
- File audio service (WAV)

## Fases
1. Encapsular init/clock I2S
2. Migrar API de melodias
3. Migrar playback/recording SD

## Validação
- Reprodução e gravação funcionais sem conflito

## Progresso desta implementação
- `AudioService` ganhou arbitragem de sessão com lock interno para evitar conflito entre melodia, playback de arquivo e gravação
- Inicialização de I2S encapsulada no `AudioService` (lazy init via `UNIHIKER_K10::initI2S()`)
- Validações de entrada adicionadas para gravação/playback (`path` e `seconds`)
- Teste inicial criado em `unihiker-pro/tests/audio_smoke` para validar play/stop e bloqueio por sessão ocupada

## Status de fase
- Fase 1 iniciada e coberta no serviço com init I2S encapsulado e sessão única de uso
- Próximo incremento: separar controles de tone/melody/file/record em subservices mantendo a facade atual
