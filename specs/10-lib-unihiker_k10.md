# 10 — Biblioteca `unihiker_k10`

Pasta: [external_packages/framework-arduinounihiker/libraries/unihiker_k10/](../external_packages/framework-arduinounihiker/libraries/unihiker_k10/)

Arquivos-fonte:
- [src/unihiker_k10.h](../external_packages/framework-arduinounihiker/libraries/unihiker_k10/src/unihiker_k10.h)
- [src/unihiker_k10.cpp](../external_packages/framework-arduinounihiker/libraries/unihiker_k10/src/unihiker_k10.cpp)
- [src/GT30L24A3W.h](../external_packages/framework-arduinounihiker/libraries/unihiker_k10/src/GT30L24A3W.h) + `DFRobot_GT30L24A3W.cpp`

Autor: TangJie / DFRobot. Licença MIT. Versão 1.0 (2024-06-21).

## 1. Visão geral

`unihiker_k10` é a fachada de alto nível para a placa. Concentra:

- Inicialização do hardware (`begin()` → `init_board()` + I²C + sensores + I²S +
  3 task FreeRTOS).
- Driver de tela LVGL+TFT_eSPI com canvas grande em PSRAM.
- Câmera GC2145 (display em background) + foto BMP em SD.
- Áudio I²S: tons, melodias built-in, playback WAV em SD, gravação WAV em SD.
- Sensores: ALS, microfone (RMS), AHT20 (via classe filha), acelerômetro, gestos.
- WS2812 RGB (3 LEDs).
- Botões A/B/A+B.
- QR-Code via `lv_qrcode`.

Inclui todos os headers necessários para compilar a maior parte dos exemplos
(é praticamente um "umbrella header"):

```cpp
#include "DFRobot_AHT20.h"
#include "Arduino.h"
#include "who_camera.h"
#include "Adafruit_NeoPixel.h"
#include <Wire.h>
#include "lvgl.h"
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ai.hpp"
#include "initBoard.h"
#include "app_wifi.h"
#include "app_httpd.hpp"
#include "app_mdns.h"
#include "SD.h"
#include "../TFT_eSPI/TFT_eSPI.h"
#include "GT30L24A3W.h"
#include "esp_code_scanner.h"
#include "lv_qrcode.h"
```

## 2. Constantes e tipos públicos

### 2.1 Macros

```c
#define PIXEL_PIN     46
#define PIXEL_COUNT    3
#define IIS_BLCK       0
#define IIS_LRCK      38
#define IIS_DSIN      39
#define IIS_DOUT      45
#define IIS_MCLK       3
```

### 2.2 Enums

```cpp
enum Melodies {
  DADADADUM = 0, ENTERTAINER, PRELUDE, ODE, NYAN, RINGTONE,
  FUNK, BLUES, BIRTHDAY, WEDDING, FUNERAL, PUNCHLINE,
  BADDY, CHASE, BA_DING, WAWAWAWAA,
  JUMP_UP, JUMP_DOWN, POWER_UP, POWER_DOWN,
};

enum MelodyOptions {
  Once = 1,
  Forever = 2,
  OnceInBackground = 4,
  ForeverInBackground = 8,
};

enum Gesture {
  Shake = 0,
  ScreenDown = 1,
  ScreenUp = 2,
  TiltLeft = 3,
  TiltRight = 4,
  TiltBack = 5,
  TiltForward = 6,
  GestureNone = 7,
};

typedef void (*CBFunc)(void);     // callbacks de botão sem argumento
```

### 2.3 Mutex globais expostos

```cpp
extern SemaphoreHandle_t xI2SMutex;   // protege transações I²S
// xLvglMutex e xSPIlMutex são internas mas referenciadas no .cpp
```

## 3. Classe `Canvas`

Wrapper sobre `lv_canvas_*` do LVGL. Buffer interno de `240×320×8 bytes` em
PSRAM (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`), formato
`LV_IMG_CF_TRUE_COLOR_ALPHA`.

### 3.1 Tipos

```cpp
class Canvas {
public:
  enum eFontSize_t {
    eCNAndENFont24,    // 24×24 ASCII+CN
    eJapanFont24x24,
    eKoreanFont24x24,
    eGreeceFont12x24,
    eCNAndENFont16,    // 16×16 ASCII+CN
  };
  ...
};
```

### 3.2 Construtor

```cpp
Canvas(void *obj, int dir);
// obj: ponteiro para lv_obj_t* "screen" (lv_scr_act()) — passado por UNIHIKER_K10::creatCanvas()
// dir: 0/2 → portrait 240×320; 1/3 → landscape 320×240
```

### 3.3 Métodos públicos

| Assinatura | Descrição |
|------------|-----------|
| `void updateCanvas()` | Faz `lv_task_handler()` sob mutex LVGL — flush para tela. |
| `void canvasClear()` | Preenche o canvas inteiro com transparente. |
| `void canvasClear(uint8_t row)` | Limpa uma "linha" (24 px de altura) com base no `dir`. |
| `void clearLocalCanvas(uint16_t x, y, w, h)` | Limpa região retangular. |
| `void canvasDrawBitmap(int16_t x, y, w, h, const uint8_t* bitmap)` | Bitmap RGB565 (`LV_IMG_CF_TRUE_COLOR`). |
| `void canvasDrawImage(int16_t x, y, const void *dir)` | Aceita `lv_img_dsc_t*` ou path SD. |
| `void canvasDrawImage(int16_t x, y, String imagePath)` | Carrega imagem via `lv_fs_fatfs_*` (drive `S:`). |
| `void canvasText(float/String/const char*, uint8_t row, uint32_t color)` | Texto na "linha" `row` (1..13 portrait), font 24, cor `0xRRGGBB`. |
| `void canvasText(text, x, y, color, eFontSize_t font, int count, bool autoClean)` | Texto em coordenadas livres. `count` < 50 ⇒ insere `\n` a cada `count` chars; `autoClean` apaga área antes de desenhar. |
| `void canvasPoint(int16_t x, y, uint32_t color)` | Ponto (arco 0–360) com largura `_lineW`. |
| `void canvasSetLineWidth(uint8_t w = 10)` | Define `_lineW` (default 5 internamente). |
| `void canvasLine(int x1, y1, x2, y2, uint32_t color)` | Linha. |
| `void canvasCircle(int x, y, r, color, bg_color, bool fill)` | Círculo (arc preenchido + arc da borda). |
| `void canvasRectangle(int x, y, w, h, color, bg_color, bool fill)` | Retângulo (border opcional). |

### 3.4 Renderização de texto

A camada usa duas fontes LVGL custom (`my_custom_font_16`, `my_custom_font_24`)
cujos callbacks chamam o **font chip GT30L24A3W** via SPI:

```cpp
ASCII_GetData(unicode_letter, ASCII_24_B, _pBits);
GBK_24_GetData((u2g >> 8) & 0xff, u2g & 0xff, _pBits);
```

A conversão Unicode→GBK é feita por `U2G(unicode_letter)` (ver `GT30L24A3W.h`).
Esse esquema permite renderizar CJK sem incluir glyphs no flash da MCU.

## 4. Classe `RGB : public Adafruit_NeoPixel`

3× WS2812 no GPIO 46.

```cpp
class RGB : public Adafruit_NeoPixel {
public:
  RGB(uint8_t pin = PIXEL_PIN,
      uint16_t num = PIXEL_COUNT,
      uint8_t bright = 5,
      neoPixelType type = NEO_GRB + NEO_KHZ800);

  void write(int8_t index, uint8_t r, uint8_t g, uint8_t b);
  void write(int8_t index, uint32_t color); // 0xRRGGBB
  void setRangeColor(int16_t start, int16_t end, uint32_t c);
  void brightness(uint8_t b);    // 0..9 — mapeado para 0..255
  uint8_t brightness();
};
```

Detalhes:
- `index = -1` → todos os LEDs.
- O construtor chama `setBrightness(map(bright, 0, 9, 0, 255))`.
- `write(index, ...)` faz swap `0↔2` para corrigir a ordem física dos LEDs.

## 5. Classe `Button`

Polling por task FreeRTOS (8 KB stack, prio 5, core 1).

```cpp
class Button {
public:
  Button(uint8_t io);                       // botão simples
  Button(uint8_t io1, uint8_t io2);         // combinação A+B
  bool isPressed(void);
  void setPressedCallback(CBFunc cb);
  void setUnPressedCallback(CBFunc cb);
};
```

`isPressed()` faz debounce manual: 5 retries × `delay(5)`. Os pinos são do tipo
`ePin_t` (cast para `uint8_t`) e lidos via `digital_read(ePin_t)`.

Instâncias criadas por `UNIHIKER_K10::begin()`:

```cpp
buttonA  = new Button(eP5_KeyA);
buttonB  = new Button(eP11_KeyB);
buttonAB = new Button(eP5_KeyA, eP11_KeyB);
```

## 6. Classe `AHT20 : public DFRobot_AHT20`

Lê T/H em background (task `aht20`, 4 KB, prio 2). Período: 1000 ms via
`vTaskDelayUntil`.

```cpp
class AHT20 : public DFRobot_AHT20 {
public:
  enum eAHT20Data_t { eAHT20TempC, eAHT20TempF, eAHT20HumiRH };
  AHT20(TwoWire &wire = Wire);
  float getData(eAHT20Data_t type);
};
```

> **Atenção**: a classe `UNIHIKER_K10` _não_ instancia `AHT20` automaticamente.
> O usuário declara `AHT20 aht20;` no sketch (ver exemplos).

## 7. Classe `Music`

Player I²S all-in-one (TX para NS4168, RX para microfone). Não depende de
`UNIHIKER_K10` para construir, mas exige `k10.begin()` antes (que inicializa I²S
e mutex).

```cpp
class Music {
public:
  Music();

  void playMusic(Melodies m, MelodyOptions opt = OnceInBackground);
  void playTone(int freq, int beat); // beat: 8000 = full, 4000 = half, ...
  void stopPlayTone();
  void playTFCardAudio(const char* path);
  void playTFCardAudio(String path);
  void stopPlayAudio();
  void recordSaveToTFCard(const char* path, uint8_t time); // 16-bit, 16 kHz, byteRate=64000
  void recordSaveToTFCard(String path, uint8_t time);
};
```

### 7.1 Formato das melodias built-in

String com tokens separados por `|`:

```
<note><octave?>:<duration>
```

- nota: `c d e f g a b r` (rest), com `#` opcional.
- octave: dígito (1..7).
- duration: dígito (1=semínima, 2=mínima…). BPM padrão 15 (`buzzMelody.beatsPerMinute = 15`).

Exemplo: `"d4:1|d#|e|c5:2|e4:1|c5:2|e4:1|c5:3|"`

Frequências são tabela `freqTable[]` indexada por `note + 12*(octave-1)` em
`unihiker_k10.cpp`.

### 7.2 Geração de tom (`playTone`)

Sintetiza seno 16-bit a `SAMPLE_RATE = 8000`, dois canais idênticos,
escrito por `i2s_write`.

### 7.3 Player WAV (`taskPlayMusic`)

- Abre WAV via `lv_fs_open` (driver FATFS LVGL — drive `S:` montado em
  `initSDFile()`).
- Lê 44 bytes do header; obtém sample rate em offset `[24..27]`.
- `i2s_set_sample_rates()` para ajustar; lê 1024 bytes por bloco e envia.
- Flag `_stopMusic` permite interromper.
- `playMusicState` reflete (1=tocando, 0=parado), exposto por
  `UNIHIKER_K10::getPlayMusicState()`.

### 7.4 Gravação (`recordSaveToTFCard`)

- 16 kHz, 16-bit, estéreo (`channels = 2`, `byteRate = 64000`,
  `blockAlign = 4`).
- Buffer de 6400 bytes em PSRAM.
- Escreve header WAV PCM linear via `createWavHeader(...)` (RIFF/WAVE/fmt/data).

## 8. Classe `UNIHIKER_K10`

Membros públicos:

```cpp
class UNIHIKER_K10 {
public:
  Canvas *canvas = NULL;
  RGB    *rgb    = NULL;
  Button *buttonA = NULL, *buttonB = NULL, *buttonAB = NULL;
  int     accX, accY, accZ;

  UNIHIKER_K10(TwoWire &wire = Wire);
  ~UNIHIKER_K10();

  // Setup
  void begin(void);
  void initScreen(int dir = 2, int frame = 0);
  void setScreenBackground(uint32_t color);
  void creatCanvas(void);

  // Camera
  void initBgCamerImage(void);
  void setBgCamerImage(bool sta = true);

  // Storage
  void initSDFile(void);
  void photoSaveToTFCard(const char *imagePath);
  void photoSaveToTFCard(String imagePath);

  // Sensors
  uint16_t readALS(void);
  uint64_t readMICData(void);     // ⚠️ header diz int16_t, .cpp retorna uint64_t (RMS)
  int      getAccelerometerX(void);
  int      getAccelerometerY(void);
  int      getAccelerometerZ(void);
  int      getStrength(void);     // sqrt(x²+y²+z²)
  bool     isGesture(Gesture g);

  // QR display
  void canvasDrawCode(String code);
  void clearCode(void);

  // Music state
  uint8_t getPlayMusicState(void);

  // Low-level helpers (públicos por necessidade do módulo gesture)
  uint8_t readData(uint8_t addr, uint8_t arg, void* pBuf, size_t size);
  void    initI2S(void);
};
```

### 8.1 Sequência de `begin()`

1. `xI2SMutex` e `xSPIlMutex` criados.
2. `init_board()` — energiza expansor, I²C interno.
3. `digital_write(eLCD_BLK, 0)` (apaga BL até `initScreen` ligar).
4. `digital_write(eAmp_Gain, 0)` (silencia amplificador).
5. `Wire.begin()`; verifica LTR303ALS (ping I²C `0x29`).
6. Configura LTR303 (`CTRL=0x01` ganho 1×, `MEAS_RATE=0x03` 100 ms / 500 ms).
7. Instancia `RGB`, apaga (cor 0).
8. Instancia botões A, B, AB.
9. `GT_Font_Init()` (font chip).
10. Verifica SC7A20H (`0x19`) e chama `initSC7A20H()` (sequência de regs 0x20–0x37).
11. `initI2S()` — driver i2s_num_0, 16 kHz, RX+TX.
12. Cria task `gesture_task` (period 100 ms, 2 KB, prio 5, core 1) que lê
    `0x27/0xA8/0x35` do SC7A20H para popular `accX/Y/Z` e `_gesture`.

### 8.2 `initScreen(dir, frame)`

- Aloca `buf1` `240×320 lv_color_t` em PSRAM.
- `lv_init()` (uma única vez, controlado por `xLvglMutex` lazy).
- `digital_write(eLCD_BLK, 1)` (liga backlight).
- `lv_disp_drv_init` + `flush_cb = my_disp_flush` (que faz
  `tft.startWrite/setAddrWindow/pushColors/endWrite`).
- `tft.begin()`; `tft.setRotation(dir)`.
- `_scr = lv_scr_act();` clear flag `LV_OBJ_FLAG_SCROLLABLE`.

`dir` válido: 0 (portrait), 1 (landscape), 2 (portrait flipped), 3 (landscape flipped).
`frame` é aceito mas não utilizado na implementação atual.

### 8.3 `creatCanvas()`

Lazy-create do objeto `Canvas` ligado a `lv_scr_act()`.

### 8.4 Câmera de fundo (`initBgCamerImage` + `setBgCamerImage`)

- Cria `lv_img_create(_scr)` 240×320 em (0,0).
- Cria queue `xQueueCamer` (2 frames) e chama
  `register_camera(PIXFORMAT_RGB565, FRAMESIZE_QVGA, 2, xQueueCamer)`
  (driver DFRobot/ESPRESSIF `who_camera`).
- Task `cameDisPlayTask` (4 KB, prio 5, core 1) consome `camera_fb_t*`,
  monta `lv_img_dsc_t` `LV_IMG_CF_TRUE_COLOR` (153600 / `LV_IMG_PX_SIZE_ALPHA_BYTE`),
  `lv_img_set_src` e devolve buffer com `esp_camera_fb_return`.
- `setBgCamerImage(false)` apenas adiciona `LV_OBJ_FLAG_HIDDEN`.

### 8.5 `initSDFile()`

`SD.begin()` em loop até sucesso. Em seguida `lv_fs_fatfs_init()` registra
o driver FATFS no LVGL com letra `S:`. A partir daí, paths como `S:/foto.bmp`
funcionam tanto em `lv_canvas_draw_img` quanto em `lv_fs_open`.

### 8.6 `photoSaveToTFCard(path)`

- Liga amp (`eAmp_Gain=1`) [reuso do pino para algo elétrico — ver schematic].
- Espera `xQueueReceive(xQueueCamer, &frame, portMAX_DELAY)`.
- Escreve BMP com `bitsPerPixel = 16`, `compression = 3` (BI_BITFIELDS) e
  máscaras `R=0xF800 G=0x07E0 B=0x001F` (RGB565).
- Os pixels são gravados linha por linha de baixo para cima e com swap de
  bytes (`pixel >> 8 | pixel << 8`).
- Header layout: `BMPFileHeader` packed, 54 bytes + máscaras.

### 8.7 `readALS()`

Lê 4 bytes de `0x88` no LTR-303ALS (CH1 e CH0). Calcula `_ratio = CH1/(CH0+CH1)`
e aplica fórmula de luminância em quatro faixas — implementação literal do
datasheet:

```
ratio < 0.45    → ALS = 1.7743·CH0 + 1.1059·CH1
0.45 ≤ r < 0.64 → ALS = 4.2785·CH0 - 1.9548·CH1
0.64 ≤ r < 0.85 → ALS = 0.5926·CH0 + 0.1185·CH1
r ≥ 0.85        → ALS = 0
```

### 8.8 `readMICData()`

Lê 24 bytes I²S, parseia 6 amostras 16-bit, calcula RMS:

```
MICData = sqrt( mean( s[i]² ) )
```

Retorna `uint64_t` (apesar do header dizer `int16_t`).

### 8.9 Acelerômetro/Gestos

Task `gesture_task` decodifica registrador `0x35` do SC7A20H em `Gesture`:

| `buf[0] & mask` | Gesto |
|-----------------|-------|
| `0x60` | `ScreenUp` |
| `0x50` | `ScreenDown` |
| `0x41` | `TiltLeft` |
| `0x42` | `TiltRight` |
| `0x44` | `TiltForward` |
| `0x48` | `TiltBack` |
| qualquer ≠0 | `Shake` |

`isGesture(g)` é "consumível": testa, retorna true e zera para `GestureNone`.
Aceleração: lê 6 bytes de `0xA8` quando `0x27 & 0x0F == 0x0F`, monta valores
12-bit signed por eixo (`>>4`, two's-complement).

### 8.10 QR-Code de display

`canvasDrawCode(str)`:
- Cria `lv_qrcode_create` 230 px (no canvas ou na câmera, dependendo se
  background da câmera está ativo).
- `lv_qrcode_update` + `lv_obj_center`.

`clearCode()`: `lv_qrcode_delete`.

### 8.11 Tabela de tarefas FreeRTOS criadas

| Task | Stack | Prio | Core | Função |
|------|-------|------|------|--------|
| `gesture_task` | 2 KB | 5 | 1 | Lê SC7A20H |
| `aht20` | 4 KB | 2 | 1 | Mede AHT20 |
| `buttonX` | 8 KB | 5 | 1 | Polling de botão |
| `cameDisPlayTask` | 4 KB | 5 | 1 | Renderiza frame da câmera |
| `taskLoop` (Music) | 2 KB | 5 | core ARDUINO | Toca melodia |
| `taskMusic` (WAV) | 4 KB | 5 | core ARDUINO | Toca WAV do SD |
| Tarefas AI (`switchAiMode`, `event`, `task_read_ai_data`) | 4 KB | 4-5 | 0/1 | Ver `11-lib-AIRecognition.md` |

## 9. Inconsistências/notas para reimplementação

1. `readMICData` declarado como `int16_t` no header, retorna `uint64_t` no .cpp.
2. `readUVS()` aparece no README/PDF mas está comentado no header.
3. `creatCanvas` (sic) — typo presente na API pública, não corrigir para manter
   compatibilidade.
4. `Music::playMusic` com opção `Once` ou `Forever` é **bloqueante** (loop
   `vTaskDelay` até o fim); apenas `*InBackground` retorna imediatamente.
5. A API `playMusic` do `unihiker_k10` (declarada no `UNIHIKER_K10`) não
   aparece implementada — o usuário sempre instancia `Music music;` à parte
   (ver exemplos).
