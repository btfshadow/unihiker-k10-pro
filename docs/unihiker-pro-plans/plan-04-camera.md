# Plan 04 - Camera

## Objetivo
Criar pipeline de câmera previsível e independente da UI.

## Entregas
- Camera HAL start/stop/frame
- Preview service
- Snapshot service

## Fases
1. Encapsular `register_camera` e filas
2. Implementar preview desacoplado
3. Integrar captura BMP para SD

## Validação
- Preview estável + foto salva sem travamento de UI
