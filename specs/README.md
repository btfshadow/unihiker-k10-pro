# UNIHIKER K10 — Especificações Técnicas (Reverse-Engineering Pack)

> Conjunto de documentos de especificação extraídos de:
> - Pasta `external_packages/` (framework Arduino-Unihiker, SDK ESP32-S3, toolchains).
> - PDFs oficiais incluídos no repositório:
>   - `UNIHIKER_K10_Sch.pdf` (esquemático V1.0 — 2025/02/13).
>   - `Specification - UNIHIKER Documentation.pdf`.
>   - `Arduino_PlatformIO API List - UNIHIKER Documentation.pdf`.
>   - `Arduino IDE_Platform IO Examples - UNIHIKER Documentation.pdf`.
>
> Objetivo: documentar APIs, mapeamento de hardware, pinagem e dependências em
> profundidade suficiente para **reimplementar** as bibliotecas/APIs do K10 a
> partir do zero, ou portar para outros frameworks (ESP-IDF puro, MicroPython,
> Rust/embassy, etc.).

## Índice

| Arquivo | Conteúdo |
|---------|----------|
| [01-hardware.md](01-hardware.md) | Especificações físicas, MCU, periféricos on-board, pinagem ESP32-S3 ↔ componentes, mapa de I²C/I²S/SPI |
| [02-edge-connector.md](02-edge-connector.md) | Edge-Connector estilo micro:bit, expansor IO XL9535, pinos `eP*` |
| [10-lib-unihiker_k10.md](10-lib-unihiker_k10.md) | Biblioteca principal `unihiker_k10` — classes `UNIHIKER_K10`, `Canvas`, `RGB`, `Button`, `AHT20`, `Music` |
| [11-lib-AIRecognition.md](11-lib-AIRecognition.md) | Biblioteca `AIRecognition` — face/cat/move/QR-code |
| [12-lib-asr.md](12-lib-asr.md) | Biblioteca `asr` (ASR + TTS), modelos WakeNet/MultiNet do ESP-SR |
| [13-lib-sensors.md](13-lib-sensors.md) | AHT20, LTR-303ALS, SC7A20H, MEMS MIC, NS4168, GC2145 |
| [14-lib-board-io.md](14-lib-board-io.md) | `initBoard.h`, `digital_read/write`, GPIO expander XL9535 |
| [15-lib-fonts-qr.md](15-lib-fonts-qr.md) | `GT30L24A3W` (font chip), `lv_qrcode`, `esp_code_scanner` |
| [20-lib-arduino-esp32.md](20-lib-arduino-esp32.md) | Catálogo de bibliotecas Arduino-ESP32 (Wire, SPI, SD, WiFi, BLE…) usadas no K10 |
| [21-lib-graphics.md](21-lib-graphics.md) | `TFT_eSPI`, `lvgl`, `Adafruit_NeoPixel`, `ESP32_Display_Panel` |
| [30-examples.md](30-examples.md) | Catálogo dos exemplos do K10 (mapeando cada API usada) |
| [99-recreation-guide.md](99-recreation-guide.md) | Roteiro para reimplementação — camadas, dependências, ordem |

## Convenções deste pacote

- **API pública** = aquilo que o usuário escreve em `setup()`/`loop()` num
  sketch Arduino para K10 (`k10.begin()`, `k10.canvas->canvasText(...)`,
  `ai.switchAiMode(...)`, etc.).
- **Camada interna** = código que vive dentro do framework
  (`framework-arduinounihiker/cores`, `tools/sdk`, drivers C/C++ do ESP-IDF
  customizados pela DFRobot: `who_camera`, `ai.hpp`, `app_*`, `initBoard`,
  `esp_code_scanner`, `esp_tts`, etc.).
- **Tarefas FreeRTOS**: o framework cria várias tasks (gesture, camera display,
  AI worker, music player, button polling). São documentadas onde relevantes.
- Onde a fonte é precompilada (presente apenas como `.a/.h` em
  `tools/sdk/esp32s3/include/`), o spec descreve apenas a API pública.

## Mapa de alto nível

```
┌────────────────────────────────────────────────────────────────────┐
│  Sketch Arduino (.ino / main.cpp)                                  │
│  ───────────────────────────────────────────────────────────────── │
│  unihiker_k10  │  AIRecognition  │  asr  │  HUSKYLENS  │  …       │
├────────────────────────────────────────────────────────────────────┤
│  Adafruit_NeoPixel · TFT_eSPI · lvgl · lv_qrcode · DFRobot_AHT20   │
│  esp_code_scanner · GT30L24A3W (font chip)                         │
├────────────────────────────────────────────────────────────────────┤
│  Camadas DFRobot internas (precompiladas em tools/sdk):            │
│  initBoard · who_camera · ai.hpp · app_wifi · app_httpd · app_mdns │
│  esp_tts · DFRobot_ESPASR (ESP-SR wrapper)                         │
├────────────────────────────────────────────────────────────────────┤
│  Arduino-ESP32 core (cores/esp32) — Wire, SPI, SD, WiFi, BLE, I2S  │
├────────────────────────────────────────────────────────────────────┤
│  ESP-IDF / FreeRTOS / drivers HAL (xtensa-esp32s3)                 │
├────────────────────────────────────────────────────────────────────┤
│  Hardware: ESP32-S3-WROOM-1 N16R8 + LCD ILI9341 + GC2145 + AHT20 + │
│  LTR303ALS + SC7A20H + 2× MEMS MIC + NS4168 + 3× WS2812 + XL9535   │
└────────────────────────────────────────────────────────────────────┘
```
