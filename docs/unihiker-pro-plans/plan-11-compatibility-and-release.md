# Plan 11 - Compatibility And Release

## Objetivo
Publicar `unihiker-pro` com migração assistida para usuários do SDK antigo.

## Status
- Em andamento.
- Fase 1 iniciada com shim legado cobrindo APIs basicas mais usadas.

## Progresso atual
- Header de compatibilidade legado expandido em `unihiker-pro/src/unihiker_pro_legacy.h` com estilo de uso proximo ao SDK antigo:
	- shim dedicado: `UniHikerProLegacyShim`
	- ponteiros legados: `canvas`, `rgb`, `buttonA`, `buttonB`, `buttonAB`
	- metodos de tela/canvas: `initScreen`, `creatCanvas`, `setScreenBackground`, `canvasText(...)`, `updateCanvas()`, primitives basicas
	- controle de camera de fundo: `initBgCamerImage`, `setBgCamerImage`
	- callbacks de botao via `setPressedCallback`/`setUnPressedCallback`
	- RGB via `rgb->write(...)` e `rgb->brightness(...)`
- Exemplo de migracao minima adicionado: `unihiker-pro/examples/legacy_compat_demo/legacy_compat_demo.ino`.
- Smoke dedicado adicionado: `unihiker-pro/tests/compat_legacy_smoke`.

## Entregas
- Camada de compatibilidade
- Guia de migração por feature
- Versionamento semântico e changelog

## Fases
1. Shim de APIs mais usadas
2. Port de exemplos legados
3. Publicação e checklist de release

## Validação
- Exemplo legado executa com ajustes mínimos
- Build reprodutível
- Documentação completa para adoção
