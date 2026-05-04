# Plan 01 - Foundation

## Objetivo
Estabelecer base de arquitetura em camadas e convenções de projeto.

## Entregas
- Estrutura de pastas `layers/hal`, `layers/services`, `facade`
- Contratos de interface para HAL
- Tipos comuns (status, config, events)
- Composicao da facade por injecao de dependencias

## Fases
1. Definir interfaces mínimas por domínio
2. Implementar adapters para drivers originais
3. Criar facade inicial `UniHikerPro`

## Validação
- Build de biblioteca sem exemplos
- Instanciação básica e `begin()` funcional

## Fechamento desta revisão
- Tipos comuns extraidos para `layers/core`
- Facade `UniHikerPro` aceita HAL customizada alem do adapter legado
- Foundation pronta para evolucao dos modulos sem acoplar services ao provider legado
