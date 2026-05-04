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
- Facilitar extensão e manutenção
- Reduzir acoplamento
- Permitir evolução segura com compatibilidade progressiva

## Status
Versão inicial com wrappers funcionais sobre drivers originais e contratos de serviço para evolução incremental.

## Estado atual
- Foundation fechada com `core`, `hal`, `services` e `facade`
- Board IO com leitura/escrita de pinos do expansor, callbacks de botão e controle básico de RGB/backlight
- Workaround incorporado para o board legado: `init_board`, `digital_write` e `digital_read` sao interceptados por linker wrap no PlatformIO para evitar o caminho binario do codec ES7243E com endereco incorreto

## Exemplos
- `examples/basic/basic.ino`
- `examples/board_io_demo/board_io_demo.ino`
