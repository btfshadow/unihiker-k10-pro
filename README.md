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
- Navegacao opcional por contexto disponivel em `NavigationService` (5 acoes fixas A/B/AB por duracao, lock de transicao, hints opcionais e contexto via JSON)
- Renderizacao de texto com saneamento UTF-8 no `DisplayService`
- Fonte externa carregavel no display via arquivo LVGL `.bin` (`DisplayService::loadFontFile`), com alias de entrada `.ttf`/`.otf` para buscar o `.bin` correspondente

## Exemplos
- `examples/basic/basic.ino`
- `examples/board_io_demo/board_io_demo.ino`
- `examples/legacy_compat_demo/legacy_compat_demo.ino`

## Convencao de caminhos no cartao SD
- Todos os caminhos de arquivos/diretorios no SD devem comecar com `S:/`.
- Exemplos validos: `S:/photos/foto.bmp`, `S:/audio/clip.wav`, `S:/data/log.txt`.
- Caminhos sem o prefixo `S:/` nao seguem a convencao da API publica.

## Utilitario de fonte (TTF -> LVGL BIN)
- Script: `scripts/ttf_to_lvgl_bin.py`
- Objetivo: converter `.ttf/.otf` para `.bin` compativel com `lv_font_load`.
- Perfil recomendado PT-BR: `ptbr` (ASCII + Latin-1).

Exemplo de conversao:

```bash
python scripts/ttf_to_lvgl_bin.py --input assets/fonts/NotoSans-Regular.ttf --size 24 --bpp 3 --profile ptbr
```

Depois copie o `.bin` para o SD (ex.: `S:/fonts/NotoSans-Regular_24px_bpp3.bin`) e carregue em runtime:

```text
font load S:/fonts/NotoSans-Regular_24px_bpp3.bin
```

Observacao:
- Entrada `.ttf/.otf` no comando `font load` funciona como alias para o mesmo nome `.bin`.
- TTF runtime direto (sem conversao) nao esta ativo nesta build porque FreeType nao esta habilitado.

## Compatibilidade com sketches legados
- Header de shim: `src/unihiker_pro_legacy.h`
- Shim de migracao rapida: `unihiker_pro::UniHikerProLegacyShim`
- Objetivo: permitir port minimo de sketches que usam `k10.initScreen()`, `k10.creatCanvas()`, `k10.canvas->canvasText(...)`, `k10.buttonA->setPressedCallback(...)` e `k10.rgb->write(...)`.
