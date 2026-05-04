# Spec 02 - Display And Canvas

## Stack de tela atual
- Driver de baixo nível: `TFT_eSPI`
- UI e desenho: `lvgl`
- Flush callback customizado para envio de framebuffer
- Canvas global com `lv_canvas_*`

## Resolução e rotação
- Base: 240x320
- Rotações aceitas: 0,1,2,3

## API atual observada
- `initScreen(dir, frame)`
- `setScreenBackground(color)`
- `creatCanvas()`
- Métodos de canvas:
  - texto por linha e por coordenada
  - bitmap/imagem
  - ponto/linha/círculo/retângulo
  - clear total/parcial

## Fontes
- Fonte custom via chip GT30L24A3W
- Callbacks LVGL de glyph (`myGetGlyphDscCb_*`, `myGetGlyphBitmapCb_*`)

## Pontos críticos
- Uso de globais (`_canvas`, `_cam`, mutexes)
- Buffers grandes e alocação dinâmica frequente
- Falta de separação entre renderização de câmera e overlay de UI

## Requisitos para unihiker-pro
- Service de display isolado da câmera
- Canvas session por instância (sem estado global)
- Render pipeline com lock explícito e limites de memória
- API tipada para fontes e alinhamento
