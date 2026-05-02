# 01 — Hardware da UNIHIKER K10 (DFR0992)

Fontes: `Specification - UNIHIKER Documentation.pdf`, `UNIHIKER_K10_Sch.pdf`,
constantes em [external_packages/framework-arduinounihiker/libraries/unihiker_k10/src/unihiker_k10.h](../external_packages/framework-arduinounihiker/libraries/unihiker_k10/src/unihiker_k10.h),
[external_packages/framework-arduinounihiker/tools/sdk/esp32s3/include/modules/board/initBoard.h](../external_packages/framework-arduinounihiker/tools/sdk/esp32s3/include/modules/board/initBoard.h).

## 1. MCU principal

| Item | Valor |
|------|-------|
| Módulo | ESP32-S3-WROOM-1 (N16R8) |
| Núcleo | Xtensa LX7 dual-core, 32-bit, até 240 MHz |
| SRAM | 512 KB |
| ROM | 384 KB |
| Flash | 16 MB |
| PSRAM | 8 MB (octal SPI) |
| RTC SRAM | 16 KB |
| Wi-Fi | IEEE 802.11 b/g/n, 2.4 GHz, BW 20/40 MHz |
| Bluetooth | BT 5 + Bluetooth Mesh (125 Kbps / 500 Kbps / 1 Mbps / 2 Mbps) |

## 2. Componentes on-board

| Componente | Modelo | Endereço/Sinal | Observações |
|------------|--------|----------------|-------------|
| Temp/Umidade | AHT20 | I²C `0x38` | -40~85 °C ±0.3 °C / 0-100 %RH ±2 % |
| Luz ambiente | LTR-303ALS-01 | I²C `0x29` | 0~64 k Lux, 2 canais (CH0/CH1) |
| Acelerômetro | SC7A20H | I²C `0x19` (SDO=HIGH) | ±2/4/8/16 G; também usado para gestos via reg `0x35` |
| Display | LCD ILI9341 | SPI dedicado (LCD_*) | 2.8" TFT 240×320, BLK via expansor |
| Câmera | GC2145M1 | I²C compartilhado + paralelo D2-D9 | 2 MP, 80° FOV |
| Microfone | 2× MSM381ACT001 (MEMS) | I²S (RX) | Estéreo digital, 16 kHz default |
| Codec audio | ES7243EU8 | I²C `0x10..0x17` (config AD0/AD1/AD2) | Não usado pela API Arduino default — fluxo I²S direto |
| Amplificador | NS4168 | I²S (TX) | 2 W, controle CTRL via `eAmp_Gain` |
| LED RGB | 3× WS2812 | GPIO 46 (`PIXEL_PIN`) | Tipo `NEO_GRB + NEO_KHZ800` |
| Font chip | GT30L24A3W | SPI (`MOSI3/MISO3/SCLK3/CS3`) | Fontes ASCII + GBK 12/24 px |
| Expansor IO | XL9535QF24 | I²C `0x20` | 16 GPIOs digitais; gera pinos extras |
| Botões | A, B, RST, BOOT | KeyA via expansor `eP5_KeyA`; KeyB via `eP11_KeyB` | RST e BOOT diretos no ESP32-S3 |
| Slot TF/microSD | Self-eject | SPI compartilhado (`MOSI3/MISO3/SCLK3` + CS via `Q1`) | Habilitado ao inicializar `SD.begin()` |

## 3. Pinagem do ESP32-S3 (do esquemático)

### 3.1 LCD (SPI bit-bang/dedicado)

| Sinal | GPIO ESP32-S3 |
|-------|---------------|
| LCD_SCLK | (definido em `TFT_eSPI/User_Setup_Select.h` para K10) |
| LCD_MOSI | idem |
| LCD_DC   | idem |
| LCD_CS   | idem |
| LCD_RST  | via expansor (RST geral) |
| LCD_BLK  | XL9535 → `eLCD_BLK` |

> Os GPIOs exatos do LCD/Camera estão definidos no `TFT_eSPI/User_Setups/Setup_K10.h` e no driver `who_camera` (ESP-IDF). Verificar em
> `external_packages/framework-arduinounihiker/libraries/TFT_eSPI/User_Setup_Select.h`.

### 3.2 Câmera GC2145

GPIOs `Camera_VSYNC`, `Camera_HREF`, `Camera_PCLK`, `Camera_XCLK`, `Camera_D2..D9`,
`Camera_RST` (este via XL9535). I²C SCCB compartilhado em `P19/SCL` e `P20/SDA`.
Inicializada por `register_camera(PIXFORMAT_RGB565, FRAMESIZE_QVGA, ...)`.

### 3.3 I²C principal (P19/P20)

Conecta: ESP32-S3 ↔ AHT20 ↔ LTR303ALS ↔ SC7A20H ↔ XL9535 ↔ GC2145 ↔ ES7243EU8.

Pull-ups: `R2`, `R3` 10 kΩ.

### 3.4 I²S (microfones + amplificador)

```c
#define IIS_BLCK   0
#define IIS_LRCK   38
#define IIS_DSIN   39   // TX → NS4168 (alto-falante)
#define IIS_DOUT   45   // RX ← MIC/codec
#define IIS_MCLK   3
```

Configuração: `I2S_NUM_0`, master, RX+TX, 16 kHz, 16-bit, formato I²S padrão.
Default channel format: `I2S_CHANNEL_FMT_RIGHT_LEFT` (estéreo).
A taxa é alterada dinamicamente em `Music::playTone` (8 kHz) e
`Music::recordSaveToTFCard` (16 kHz, byteRate 64000) e `taskPlayMusic`
(taxa lida do header WAV).

> Mutex `xI2SMutex` (FreeRTOS) protege I²S contra colisão entre player de
> música, gravação e leitura de microfone.

### 3.5 SPI3 (TF card + Font chip)

Linhas: `MOSI3`, `MISO3`, `SCLK3`. CS:
- TF card: via transistor `Q1` (`MMBT3904T`) controlado por sinal `CS3`.
- Font chip GT30L24A3W: `CS1` direto.

Mutex `xSPIlMutex` evita conflitos entre escrita de áudio/foto e leitura de font.

### 3.6 RGB LEDs

```c
#define PIXEL_PIN    46
#define PIXEL_COUNT  3
```

WS2812; nota: a API `RGB::write(index, …)` faz swap interno
`index = (index==0)?2:(index==2)?0:index` para corrigir a ordem física dos
LEDs serializados.

## 4. Conector de borda (Edge Connector)

Pinout 40 vias, compatível com micro:bit (ver `02-edge-connector.md`).

| Categoria | Pinos disponíveis |
|-----------|-------------------|
| GPIO full-function (digital + analog + PWM) | `P0`, `P1` (no ESP32 direto) |
| Digital IO via XL9535 | `P2`, `P3`, `P4`, `P5_KeyA`, `P6`, `P8`–`P15`, `eAmp_Gain`, `eLCD_BLK`, `eCamera_rst` |
| I²C | `P19/SCL`, `P20/SDA` (3.3 V) |
| 3 V3 / GND | nas extremidades |

> `P0` e `P1` são GPIOs nativos do ESP32-S3 (analogRead, analogWrite, pinMode);
> os demais saem do expansor XL9535 e portanto são **somente digital
> input/output** — ver exemplo "Digital input/output (Extended GPIO)".

## 5. Conectores físicos extras

- USB-C (alimentação 5 V + upload).
- 2-pin PH2.0 bateria (3.0–6.0 V; LiPo 3.7 V ou 3× 1.5 V AA/AAA).
- 2× 3-pin PH2.0 GPIO full-function.
- 1× 4-pin PH2.0 I²C.
- 1× microSD self-eject.

## 6. Alimentação

- USB 5 V → buck JW3651 (`U13`, 2.2 µH inductor) → 3 V3.
- LDO 1.8 V (AP7343Q-18W5-7) e 2.8 V (BL8555-28PRAU6) para câmera.
- Path-switch BCM857BS para selecionar USB ou bateria; AO3401 controla rail VCC.

## 7. Estado dos GPIOs strapping (do esquemático)

| Pino | Default |
|------|---------|
| GPIO0 | Pull-up (BOOT externa) |
| GPIO3 | Sem pull (`I2S_MCLK`) |
| GPIO45 | Pull-down (`I2S_DOUT`) |
| GPIO46 | Pull-down (`PIXEL_PIN`) |
