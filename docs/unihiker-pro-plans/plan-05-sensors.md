# Plan 05 - Sensors

## Objetivo
Unificar leitura de sensores e gestos com contratos consistentes.

## Entregas
- Sensors HAL por dispositivo
- Sensor service com cache e atualização
- Diagnóstico de presença no barramento

## Fases
1. AHT20 + ALS
2. Acelerômetro + gesto
3. Nível de MIC com normalização

## Validação
- Leituras estáveis e API sem bloqueios longos

## Progresso desta implementação
- `SensorService` agora possui cache temporal configurável por grupo (ambiente, ALS, acelerômetro e MIC)
- Adicionados métodos de refresh explícito: `refreshEnvironment`, `refreshAmbient`, `refreshMotion`, `refreshMic` e `refreshAll`
- Adicionado diagnóstico de presença I2C (`diagnose`) para AHT20, ALS e acelerômetro (endereços conhecidos)
- Criada suíte inicial de validação em `unihiker-pro/tests/sensors_smoke`

## Status de fase
- Fase 1 iniciada com foco em AHT20 + ALS via cache e leitura periódica no `sensors_smoke`
- Próximo incremento: incluir diagnóstico/estado explícito de gesto (SC7A20H) e refinamento de normalização de MIC
