# 14 — `initBoard.h` e gerenciamento de IO

Arquivo (somente header — implementação precompilada): [external_packages/framework-arduinounihiker/tools/sdk/esp32s3/include/modules/board/initBoard.h](../external_packages/framework-arduinounihiker/tools/sdk/esp32s3/include/modules/board/initBoard.h).

## 1. API C

```c
typedef enum {
    eLCD_BLK,
    eCamera_rst,
    eP11_KeyB,
    eP12, eP13, eP14, eP15,
    eP2,
    eP8, eP9, eP10,
    eP6,
    eP5_KeyA,
    eP4,
    eP3,
    eAmp_Gain
} ePin_t;

esp_err_t init_board(void);
void      digital_write(ePin_t pin, uint8_t state);
uint8_t   digital_read(ePin_t pin);
```

`init_board()` é chamado uma vez por `UNIHIKER_K10::begin()` e:

- Inicializa `i2c_bus` (driver Espressif IDF) na porta `I2C_NUM_0` em P19/P20
  com pull-ups internos disabled (já tem 10 kΩ externos).
- Configura XL9535 (`0x20`) — direção/output state default.
- Liga rails da câmera (1V8 e 2V8 via EN dos LDOs `U6`, `U7`).

## 2. Constantes adicionais (definidas no header)

```c
#define INTR_POSEDGE  0
#define INTR_NEGEDGE  1
#define INTR_ANYEDGE  2
#define OPENDRAIN     0
#define PUSHPULL      1
```

`interruptFunc` é definido para suportar interrupções por pino (a função
declarada `digitalInterrupt` está comentada no header — provavelmente reservada
para versão futura do SDK).

## 3. Mapeamento `ePin_t` ↔ XL9535 (deduzido)

A correspondência exata mora em `init_board.c` (precompilado), mas o
esquemático fornece pistas:

| ePin | Sinal hardware |
|------|----------------|
| `eLCD_BLK`     | Controle de backlight (Q4 → MMBT3904T) |
| `eCamera_rst`  | RESET da câmera GC2145 |
| `eP11_KeyB`    | Botão B (`KeyB`, K4) |
| `eP12..eP15`   | Edge connector P12–P15 |
| `eP2`          | Edge connector P2 |
| `eP8..eP10`    | Edge connector P8–P10 |
| `eP6`          | Edge connector P6/Buzz |
| `eP5_KeyA`     | Botão A (`KeyA`, K3) |
| `eP4`          | Edge connector P4/A4/LIGHT |
| `eP3`          | Edge connector P3/A3/EXT |
| `eAmp_Gain`    | NS4168 CTRL |

Pinos `P0` e `P1` (GPIOs nativos do ESP32-S3) **não** estão neste enum:
usam-se `pinMode(P0,...)` / `digitalRead(P1)` da API Arduino padrão.

## 4. Reimplementação

Para portar fora do framework:

```c
static const xl9535_map_t map[] = {
  [eLCD_BLK]    = { .port=0, .bit=X },
  [eCamera_rst] = { .port=Y, .bit=Z },
  ...
};

void digital_write(ePin_t pin, uint8_t state) {
  xl9535_set(&xl, map[pin].port, map[pin].bit, state);
}
```

Chip XL9535 documentação: 24-pin QFN, registradores 0x00=Input0, 0x02=Output0,
0x04=Polarity0, 0x06=Config0 (1=input). Implementar wrapper minimalista é
trivial via `Wire`.
