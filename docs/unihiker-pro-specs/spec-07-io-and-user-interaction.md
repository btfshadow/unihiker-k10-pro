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
