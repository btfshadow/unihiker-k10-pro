# Plan 12 - Input Navigation

## Objetivo
Criar um plano dedicado para comportamento de botoes A/B/AB e navegacao entre telas, com contrato unico de gestos e baixo indice de evento perdido/duplicado.

## Escopo
- Gestos de botoes: press, release, short, long e chord AB.
- Priorizacao de eventos quando houver conflito A/B/AB.
- Padrao de navegacao para menu e submenus.
- Telemetria de input para validar confiabilidade no hardware.

## Fora de escopo
- Mudancas de UI visual fora de hints de navegacao.
- Novos sensores ou mudancas de HAL nao relacionadas a botoes.
- Controle por toque ou encoder (nao existente no hardware alvo).

## Estado atual
- Existem APIs de input basicas e `onReleaseByDuration(...)`.
- A logica de duracao depende de controlador global unico.
- Nao ha contrato global de prioridade para AB contra A/B.
- Cada smoke aplica navegacao por convencao local.

## Entregas
- Contrato de gesto versionado para A/B/AB.
- Arbiter de chord AB com janela configuravel.
- API de binding por contexto de tela (bind/unbind).
- Mapa padrao de navegacao reutilizavel.
- Smoke novo de stress para input e navegacao.

## Contrato alvo (V1)
1. Chord AB: se A e B forem pressionados na janela configurada, emitir apenas AB.
2. Supressao: ao confirmar AB, suprimir A e B daquele mesmo ciclo.
3. Duracao: short e long por gesto; repeat opcional por tela.
4. Prioridade: AB > A/B em conflitos temporais do mesmo ciclo.
5. Navegacao padrao:
   - A short: anterior
   - B short: proximo
   - AB short: confirmar/entrar
   - A long: voltar
   - B long: acao secundaria
   - AB long: menu raiz ou contexto global

## Fases

### Fase 1 - Baseline e metrica
- Adicionar contadores de eventos no InputService:
  - recebidos
  - emitidos
  - suprimidos
  - duplicados detectados
  - long/short por botao
- Definir estrutura de diagnostico para serial.
- Meta inicial: perda < 1% em 500 interacoes por botao.

### Fase 2 - Arbiter AB
- Implementar maquina de estados curta para consolidar A/B/AB.
- Adicionar configuracao `chordWindowMs`.
- Garantir no maximo 1 evento final por ciclo de gesto.

### Fase 3 - Contexto de handlers
- Evoluir API para bind/unbind por contexto de tela.
- Remover dependencia do controlador global unico de duracao.
- Evitar conflito entre modulos simultaneos (camera/live/menu/vision).

### Fase 4 - Navigation profile
- Criar profile padrao de navegacao com mapeamento unico.
- Integrar profile em:
  - `tests/vision_smoke`
  - `tests/vision_workflow_smoke`
  - `tests/camera_live_flow_test`
- Mostrar hints de comandos ao trocar de contexto.

### Fase 5 - Validacao e rollout
- Criar `tests/input_navigation_smoke` com casos:
  - A e B isolados
  - AB simultaneo
  - long press
  - alternancia rapida de contexto
- Rodar regressao nos smokes com botoes.
- Documentar guia de migracao para API nova.

## Criterios de pronto
- AB nao dispara A/B acidental no mesmo gesto.
- Sem duplicidade em sequencias rapidas de clique.
- Mapeamento de navegacao consistente em pelo menos 3 smokes.
- Teste de stress passa com metricas dentro da meta definida.

## Riscos e mitigacao
- Risco: janela AB muito curta ou longa gerar falso negativo/positivo.
  - Mitigacao: calibrar `chordWindowMs` com teste real em hardware.
- Risco: regressao de callbacks antigos.
  - Mitigacao: manter adaptador legado durante uma versao.
- Risco: conflito entre modulos registrando handlers.
  - Mitigacao: ownership por contexto e unbind explicito.

## Dependencias
- InputService em `src/layers/services/services.h` e `src/layers/services/services.cpp`.
- Atualizacao de contrato em `docs/unihiker-pro-specs/spec-07-io-and-user-interaction.md`.
- Alinhamento com compatibilidade publica em `docs/unihiker-pro-specs/spec-11-public-api-compatibility.md`.
