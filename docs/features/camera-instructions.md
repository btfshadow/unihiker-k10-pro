Feature: Camera
================

Descrição curta
---------------
Captura e streaming de câmera via device (preview), API web (`snapshot` e `stream`) e comandos via serial.

Device
------
- Screen ID / Menu path: Menu → Camera → Live
- Entry fn: `enterCameraLive()`
- Render fn: `renderCameraLive(bool force)`
- Inputs: A short = capturar snapshot, A long = início de gravação (se suportado), B short = voltar, B long = voltar-forte
- Observações: não re-mapear `button_control` com o mesmo `mapping` sem recriar a instância; isso pode sobrescrever callbacks (causa problemas de B-long)
- Files to edit: `unihiker-pro/src/camera.*`, `unihiker-app/src/luci_renderer.*`, `unihiker-app/src/luci_app.*`

Web / API
----------
- Endpoints:
  - `GET /api/camera/snapshot` — retorna JPEG
  - `GET /api/camera/stream` — retorna MJPEG (multipart/x-mixed-replace)
- Autenticação: mesma sessão portal (`portal_auth`) ou token API; checar `isAuthorized`.

Serial / Terminal
-----------------
- Comandos: `camera snapshot [--json]`, `camera stream start`, `camera status`
- Saída: URL de snapshot local ou texto / JSON com metadados

Testes
------
- Teste device: abrir Live, pressionar A para snapshot, verificar imagem salva/exibida
- Teste web: `curl http://<ip>/api/camera/snapshot -o snap.jpg`
- Teste serial: `camera snapshot --json` retorna metadata
