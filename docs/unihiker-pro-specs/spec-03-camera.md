# Spec 03 - Camera

## Implementação atual
- Header interno: `who_camera.h`
- API usada: `register_camera(pixformat, frame_size, fb_count, queue)`
- Consumo via fila FreeRTOS (`QueueHandle_t`) e task de display

## Parâmetros comuns
- `PIXFORMAT_RGB565`
- `FRAMESIZE_QVGA`
- `fb_count = 2`

## Fluxo observado
1. Inicializa objeto LVGL de imagem (`_cam`)
2. Cria fila `xQueueCamer`
3. Registra câmera
4. Task lê frame da fila
5. Atualiza `lv_img_set_src`
6. Devolve frame (`esp_camera_fb_return`)

## Funcionalidades dependentes
- Fundo de câmera na tela
- Captura de foto para BMP no SD
- Alimentação dos pipelines de IA (face/cat/move/qr)

## Riscos atuais
- Concorrência entre display, IA e snapshot
- Reuso de filas globais com ownership difuso
- Dependência de tarefas com ciclo de vida implícito

## Requisitos para unihiker-pro
- Camera HAL com API explícita: start/stop/getFrame/releaseFrame
- Modo preview separado de modo AI
- Pipeline de snapshot desacoplado do preview
- Telemetria de FPS e perda de frames
