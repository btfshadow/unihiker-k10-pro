# 20 — Bibliotecas Arduino-ESP32 incluídas no framework

A pasta [external_packages/framework-arduinounihiker/libraries/](../external_packages/framework-arduinounihiker/libraries/) é
um *fork* dos *bundled libraries* do Arduino-ESP32 (versão da DFRobot para a K10),
mais as adições específicas (`unihiker_k10`, `AIRecognition`, `asr`,
`DFRobot_AHT20`, `DFRobot_ESPASR`, `DFRobot_IICScan`, `HUSKYLENS`,
`lv_lib_qrcode`, `lvgl`, `TFT_eSPI`).

Esta página é um índice rápido com a função de cada uma e pontos onde a K10
as utiliza. Para a API canônica, consulte a [documentação oficial Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/).

## Núcleo / Hardware

| Lib | Papel | Onde a K10 usa |
|-----|-------|----------------|
| `Wire` | I²C master | I²C principal P19/P20 (todos os sensores + XL9535 + câmera SCCB) |
| `SPI` | SPI master | LCD (`TFT_eSPI`), SD card, font chip — múltiplos hosts |
| `EEPROM` | NVS-backed EEPROM emulation | Não usada por padrão; disponível |
| `Preferences` | Wrapper NVS | Onde modelos AI armazenam DB de faces |
| `Ticker` | Timer alta-resolução | Não usada por padrão |
| `RMT` | Espressif RMT (IR/WS2812) | Adafruit_NeoPixel pode usar `rmt_*` |
| `I2S` | API ESP-IDF I²S | Substituído pelas chamadas diretas `driver/i2s.h` em `unihiker_k10.cpp` |
| `USB` | TinyUSB | Reservada (não exposta nos exemplos) |

## Storage

| Lib | Papel | Uso K10 |
|-----|-------|---------|
| `SD` | SPI SD/microSD | `k10.initSDFile()` chama `SD.begin()`. Path FATFS via `S:/` no LVGL. |
| `SD_MMC` | SDIO/SDMMC | Não usada (slot é SPI no schematic) |
| `FS` | Sistema base | Underlying de SD/SPIFFS/FFat/LittleFS |
| `SPIFFS` | Filesystem em flash | Disponível |
| `LittleFS` | Filesystem em flash (preferido) | Disponível |
| `FFat` | FAT em flash | Disponível |

## Conectividade

| Lib | Papel |
|-----|-------|
| `WiFi` | STA/AP TCP/IP |
| `WiFiClientSecure` | TLS (mbedTLS) |
| `WiFiProv` | Provisioning BLE/SoftAP |
| `WebServer` | HTTP server síncrono |
| `AsyncUDP` | UDP assíncrono |
| `DNSServer` | Captive DNS |
| `ESPmDNS` | mDNS (`<host>.local`) |
| `NetBIOS` | NetBIOS over TCP |
| `Ethernet` | RMII/SPI Ethernet (sem hw on-board, mas pode-se conectar) |
| `HTTPClient` | Cliente HTTP[S] |
| `HTTPUpdate` | OTA via HTTP |
| `HTTPUpdateServer` | OTA pull |
| `Update` | API de OTA do Arduino-ESP32 |
| `ArduinoOTA` | OTA com mDNS |
| `RainMaker` | ESP-RainMaker (cloud Espressif) |
| `Insights` | ESP-Insights telemetria |

## Bluetooth

| Lib | Papel |
|-----|-------|
| `BLE` | NimBLE/Bluedroid GATT/GAP |
| `SimpleBLE` | API simplificada (legado) |
| `BluetoothSerial` | SPP-like |

## Internas DFRobot (relevância K10)

| Lib | Função | Doc |
|-----|--------|-----|
| `unihiker_k10` | Fachada da placa | [10-lib-unihiker_k10.md](10-lib-unihiker_k10.md) |
| `AIRecognition` | Visão computacional (face/cat/move/QR) | [11-lib-AIRecognition.md](11-lib-AIRecognition.md) |
| `asr` + `DFRobot_ESPASR` | ASR + TTS | [12-lib-asr.md](12-lib-asr.md) |
| `DFRobot_AHT20` | Sensor T/H | [13-lib-sensors.md](13-lib-sensors.md) |
| `DFRobot_IICScan` | Scanner I²C utilitário | Helper de diagnóstico |
| `HUSKYLENS` | Câmera Huskylens externa (UART/I²C) | Para acessório opcional |
| `Adafruit_NeoPixel` | Driver WS281x | Usado por `RGB` |
| `lvgl` | UI graphics | UI da câmera + canvas + qrcode |
| `lv_lib_qrcode` | QR generator | `canvasDrawCode` |
| `TFT_eSPI` | LCD ILI9341 | Backend de `lv_disp_drv` |
| `ESP32_Display_Panel` / `ESP32_IO_Expander` | Panel API + expander wrapper | Possível alternativa moderna ao stack acima |

## Como o `platformio.ini` puxa tudo isso

O framework é resolvido pelo PlatformIO via
[platformio-build-esp32s3.py](../external_packages/framework-arduinounihiker/tools/platformio-build-esp32s3.py)
e `platformio-build.py`. As bibliotecas em `libraries/` são auto-detectadas pelo
LDF (Library Dependency Finder) — qualquer `#include` puxa as dependências
recursivamente.

## Notas

- A versão dessas libs **não é a oficial Arduino-ESP32 mais recente**: é um
  *snapshot* customizado (e.g. `cores/esp32` foi alterado em pontos para incluir
  `who_camera`, `app_*`).
- `who_camera`, `app_wifi`, `app_httpd`, `app_mdns`, `ai.hpp` **não são**
  bibliotecas Arduino — são módulos C/C++ ESP-IDF expostos via header em
  `tools/sdk/esp32s3/include/modules/`. Eles são linkados como `.a` estáticas.
