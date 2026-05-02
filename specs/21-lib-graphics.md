# 21 — Stack gráfico do K10 (TFT_eSPI + LVGL + Adafruit_NeoPixel)

## 1. TFT_eSPI

Pasta: [external_packages/framework-arduinounihiker/libraries/TFT_eSPI/](../external_packages/framework-arduinounihiker/libraries/TFT_eSPI/)

Driver de LCD compatível com ILI9341 (e dezenas de outros). Usado pela K10
**apenas** para flush do framebuffer LVGL — não é a API que o usuário toca.

API utilizada em `unihiker_k10.cpp`:

```cpp
TFT_eSPI tft = TFT_eSPI(240, 320);
tft.begin();
tft.setRotation(dir);
tft.startWrite();
tft.setAddrWindow(x1, y1, w, h);
tft.pushColors((uint16_t*)pixels, w*h, false);
tft.endWrite();
```

Configuração: definida em `User_Setup_Select.h` para a placa K10 (driver
`ILI9341_DRIVER`, `TFT_WIDTH=240`, `TFT_HEIGHT=320`, GPIO MOSI/SCLK/DC/CS/RST,
`SPI_FREQUENCY=40000000`, etc.).

> Para reimplementar em ESP-IDF puro, substituir por `lvgl_esp32_drivers` ou
> driver próprio chamando `spi_device_transmit`.

## 2. LVGL

Pasta: [external_packages/framework-arduinounihiker/libraries/lvgl/](../external_packages/framework-arduinounihiker/libraries/lvgl/)

Versão do LVGL embarcada (LVGL 8.x — assinatura de API: `lv_disp_drv_t`,
`lv_canvas_*`, `lv_img_dsc_t`, `LV_IMG_CF_TRUE_COLOR_ALPHA`, etc.).

Componentes ativados pela DFRobot na build:

- `lv_canvas` (fundamental para a API `Canvas`).
- `lv_img` (display da câmera + imagens do SD).
- `lv_label` / `lv_draw_label_dsc_t`.
- `lv_arc` (usado para `canvasPoint` e `canvasCircle`).
- `lv_line` / `lv_rect` (low-level draw descriptors).
- Driver FATFS via `lv_fs_fatfs_init()` (drive `S:`).
- Mutex global `xLvglMutex` envolvendo `lv_task_handler()` e funções de canvas.

### 2.1 Driver de display LVGL

```cpp
static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;

lv_color_t *buf1 = heap_caps_malloc(240*320*sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
lv_init();
lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 240*320);
disp_drv.hor_res = 240;
disp_drv.ver_res = 320;
disp_drv.flush_cb = my_disp_flush;   // adapter para TFT_eSPI
disp_drv.draw_buf = &draw_buf;
lv_disp_drv_register(&disp_drv);
```

> Em `LV_IMG_CF_TRUE_COLOR_ALPHA` o pixel ocupa 4 bytes na K10 (3 RGB + 1 A),
> de onde sai o cálculo de buffer `240*320*sizeof(lv_color_t)*8` bytes para
> o canvas.

### 2.2 Decodificadores

A renderização de imagens BMP/PNG/JPG do SD card depende dos decodificadores
LVGL ativados via `lv_conf.h`. Com base nos exemplos (`canvasDrawImage` aceita
PNG e BMP), pelo menos `LV_USE_PNG` e parser BMP estão habilitados.

## 3. Adafruit_NeoPixel

Pasta: [external_packages/framework-arduinounihiker/libraries/Adafruit_NeoPixel/](../external_packages/framework-arduinounihiker/libraries/Adafruit_NeoPixel/)

Driver clássico para WS2812. A K10 herda da classe via `RGB` (ver
[10-lib-unihiker_k10.md](10-lib-unihiker_k10.md) §4).

API básica usada:

```cpp
Adafruit_NeoPixel strip(num, pin, NEO_GRB + NEO_KHZ800);
strip.begin();
strip.setBrightness(0..255);
strip.setPixelColor(i, color);
strip.show();
```

No ESP32-S3 a transmissão é feita pelo periférico RMT (`RMT.h` + `Adafruit_NeoPixel`
detecta automaticamente).

## 4. ESP32_Display_Panel + ESP32_IO_Expander

Pastas:
- [external_packages/framework-arduinounihiker/libraries/ESP32_Display_Panel/](../external_packages/framework-arduinounihiker/libraries/ESP32_Display_Panel/)
- [external_packages/framework-arduinounihiker/libraries/ESP32_IO_Expander/](../external_packages/framework-arduinounihiker/libraries/ESP32_IO_Expander/)

Bibliotecas modernas (ESP-IDF + Arduino) da Espressif que **não** são usadas
pela API atual do K10 (que opta pelo `TFT_eSPI` legado). Disponíveis para
projetos que prefiram a stack `esp_lcd_panel_*`.

`ESP32_IO_Expander` inclui driver para CH422G, TCA95xx, HT8574 — pode ser
extendido para XL9535 com pouco esforço (`XL9535` segue o padrão TCA9535).

## 5. Reimplementação alternativa recomendada

| Camada | Substituto moderno (ESP-IDF v5) |
|--------|----------------------------------|
| TFT_eSPI | `esp_lcd_panel_ili9341` + `esp_lcd_panel_io_spi` |
| LVGL 8.x | LVGL 9 com `lv_display_create` |
| Adafruit_NeoPixel | `led_strip` (driver oficial Espressif via RMT) |
| `lv_fs_fatfs_init` | `lv_fs_fatfs.c` distribuído pelo LVGL |
