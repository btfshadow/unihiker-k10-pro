# Plan 08 - AI

## Objetivo
Refatorar IA para máquina de estados por modo e contratos de dados estáveis.

## Entregas
- Vision HAL
- Vision service com modos (`face`, `cat`, `move`, `qr`)
- Eventos e structs tipadas para resultados

## Fases
1. Isolar troca de modo
2. Consolidar resultados e eventos
3. Integrar overlay opcional com display

## Validação
- Alternância de modos sem leak de task/fila
