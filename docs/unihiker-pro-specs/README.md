# UNIHIKER-PRO Specifications

Este diretório consolida as especificações técnicas do SDK original do UNIHIKER K10 e define a base de evolução para o novo SDK `unihiker-pro`.

## Fontes usadas
- Código em `external_packages/framework-arduinounihiker/libraries/unihiker_k10`
- Bibliotecas auxiliares (`AIRecognition`, `asr`, `DFRobot_AHT20`, `TFT_eSPI`, `lvgl`)
- Headers internos do framework (`tools/sdk/esp32s3/include/modules`)
- PDFs oficiais em `external_packages/*.pdf`

## Índice de especificações
- `spec-00-system-overview.md`
- `spec-01-board-and-pins.md`
- `spec-02-display-and-canvas.md`
- `spec-03-camera.md`
- `spec-04-sensors.md`
- `spec-05-audio.md`
- `spec-06-storage-and-filesystem.md`
- `spec-07-io-and-user-interaction.md`
- `spec-08-ai-vision.md`
- `spec-09-speech-asr-tts.md`
- `spec-10-connectivity-and-web.md`
- `spec-11-public-api-compatibility.md`

## Objetivo
Documentar o estado atual com precisão e preparar uma migração segura para uma arquitetura multi-camadas, mantendo compatibilidade prática com os drivers originais e APIs mais usadas.
