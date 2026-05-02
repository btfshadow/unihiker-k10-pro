# 11 — Biblioteca `AIRecognition`

Pasta: [external_packages/framework-arduinounihiker/libraries/AIRecognition/](../external_packages/framework-arduinounihiker/libraries/AIRecognition/)

Arquivos:
- [AIRecognition.h](../external_packages/framework-arduinounihiker/libraries/AIRecognition/AIRecognition.h)
- [AIRecognition.cpp](../external_packages/framework-arduinounihiker/libraries/AIRecognition/AIRecognition.cpp)

Depende de:
- `who_ai_utils.hpp` / `ai.hpp` — camada precompilada DFRobot
  (em `tools/sdk/esp32s3/include/modules/...`).
- `unihiker_k10` (precisa de canvas, câmera, queue compartilhada `xQueueCamer`).
- ESP-DL (Espressif Deep Learning library) — modelos de face detection,
  recognition, motion detection embarcados em flash.
- `esp_code_scanner` para QR codes.

## 1. Tipos públicos

```cpp
class AIRecognition {
public:
  enum eAiType_t { Face, Cat, Move, Code, NoMode };

  enum eFaceOrCatData_t {
    Length, Width,
    CenterX, CenterY,
    LeftEyeX, LeftEyeY,
    RightEyeX, RightEyeY,
    NoseX, NoseY,
    LeftMouthX, LeftMouthY,
    RightMouthX, RightMouthY,
  };
};
```

`recognizer_state_t` vem de `who_ai_utils.hpp` (camada interna):

```cpp
typedef enum { ENROLL, RECOGNIZE, DELETEALL, /* DELETE com id */ } recognizer_state_t;
```

Estruturas internas (do header DFRobot interno):

```cpp
typedef struct {
    int16_t faceFrameLength;
    int16_t faceFrameWidth;
    int16_t leftEye[2];
    int16_t rightEye[2];
    int16_t nose[2];
    int16_t leftMouth[2];
    int16_t rightMouth[2];
} sFaceData_t;

typedef struct {
    int16_t catFrameLength;
    int16_t catFrameWidth;
} sCatData_t;

typedef struct {
    bool is_faceok;
    bool is_catok;
    bool is_codeok;
    bool is_moved;
    char *codeData;
    std::list<dl::detect::result_t> *is_detectResults;
} sAIRead_t;

typedef struct {
    int cmd;     // ENROLL / RECOGNIZE / DELETEALL
    int id;      // alvo p/ DELETE
} sAISet_t;
```

## 2. API pública

```cpp
void initAi(void);
void switchAiMode(eAiType_t mode);          // Face/Cat/Move/Code/NoMode

int  getFaceData(eFaceOrCatData_t type);    // -1 se inválido
int  getCatData(eFaceOrCatData_t type);
bool isDetectContent(eAiType_t mode);

void   sendFaceCmd(recognizer_state_t cmd); // ENROLL/RECOGNIZE/DELETEALL
void   sendFaceCmd(uint8_t cmd, int id);    // delete específico

void   setMotinoThreshold(uint8_t threshold); // 10..200, internamente: _sensitivity = (200 - threshold)
String getQrCodeContent(void);
int    getRecognitionID(void);              // -1 se não há nova recognição
bool   isRecognized(void);                  // edge: true uma vez por face match
```

## 3. Modelo de execução

### 3.1 `initAi()`

Cria três queues:

| Queue | Tamanho | Item |
|-------|---------|------|
| `xQueueCamer` | 20 | `camera_fb_t*` (câmera → display) |
| `xQueueAI`    | 20 | `camera_fb_t*` (câmera → IA) |
| `xQueueResult`| 20 | `sAIRead_t*` (IA → reader) |

E uma task `task_read_ai_data` (4 KB, prio 5, core 0) que consome
`xQueueResult` e popula um `sAIData_t* aiGetData` em PSRAM,
protegido por `readDataMutex`.

### 3.2 `switchAiMode(mode)`

Para qualquer task de modo anterior (notify + espera de `isTaskRunning=false`),
depois cria a task correspondente, todas em core 1 com 4 KB stack,
recebendo `xQueueCamer` como argumento:

| Modo | Task | Função interna |
|------|------|----------------|
| `Face` | `task_face_recognition` (prio 5) + `task_event_handler` (prio 2) | Pipeline detection→recognition + event consumer (ENROLL/RECOGNIZE) |
| `Cat`  | `task_cat_recognition` | Detecção cat/dog face |
| `Move` | `task_motion_recognition` | Diferença de quadros, threshold via `_sensitivity` |
| `Code` | `task_code_recognition` | `esp_code_scanner` |
| `NoMode` | `task_nomode_handler` | Apenas drena buffers |

> Todas essas tasks vivem em código precompilado (DFRobot) e são "públicas"
> apenas como símbolos. O contrato com o usuário é via queues e a
> struct `sAIData_t`.

### 3.3 Comandos de face

Para reconhecimento, uma queue `xQueueEvent` (4 slots de `sAIRead_t*`) envia
`sAISet_t*` (alocado em PSRAM) ao `task_event_handler`. Esse handler:

- `ENROLL`: aprende a face atual e atribui novo ID; sinaliza variáveis externas
  `_recognize` (uint8) e `_faceID` (int).
- `RECOGNIZE`: classifica face atual contra DB; popula `_faceID`.
- `DELETEALL`: zera DB.
- `DELETE` por `id` (sobrecarga `sendFaceCmd(uint8_t, int)`): remove um ID.
  *(Na implementação atual, qualquer id passa cmd como RECOGNIZE — possível
  bug.)*

### 3.4 Sensibilidade de movimento

```cpp
void setMotinoThreshold(uint8_t threshold) {
  _sensitivity = (200 - threshold);   // var. global em ai.hpp
}
```

Faixa documentada: 10..200. Quanto maior `threshold`, menor `_sensitivity` →
menos sensível.

## 4. Fluxo típico (ver `30-examples.md`)

```cpp
k10.begin();
k10.initScreen(2);
ai.initAi();             // cria queues + task read
k10.initBgCamerImage();  // configura câmera + xQueueCamer
k10.setBgCamerImage(false);
k10.creatCanvas();
ai.switchAiMode(ai.NoMode);
k10.setBgCamerImage(true);
ai.switchAiMode(ai.Face);
```

A ordem importa porque `initBgCamerImage` cria `xQueueCamer` que `switchAiMode`
precisa. Trocar de modo "consome" e recria as task; a câmera continua
streamando para `xQueueAI` e `xQueueCamer` (multicast feito pelo driver
`who_camera`).

## 5. Notas para reimplementação

1. **Modelos**: ESP-DL pirando neural networks fornecidos pela Espressif:
   `face_detection` (MTMN/MNP), `face_recognition`, `cat_face_detection`. As
   blobs ficam em `tools/sdk/esp32s3/lib` ou `tools/partitions`.
2. **Pipeline câmera**: PIXFORMAT_RGB565, FRAMESIZE_QVGA (320×240). A IA
   consome `camera_fb_t` direto.
3. **Persistência**: o DB de faces persiste em flash (`Preferences` ou NVS) —
   verificar `who_ai_utils.cpp` (não fornecido em código aberto neste pacote).
4. **QR-Code**: `task_code_recognition` chama `esp_code_scanner_scan_image`
   (`esp_code_scanner.h` em `lvgl_qrcode` lib ou módulo separado).
   Conteúdo decodificado vai como `char*` em `aiData->codeData` e é copiado
   para `aiGetData->_codeData` (até 200 bytes).
5. Para reimplementar sem DFRobot: portar usando ESP-WHO (Espressif) +
   ESP-DL + `esp32-camera` + ESP-IDF. As primitivas existem; a "cola" é o que
   essa lib faz (queues + tasks + struct compartilhada + LVGL).
