# 13 — Sensores e periféricos on-board

## 1. AHT20 — Temperatura e umidade

Driver: [external_packages/framework-arduinounihiker/libraries/DFRobot_AHT20/DFRobot_AHT20.h](../external_packages/framework-arduinounihiker/libraries/DFRobot_AHT20/DFRobot_AHT20.h)

I²C `0x38`. Faixas: -40…85 °C (±0.3 °C); 0…100 %RH (±2 %).

```cpp
class DFRobot_AHT20 {
public:
  DFRobot_AHT20(TwoWire &wire = Wire);
  uint8_t begin();                           // 0=ok, 2=not found, 3=init fail
  void    reset();
  bool    startMeasurementReady(bool crcEn = false);
  float   getTemperature_F();
  float   getTemperature_C();
  float   getHumidity_RH();
};
```

Comandos do chip (datasheet AHT20):
- `0xBA` soft reset.
- `0xBE 0x08 0x00` initialize.
- `0xAC 0x33 0x00` trigger measurement.
- Status byte: bit 7 = busy, bit 3 = calibrated.

Wrapper na K10 (`AHT20` em `unihiker_k10.h`) cria task FreeRTOS que chama
`startMeasurementReady(true)` a cada 1 s; `getData(eAHT20Data_t)` apenas lê
o último valor.

## 2. LTR-303ALS — Luz ambiente

I²C `0x29`. Dois canais (CH0=visível+IR, CH1=IR). Lido em
`UNIHIKER_K10::readALS()` — fórmula 4-faixas no documento principal.

Registradores usados:

| Reg | Significado | Valor escrito |
|-----|-------------|---------------|
| `0x80` (CTRL) | gain + active mode | `0x01` (gain ×1, active) |
| `0x85` (MEAS_RATE) | integration / measure | `0x03` (100 ms / 500 ms) |
| `0x88-0x8B` | CH1_0/CH1_1/CH0_0/CH0_1 (LSB/MSB) | leitura |
| `0x8C` (STATUS) | data valid | leitura |

## 3. SC7A20H — Acelerômetro 3-eixos

I²C `0x19` (SDO=HIGH). Compatível STMicro LIS2DH em registradores básicos.

Sequência de inicialização em `UNIHIKER_K10::initSC7A20H()`:

```text
0x24 |= 0x08                ; reset/CTRL_REG5
0x30 |= 0x40 | 0x03 | 0x0c | 0x38   ; INT1_CFG (AOI1 6D)
0x21 |= 0x81                ; CTRL_REG2 (HPF)
0x32  = 0x60                ; INT1_THS
0x33  = 0x02                ; INT1_DURATION
0x21 |= 0x40                ; CTRL_REG2
0x24 |= 0x02                ; CTRL_REG5
0x25 |= 0x02                ; CTRL_REG6
0x34  = 0xc0 | 0x3f         ; INT2_CFG (AOI2 6D pos+neg)
0x21 |= 0xfd                ; CTRL_REG2
0x36  = 0x18                ; INT2_THS
0x37  = 0x02                ; INT2_DURATION
0x25 |= 0x20                ; CTRL_REG6
0x23  = 0x80 | 0x08         ; CTRL_REG4 (BDU + HR)
0x20  = 0x27                ; CTRL_REG1 (ODR=10 Hz, XYZ enabled)
```

Leitura (no `gesture_task`):

- `0x27` (STATUS_REG): bit `0x0F` indica novos dados XYZ.
- `0xA8` (OUT_X_L|0x80, auto-increment): 6 bytes; cada eixo é `(MSB<<8|LSB)>>4`,
  signed 12-bit (two's-complement em 0x800).
- `0x35` (INT2_SRC): retorna flags 6D — usado para gesture map.

## 4. Microfones MEMS (MSM381ACT001 ×2) + ES7243EU8

Saída digital I²S RX. Configuração padrão do driver:

```c
i2s_config_t i2s_config = {
  .mode               = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX,
  .sample_rate        = 16000,
  .bits_per_sample    = 16,
  .channel_format     = I2S_CHANNEL_FMT_RIGHT_LEFT,
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .intr_alloc_flags   = ESP_INTR_FLAG_LEVEL2,
  .dma_buf_count      = 3,
  .dma_buf_len        = 300,
  .use_apll           = false,
  .tx_desc_auto_clear = true,
  .fixed_mclk         = 0,
  .mclk_multiple      = I2S_MCLK_MULTIPLE_DEFAULT,
  .bits_per_chan      = I2S_BITS_PER_CHAN_16BIT,
};
i2s_pin_config_t pin_config = {
  .mck_io_num   = IIS_MCLK,    // 3
  .bck_io_num   = IIS_BLCK,    // 0
  .ws_io_num    = IIS_LRCK,    // 38
  .data_out_num = IIS_DOUT,    // 45
  .data_in_num  = IIS_DSIN,    // 39
};
i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
i2s_set_pin(I2S_NUM_0, &pin_config);
```

`UNIHIKER_K10::readMICData()` lê 24 bytes (~6 amostras) e calcula RMS.

## 5. NS4168 — Amplificador class-D

Controle de ganho/shutdown via pino `eAmp_Gain` (XL9535). Receber `1` →
amp ativo (audível); `0` → silencioso. O firmware liga durante
`photoSaveToTFCard` (motivo elétrico — há ruído eletromagnético; a função
faz `digital_write(eAmp_Gain, 1)` no início e `0` no fim).

## 6. GC2145 — Câmera 2 MP

Inicializada por `register_camera()` da DFRobot (camada precompilada
`who_camera`). A API Arduino expõe apenas:

```cpp
k10.initBgCamerImage();    // configura câmera + display
k10.setBgCamerImage(true); // exibe/oculta
k10.photoSaveToTFCard(path);
```

Resolução fixa em QVGA (320×240) RGB565, 2 frame buffers.

## 7. Font chip GT30L24A3W

Pinos SPI: `MOSI3/MISO3/SCLK3` + `CS1` (independente do SD).

API exposta por `GT30L24A3W.h`:

```c
int           GT_Font_Init(void);
unsigned char ASCII_GetData(unsigned char asc, unsigned long ascii_kind, unsigned char *DZ_Data);
void          gt_12_GetData(unsigned char MSB, unsigned char LSB, unsigned char *DZ_Data);
unsigned long GBK_24_GetData(unsigned char c1, unsigned char c2, unsigned char *DZ_Data);
unsigned long U2G(unsigned int unicode);             // Unicode → GBK
unsigned char ASCII_GetInterval(unsigned char asc, unsigned long ascii_kind);
```

`ascii_kind` cobre 5×7, 7×8, 6×12, 12×16 (`ASCII_12_A`), 8×16, 12×24,
16×32, 24×32… O wrapper LVGL em `unihiker_k10.cpp` usa apenas
`ASCII_12_A`+`gt_12_GetData` (16 px) e `ASCII_24_B`+`GBK_24_GetData` (24 px).

Buffer estático de glyph: `static unsigned char _pBits[136]` (suficiente
para 24×24 com bpp=1 ⇒ 72 bytes; 16×16 ⇒ 32 bytes; 16 px alto × 12 wide
mono ⇒ 24 bytes etc.).

## 8. WS2812 RGB

Driver: `Adafruit_NeoPixel`. Tipo `NEO_GRB + NEO_KHZ800`. 3 LEDs no GPIO 46.

Wrapper `RGB` na K10 expõe brightness 0..9 (mapeado para 0..255).

## 9. ILI9341 — LCD 2.8" 240×320

Driver: `TFT_eSPI` (ver `21-lib-graphics.md`). Pinos definidos no
`User_Setup_Select.h` para K10. Backlight em `eLCD_BLK` (XL9535).

## 10. XL9535 — Expansor I²C

Endereço `0x20`. Veja `02-edge-connector.md`.

A camada DFRobot (em `tools/sdk/.../initBoard.h`) abstrai como `digital_read`
/`digital_write` sobre `ePin_t`. Internamente:

- Registradores P0/P1 (entrada), P2/P3 (saída), P4/P5 (polaridade), P6/P7
  (direção).
- `init_board()` configura direções para LCD_BLK, KeyA, KeyB, etc.
