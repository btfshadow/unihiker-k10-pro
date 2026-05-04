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
