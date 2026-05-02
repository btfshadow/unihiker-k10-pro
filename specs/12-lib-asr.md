# 12 — Biblioteca `asr` (Speech Recognition + TTS)

Pastas:
- [external_packages/framework-arduinounihiker/libraries/asr/](../external_packages/framework-arduinounihiker/libraries/asr/) — wrapper alto nível (header-only).
- [external_packages/framework-arduinounihiker/libraries/DFRobot_ESPASR/](../external_packages/framework-arduinounihiker/libraries/DFRobot_ESPASR/) — base ESP-SR.

> A `.cpp` da classe `ASR` (asrInit/addASRCommand/etc) **não está exposta em
> código-fonte** neste pacote. Ela vive precompilada dentro do SDK em
> `tools/sdk/esp32s3/lib`, junto com modelos WakeNet/MultiNet. Apenas a API
> pública e as constantes são abertas.

Dependências do header:

```cpp
#include "DFRobot_ESPASR.h"
#include "esp_tts.h"
#include "esp_process_sdkconfig.h"
#include "esp_wn_iface.h" / esp_wn_models.h
#include "esp_afe_sr_iface.h" / esp_afe_sr_models.h
#include "esp_mn_iface.h" / esp_mn_models.h
#include "model_path.h"
#include "esp_mn_speech_commands.h"
```

## 1. Constantes públicas

```c
// Modos
#define ONCE        0
#define CONTINUOUS  1

// Línguas
#define CN_MODE     0
#define EN_MODE     1
```

## 2. Classe base `DFRobot_ESPASR`

```cpp
class DFRobot_ESPASR {
public:
  DFRobot_ESPASR();

  /** mode: ONCE|CONTINUOUS, modeType: CN_MODE|EN_MODE
   *  wakeUpTime em ms (default 6000) */
  void initASR(uint8_t mode = 0,
               QueueHandle_t xQueueID = NULL,
               uint8_t modeType = 0,
               uint16_t wakeUpTime = 6000);

  void _addASRCommand(uint8_t id, char* data);

  uint8_t  _asrState = 0;          // 0 = inicializando, 1 = pronto
  uint16_t _wakeUpTime = 0;
  uint8_t  _modeType = 0;
  uint8_t  _recognitionMode = 0;
  srmodel_list_t   *models;        // ESP-SR
  esp_afe_sr_iface_t *afe_handle;
  bool _wakeUp = false;
};
```

## 3. Classe wrapper `ASR : public DFRobot_ESPASR`

```cpp
class ASR : public DFRobot_ESPASR {
public:
  enum eAsrSpeed_t { Fast, Medium, Slow };
  ASR();

  void asrInit(uint8_t mode = 0, uint8_t lang = 0, uint16_t wakeUpTime = 6000);
  void addASRCommand(uint8_t id, char* cmd);
  void addASRCommand(uint8_t id, String cmd);
  bool isWakeUp(void);
  bool isDetectCmdID(uint8_t id);

  void setAsrSpeed(uint8_t speed);   // 0..5
  void speak(String  prompt);
  void speak(const char *prompt);
  void speak(float   prompt);

  uint8_t _isDetectCmdID = 0;
  esp_tts_handle_t *tts_handle = NULL;
  QueueHandle_t     xQueueTTS = NULL;
};
```

### 3.1 Modelo de uso (de exemplos)

```cpp
asr.asrInit(CONTINUOUS, CN_MODE, 6000);
while (asr._asrState == 0) delay(100);     // espera modelos carregarem
asr.addASRCommand(1, "lights on");
asr.addASRCommand(2, "lights off");

// loop:
if (asr.isWakeUp()) { ... }
if (asr.isDetectCmdID(1)) { ... }
```

### 3.2 Comportamento documentado

| Função | Detalhes |
|--------|----------|
| `asrInit` | Carrega WakeNet + MultiNet conforme `lang`; inicia AFE (Acoustic Front-End: AGC, NS, AEC); `_asrState=1` quando pronto. Wake-word fixa: **"Hi, Telly"** (CN: "你好小智"-like). |
| `addASRCommand(id, cmd)` | Insere comando em `esp_mn_speech_commands` API. ID `0` reservado pelo wake — usuário começa em `1`. Strings em pinyin (CN) ou inglês fonético. |
| `isWakeUp` | Retorna `_wakeUp`; consome (pode resetar conforme política `ONCE`/`CONTINUOUS`). |
| `isDetectCmdID(id)` | Retorna true uma vez quando o comando bate (edge). |
| `setAsrSpeed(speed)` | Inicializa engine TTS Espressif com voz `voice_data` e velocidade [0..5]. |
| `speak(text)` | Envia texto à queue `xQueueTTS`; task de TTS gera PCM via `esp_tts_voice_*` e despeja em I²S TX. |

### 3.3 Modos `ONCE` × `CONTINUOUS`

- `ONCE`: após uma detecção de comando, requer wake-word novamente.
- `CONTINUOUS`: continua aceitando comandos por `wakeUpTime` ms após o wake.

## 4. Componentes ESP-SR usados (resumo)

| Componente | Propósito | Origem |
|------------|-----------|--------|
| `esp-sr/wn` (WakeNet 9) | Detecta wake-word | Espressif esp-sr |
| `esp-sr/afe` | AGC/NS/AEC sobre dual-mic I²S | Espressif esp-sr |
| `esp-sr/mn` (MultiNet 5/6) | Reconhecimento de comandos custom | Espressif esp-sr |
| `esp_tts` | Síntese chinês/inglês | Espressif esp-sr |
| `model_path` / `srmodel_list_t` | Lista de modelos em partição `model` | Espressif esp-sr |

> A partição `model` deve estar declarada em `partitions.csv` (ver
> `tools/partitions/` no framework-arduinounihiker).

## 5. Notas para reimplementação

1. Compilar contra ESP-IDF + esp-sr (não trivial fora do framework Arduino).
2. Mutex I²S (`xI2SMutex`) precisa ser tomado quando o TTS gerar áudio,
   senão colide com `Music`.
3. A maioria do trabalho está em definir os comandos no formato esperado
   pelo MultiNet (fonemas), o que difere entre CN e EN.
4. `isWakeUp` e `isDetectCmdID` são **edge-triggered**: chamar uma única vez
   por evento.
