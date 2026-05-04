# Spec 04 - Sensors

## Sensores disponíveis
- AHT20: temperatura e umidade
- LTR303ALS: luminosidade ambiente
- SC7A20H: acelerômetro + eventos de gesto
- MIC I2S: leitura de amplitude/energia de áudio

## API atual observada
- `AHT20::getData(eAHT20Data_t)`
- `UNIHIKER_K10::readALS()`
- `getAccelerometerX/Y/Z()`
- `getStrength()`
- `isGesture(gesture)`
- `readMICData()`

## Notas de implementação
- AHT20 roda task interna para medições periódicas
- Gestos vêm de leitura periódica de registradores do SC7A20H
- ALS calcula lux por faixa de razão CH1/(CH0+CH1)

## Problemas atuais
- Inicialização e aquisição estão misturadas
- Sem timeout/retry padronizado por sensor
- Sem camada de calibração/configuração externa

## Requisitos para unihiker-pro
- Sensor HAL com contratos por sensor
- Service unificado de sensores com cache temporal
- Config de taxa de atualização por recurso
- API para diagnóstico de presença no barramento
