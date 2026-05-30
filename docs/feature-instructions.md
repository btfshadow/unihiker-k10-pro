Template: Instruções por Funcionalidade
=====================================

Visão geral
-----------
Cada funcionalidade deve ser entregue com três interfaces mínimas:

- Dispositivo (navegação via device) — INTERFACE PRINCIPAL
- Web (UI e/ou APIs) — para acesso via navegador/remote device
- Terminal / Serial (comandos via Serial Monitor) — para debug/automatização

Checklist mínima por interface
------------------------------

1) Dispositivo (obrigatório)
- Propósito curto (1-2 linhas)
- Caminho de navegação / nome da tela (ex: Menu → Configurações → MinhaFuncao)
- Funções de entrada/saída: `enter<Nome>()`, `exit<Nome>()`, `render<Nome>(bool force)`
- Controles: A/B, short/long press, gestos; mapear callbacks em `InputService`/`NavigationService` (evitar reuso de `mapping` que sobrescreve callbacks)
- Estados runtime e persistência: campos em `luci_runtime.h` e uso de `Preferences` quando necessário
- UI: fontes (usar `ScopedFont` para alteração local), overlays (press-overlay), layouts e tamanho de fonte
- Erros e edge-cases: perda de conectividade, falha de hardware
- Testes: passos manuais no dispositivo, smoke tests, unit tests (quando aplicável)

2) Web (navegador / API)
- Endpoints recomendados (ex.: `GET /feature`, `POST /api/feature/action`)
- Autenticação: cookie `portal_auth` com TTL, validação `isAuthorized(WebServer*)`, não vazar segredos em `/status` (usar `apiKeyPresent: true|false`)
- Contrato JSON de request/response (exemplos e códigos de erro)
- UI: responsividade, comportamento parity com device (onde aplicável)
- Segurança: CORS, rate-limit, proteção contra CSRF para ações sensíveis
- Testes: chamadas com `curl`, testes automatizados de API

3) Terminal / Serial
- Comandos suportados (ex.: `feature status`, `feature start`, `feature scan`, `feature --json`)
- Formato de saída: texto legível + opcional `--json` para parsing automático
- Parser de comandos e mapeamento interno (ex.: `SerialService::handleCommand()` → chama service)
- Testes: exemplos de sessão serial e outputs esperados

Integração, documentação e qualidade
----------------------------------
- Arquivos a atualizar: `docs/features/<nome-da-funcionalidade>-instructions.md`, README, changelog
- Mensagem de commit sugerida: `unihiker-<componente>: feat <feature> — device/web/serial implemented`
- PR checklist (mínimo): build PlatformIO OK, testes básicos passou, teste manual no device descrito, endpoints documentados
- Performance: evitar redraw completo quando possível; usar redraw parcial ou re-render da região afetada
- Segurança: nunca logar segredos, endpoints de status devem retornar booleans para presença de credenciais

Modelo mínimo (copiar ao criar nova funcionalidade)
-------------------------------------------------
Feature: <FeatureName>
Descrição curta: <1-2 linhas>
Owner: <nome>

Device:
- Screen ID / Menu path: <Menu → ...>
- Entry fn: `enter<FeatureName>()`
- Render fn: `render<FeatureName>(bool force)`
- Inputs: A short, A long, B short, B long
- Estado runtime: campos a adicionar em `luci_runtime.h`
- Files to edit: list

Web:
- Endpoints: list
- Auth: cookie / apiKey
- JSON samples

Serial:
- Comandos: list
- Exemplo de sessão

Checks:
- [ ] Implementado no device
- [ ] Implementado na Web/API
- [ ] Implementado no Serial
- [ ] Documentado em `docs/features/<FeatureName>-instructions.md`

Referências e padrões
---------------------
- Use `ScopedFont` para mudanças temporárias de fonte nas renderizações
- Use `restoreOverlayBackground()` ao esconder overlays para evitar artefatos (dashboard incremental rendering)
- Para capturas de câmera, exponha `/api/camera/snapshot` (JPEG) e `/api/camera/stream` (MJPEG) com autenticação
