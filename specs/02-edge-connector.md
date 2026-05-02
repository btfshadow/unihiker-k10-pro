# 02 — Edge-Connector e expansor XL9535

Conector de borda 40 vias compatível com micro:bit (`J9A` no esquemático).

## 1. Pinagem do edge connector

| # | Sinal | Origem | Função suportada |
|---|-------|--------|------------------|
| 1 | P3/A3/EXT | XL9535 | DigitalIO |
| 2-5 | P0/A0 | ESP32 nativo | DigitalIO + AnalogIn + PWM |
| 6 | P4/A4/LIGHT | XL9535 | DigitalIO |
| 7 | P5/BOOT0/BTN-A | XL9535 (`eP5_KeyA`) | Botão A (ativo-baixo) |
| 8 | P6/Buzz | XL9535 | DigitalIO |
| 9 | P7/PIXELS | (interno) | (RGB on-board) |
| 10-13 | P1/A1 | ESP32 nativo | DigitalIO + AnalogIn + PWM |
| 14 | P8 | XL9535 | DigitalIO |
| 15 | P9 | XL9535 | DigitalIO |
| 16 | P10/A10/SOUND | XL9535 | DigitalIO |
| 17 | P11/BOOT2/BTN-B | XL9535 (`eP11_KeyB`) | Botão B |
| 18 | P12 | XL9535 | DigitalIO |
| 19-22 | P2/A2 | XL9535 | DigitalIO |
| 23 | P13 | XL9535 | DigitalIO |
| 24 | P14 | XL9535 | DigitalIO |
| 25 | P15 | XL9535 | DigitalIO |
| 26 | P16 | XL9535 | DigitalIO |
| 27-32 | 3V3 | – | – |
| 33 | P19/SCL | ESP32 nativo | I²C |
| 34 | P20/SDA | ESP32 nativo | I²C |
| 35-40 | GND | – | – |

> Apenas **P0** e **P1** suportam analog/PWM nativos da API Arduino padrão.
> `analogRead(P1)`, `analogWrite(P0,…)`, `pinMode(P0,OUTPUT)` etc. funcionam
> diretamente porque mapeiam GPIOs reais do ESP32-S3.

## 2. Expansor XL9535QF24 (`U5`)

Endereço I²C **`0x20`** (A2=A1=A0=0). Possui 16 portas (P00–P17), interrupção
em `BUS_INT`. A SDK DFRobot mapeia parte dessas portas para o enum `ePin_t`
em [initBoard.h](../external_packages/framework-arduinounihiker/tools/sdk/esp32s3/include/modules/board/initBoard.h).

## 3. Enum `ePin_t` (API DFRobot)

```c
typedef enum {
    eLCD_BLK,
    eCamera_rst,
    eP11_KeyB,
    eP12,
    eP13,
    eP14,
    eP15,
    eP2,
    eP8,
    eP9,
    eP10,
    eP6,
    eP5_KeyA,
    eP4,
    eP3,
    eAmp_Gain
} ePin_t;
```

Helpers expostos pelo framework (camada C, dentro de
`tools/sdk/esp32s3/include/modules/board/`):

```c
esp_err_t init_board(void);                 // power chip + I²C + XL9535
void      digital_write(ePin_t pin, uint8_t state);
uint8_t   digital_read(ePin_t pin);
```

`init_board()` é chamado dentro de `UNIHIKER_K10::begin()` e é responsável por:

1. Inicializar barramento I²C interno `i2c_bus`.
2. Configurar XL9535 com direção/estado iniciais.
3. Habilitar rails de alimentação (LDOs câmera, etc.).

## 4. Macros de constantes adicionais (de `unihiker_k10.h`)

```c
#define MSA311_ADDR          0x62   // (legado, módulo MSA311 não usado na K10 final)
#define LTR303ALS_ADDR       0x29
#define LTR303ALS_STATUS     0x8C
#define LTR303ALS_CTRL       0x80
#define LTR303ALS_GAIN_MODE  0x01   // Gain×1, Active mode
#define LTR303ALS_MEAS_RATE  0x85
#define LTR303ALS_INTEG_RATE 0x03   // 100 ms integ, 500 ms measure
#define LTR303_DATA_CH1_0    0x88
#define LTR303_DATA_CH1_1    0x89
#define LTR303_DATA_CH0_0    0x8A
#define LTR303_DATA_CH0_1    0x8B
```
