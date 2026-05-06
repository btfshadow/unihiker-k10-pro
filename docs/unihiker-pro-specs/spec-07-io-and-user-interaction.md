# Spec 07 - IO And User Interaction

## Recursos
- Botões A, B e combinação AB
- RGB WS2812 x3
- GPIOs do edge connector via XL9535
- Controle de backlight e pinos auxiliares

## API atual observada
- `Button::isPressed()`
- `setPressedCallback(cb)`
- `setUnPressedCallback(cb)`
- `RGB::write(index, r,g,b)`
- `RGB::setRangeColor(start,end,color)`
- `RGB::brightness(level)`

## Comportamento atual
- Debounce com polling e delay curto
- Callbacks em task dedicada por botão
- Mapeamento de índice RGB não linear (troca 0/2)

## Riscos
- Multiplicação de tasks por callback
- Dependência de estados globais de IO
- Sem abstração de eventos de entrada

## Requisitos para unihiker-pro
- Input service com eventos (`pressed`, `released`, `long_press`)
- Output service para LEDs e pinos digitais
- Mapeamento documentado de índices RGB
- Configuração central de debounce e polling
- Diagnostico de input para stress em hardware:
	- `diagnostics(InputDiagnostics &out)`
	- `resetDiagnostics()`
	- contadores de `received`, `emitted`, `suppressed`, `duplicatesDetected`
	- contadores por botao para `press/release/short/long`

## Navegacao contextual opcional
- Deve existir modo de menu dinamico por contexto sem obrigar uso de UI.
- Contrato base de acoes por contexto:
	- A rapido (< 2000 ms)
	- A lento (>= 2000 ms)
	- B rapido (< 2000 ms)
	- B lento (>= 2000 ms)
	- AB lento (>= 2000 ms)
- Durante transicao de contexto, entradas podem ser ignoradas por janela configuravel (`transitionIgnoreMs`).
- Cada contexto deve permitir labels/acoes customizadas e opcao de mostrar hints no rodape.
- Deve haver caminho opcional para construcao de contexto por JSON.
- UTF-8 deve ser suportado globalmente no fluxo de exibicao de textos.
