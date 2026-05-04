# Spec 09 - Speech ASR TTS

## Recursos
- Reconhecimento de comandos de voz (ASR)
- Síntese de fala (TTS)

## Implementação atual
- Classe `ASR` herda `DFRobot_ESPASR`
- Uso de `esp_tts.h` para síntese
- API principal declarada em `asr.h`

## API observada
- `asrInit(mode, lang, wakeUpTime)`
- `addASRCommand(id, text)`
- `isWakeUp()`
- `isDetectCmdID(id)`
- `setAsrSpeed(speed)`
- `speak(text)`

## Riscos
- Implementação parcial no pacote atual (header sem cpp no diretório)
- Sem gerenciamento explícito de fila de fala

## Requisitos para unihiker-pro
- Speech HAL que desacople ASR/TTS do app
- Speech service com fila de comandos e prioridades
- API clara para idiomas e wake word
- Modo bloqueante e não-bloqueante para speak
