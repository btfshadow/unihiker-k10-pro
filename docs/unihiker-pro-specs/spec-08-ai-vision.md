# Spec 08 - AI Vision

## Capacidades
- Face detection/recognition
- Cat face detection
- Motion detection
- QR code recognition

## Implementação atual
- Biblioteca `AIRecognition`
- Tasks dedicadas por modo (`task_face_recognition`, `task_cat_recognition`, etc.)
- Filas globais para frame, evento e resultado

## API atual observada
- `initAi()`
- `switchAiMode(mode)`
- `isDetectContent(mode)`
- `getFaceData(type)`
- `getCatData(type)`
- `getQrCodeContent()`
- `sendFaceCmd(...)`
- `getRecognitionID()`
- `isRecognized()`

## Riscos
- Ciclo de vida de tasks e filas complexo
- Troca de modo com teardown parcial
- Estado compartilhado sem owner único

## Requisitos para unihiker-pro
- Vision HAL focado em pipeline de frame + inferência
- Vision service com máquina de estados por modo
- Contratos de dados (bounding box, landmarks, QR payload)
- Guardrails de memória e concorrência
