# unihiker-pro

SDK reescrita para UNIHIKER K10 com arquitetura multi-camadas:

- Core: tipos comuns, config e contratos compartilhados
- HAL: adaptação de hardware e drivers base
- Services: regras de uso por domínio
- Facade: API pública simples para aplicações

## Estrutura
- `src/layers/core`
- `src/layers/hal`
- `src/layers/services`
- `src/unihiker_pro.h`

## Objetivos
- Substituir progressivamente a lib original do UNIHIKER com implementacao propria e publicavel
- Oferecer API em dois niveis: uso simplificado e uso completo/avancado
- Facilitar extensao e manutencao
- Reduzir acoplamento
- Permitir evolucao segura com compatibilidade progressiva
- Otimizar desempenho e previsibilidade de comportamento em hardware real
- Aumentar cobertura de validacao com testes unitarios e smokes por dominio

## Status
- Versao inicial com wrappers funcionais sobre drivers originais e contratos de servico para evolucao incremental.

## Direcao de evolucao
- `external_packages/` e tratado como base de referencia tecnica, nao como limite de implementacao.
- Evolucoes devem ocorrer por padrao em `unihiker-pro/`, com foco em qualidade de API, configurabilidade e eficiencia.

## Estado atual
- Foundation fechada com `core`, `hal`, `services` e `facade`
- Board IO com leitura/escrita de pinos do expansor, callbacks de botão e controle básico de RGB/backlight
- Workaround incorporado para o board legado: `init_board`, `digital_write` e `digital_read` sao interceptados por linker wrap no PlatformIO para evitar o caminho binario do codec ES7243E com endereco incorreto

## Exemplos
- `examples/basic/basic.ino`
- `examples/board_io_demo/board_io_demo.ino`
- `examples/legacy_compat_demo/legacy_compat_demo.ino`

## Compatibilidade com sketches legados
- Header de shim: `src/unihiker_pro_legacy.h`
- Shim de migracao rapida: `unihiker_pro::UniHikerProLegacyShim`
- Objetivo: permitir port minimo de sketches que usam `k10.initScreen()`, `k10.creatCanvas()`, `k10.canvas->canvasText(...)`, `k10.buttonA->setPressedCallback(...)` e `k10.rgb->write(...)`.
