Feature: Portal
================

Descrição curta
---------------
Portal captive + SPA/API unificado para modos AP e STA. Interface primária no device, com versão web e comandos via serial.

Device
------
- Screen ID / Menu path: Menu → Connectivity → Portal
- Entry fn: `enterPortal()`
- Render fn: `renderPortal(bool force)`
- Inputs: A short = abrir/selecionar, A long = confirmar, B short = voltar, B long = voltar-forte
- Estado runtime: `rt_.portalActive`, `rt_.portalSettings` (ver `luci_runtime.h`)
- Files to edit: `unihiker-pro/src/portal.*`, `unihiker-app/src/luci_app.cpp`, `unihiker-app/src/luci_renderer.*`

Web / API
----------
- Endpoints públicos:
  - `GET /` — landing page
  - `GET /scan` — lista redes
  - `POST /connect` — conectar (body: ssid, pwd)
  - `GET /settings` — página de configurações
  - `GET /status` — status público; NÃO retornar `portal_pwd`/`api_key`. Retornar `apiKeyPresent: true|false`.
- APIs sob `/api/*` com autenticação via cookie `portal_auth` (TTL), e checagem `isAuthorized(WebServer*)`.
- Camera endpoints (relacionados): `/api/camera/snapshot` (JPEG), `/api/camera/stream` (MJPEG) — também autenticados.

Segurança
--------
- Nunca exponha segredos em `/status`.
- Para ações sensíveis, exigir sessão autorizada (cookie) e checar permissões.

Serial / Terminal
-----------------
- Comandos úteis: `portal status`, `portal scan`, `portal connect <ssid> <pwd>`, `portal settings`
- Saída: texto legível; oferecer `--json` para parsing

Testes
------
- Build PlatformIO OK
- Teste manual: entrar em Portal via device, executar scan, conectar a STA
- Teste Web: abrir `/` e `/status` sem vazar segredos
- Teste Serial: `portal scan` retorna lista
