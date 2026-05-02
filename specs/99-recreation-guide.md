# 99 — Roteiro para reimplementar as bibliotecas/APIs do K10

Este documento amarra todas as especificações em uma ordem prática de
reimplementação. Cobre tanto **port para outro toolchain (ESP-IDF v5 puro,
PlatformIO + Arduino-ESP32 oficial, MicroPython, Rust embassy)** quanto
**limpeza/refatoração mantendo o mesmo SDK**.

## 0. Inventário do que precisa ser recriado

| Camada | Status no repo | Esforço de recriação |
|--------|----------------|----------------------|
| `unihiker_k10` (Canvas, RGB, Button, AHT20-wrapper, Music, UNIHIKER_K10) | **Código-fonte aberto** (`libraries/unihiker_k10/`) | Médio — usa LVGL+TFT_eSPI direto |
| `AIRecognition` API + `task_read_ai_data` | **Código-fonte aberto** | Baixo (cola) |
| `task_face_recognition`, `task_cat_recognition`, `task_motion_recognition`, `task_code_recognition`, `task_event_handler` | **Precompilado** em `tools/sdk/esp32s3/lib` (símbolos em `who_*.a`, `ai_*.a`) | Alto — depende ESP-DL |
| `asr` + `DFRobot_ESPASR` implementação | **Precompilado** | Alto — depende esp-sr + esp_tts |
| `initBoard` (XL9535 + power) | **Precompilado** (header aberto) | Baixo — XL9535 é trivial |
| `who_camera`, `app_wifi`, `app_httpd`, `app_mdns` | **Precompilado** | Médio (são equivalentes ESP-WHO) |
| `GT30L24A3W` driver | **Código-fonte aberto** (`unihiker_k10/src/`) | Baixo |
| `esp_code_scanner` | **Precompilado** | Baixo (substituir por `quirc`) |
| Stack gráfica (LVGL, TFT_eSPI, Adafruit_NeoPixel) | **Open source upstream** | Apenas configurar |

## 1. Camada física → drivers HAL

Comece pelo "chão". Sem isso nada funciona:

1. **GPIO expansor XL9535** sobre I²C `0x20`.
   - Implementar `xl9535_init`, `xl9535_dir(port, mask)`, `xl9535_set/get`.
   - Reproduzir o enum `ePin_t` para manter compatibilidade-fonte.
2. **Power-up sequence** (do esquemático §8 e §7 do hardware):
   - Ligar LDOs câmera (1V8 + 2V8) — pinos EN do XL9535.
   - Backlight LCD via `eLCD_BLK`.
3. **I²C bus principal** em `P19/SCL`, `P20/SDA` (pull-ups externos 10 kΩ).
4. **Drivers I²C dos sensores**:
   - `AHT20` (`0x38`) — datasheet trivial.
   - `LTR-303ALS` (`0x29`) — fórmula 4-faixas em `13-lib-sensors.md`.
   - `SC7A20H` (`0x19`) — sequência exata de `initSC7A20H()` em `10-lib-unihiker_k10.md`.
5. **SPI3 + chip selects**:
   - SD via `SD.h`/`fatfs`.
   - GT30L24A3W (font chip) — usar implementação aberta em
     `unihiker_k10/src/DFRobot_GT30L24A3W.cpp`.
6. **I²S**:
   - Configuração-base no `13-lib-sensors.md §4`.
   - Mutex `xI2SMutex` para serializar `Music`, `recordSaveToTFCard`,
     `readMICData`, TTS.

## 2. Display + LVGL

1. Inicializar TFT_eSPI (ou substituto `esp_lcd_panel_ili9341`).
2. Registrar driver LVGL com flush para o painel.
3. Habilitar decodificadores BMP/PNG no `lv_conf.h`.
4. Registrar driver FATFS (`lv_fs_fatfs_init`) com letra `S:`.
5. Implementar fontes "virtuais" `my_custom_font_16/24` que delegam ao font chip.
6. Implementar a classe `Canvas` mapeando 1:1 com `lv_canvas_*`.

## 3. Câmera

1. Driver `esp32-camera` (Espressif) configurado para GC2145 → RGB565 QVGA.
2. Reproduzir a função `register_camera(format, size, fb_count, queue)` que
   alimenta uma `QueueHandle_t` de `camera_fb_t*`.
3. Multicast: a API original alimenta `xQueueCamer` E `xQueueAI`. Implementar
   simples broadcast (push para ambas as queues).
4. Task de display: consome de `xQueueCamer` → `lv_img_set_src`.

## 4. Áudio (`Music`)

1. Player `playTone` — gerar seno PCM 16-bit a 8 kHz, escrever em I²S.
2. Player `taskPlayMusic` — abrir WAV via `lv_fs_open` (ou `FATFS` direto),
   ler header, ajustar `i2s_set_sample_rates`, streaming 1024 bytes.
3. Recorder — header WAV PCM linear (ver `createWavHeader`), 16 kHz 16-bit
   stereo (byteRate = 64000).
4. Parser de melodias built-in: tabela `freqTable` + tokenizer
   `<note><octave?>:<duration>` separado por `|`.

## 5. Sensores de alto nível e tasks

1. Task `aht20` (1 Hz) — popula cache T/H.
2. Task `gesture_task` (10 Hz) — lê SC7A20H, popula `accX/Y/Z` + `_gesture`.
3. Task de botão (debounced polling) com callbacks `pressed`/`unpressed`.
4. WS2812 — `Adafruit_NeoPixel` ou `led_strip` (Espressif).

## 6. AI

Opção A — manter blob DFRobot:
- Compilar contra os `.a` precompilados (depende de a partição `model` estar
  preservada).

Opção B — recriar com Espressif open source:
- `ESP-WHO` para face detection (`task_face_recognition`).
- `ESP-DL` para feature embeddings (`task_event_handler` ENROLL/RECOGNIZE).
- Movimento: simples diff de luminância entre frames, threshold via
  `_sensitivity = 200 - threshold`.
- QR-code: substitua `esp_code_scanner` por `quirc`.

## 7. ASR/TTS

A maneira oficial e mantida hoje é a [esp-sr](https://github.com/espressif/esp-sr).

1. Adicionar como componente IDF.
2. Carregar partição `model` (custom-built com WakeNet + MultiNet + voz TTS).
3. Implementar `asrInit` reproduzindo o pipeline:
   - Criar AFE handler (`esp_afe_sr_v1.create`).
   - Loop: ler I²S, alimentar `feed`, ler `fetch`, despachar para WakeNet/
     MultiNet conforme estado.
   - Sinalizar `_wakeUp`, `_isDetectCmdID`.
4. `speak`: usar `esp_tts_voice_set_speed`, `esp_tts_parse_chinese`,
   sintetizar PCM e mandar via I²S TX (mutex `xI2SMutex`).

## 8. Compatibilidade-fonte (manter sketches existentes funcionando)

- Manter typo `creatCanvas`.
- Manter `readMICData` retornando RMS (mas com tipo correto `uint64_t`).
- Manter os enums com mesma ordem (`Melodies`, `Gesture`, `eAiType_t`,
  `eFaceOrCatData_t`, `recognizer_state_t`, `eAHT20Data_t`, `eFontSize_t`).
- Mover declarações de `playMusic`/`playTone` para a classe `Music` (já é o
  caso na implementação real).

## 9. Estratégia de testes incrementais

| Teste | Lib mínima necessária |
|-------|-----------------------|
| Print pelo Serial | Arduino-ESP32 base |
| Acender RGB | XL9535? não (RGB usa GPIO46 nativo). Apenas `Adafruit_NeoPixel`. |
| Acender backlight + tela branca | `init_board` + TFT_eSPI |
| Mostrar texto LVGL com fontes do font chip | TFT_eSPI + LVGL + GT30L24A3W |
| Botão A acender LED | `digital_read(eP5_KeyA)` |
| Ler AHT20 / LTR / SC7A20H | I²C |
| Microfone (RMS) | I²S RX |
| Tocar tom | I²S TX + amp gain |
| Tocar WAV do SD | SD + I²S TX |
| Foto BMP | Camera + SD |
| Display da câmera | Camera + LVGL img |
| Face detect | + ESP-DL |
| ASR | + esp-sr |

Siga essa ordem para validar cada degrau.

## 10. Alternativas de framework

| Alvo | Caminho |
|------|---------|
| **Arduino-ESP32 oficial (sem fork DFRobot)** | Substitua `framework-arduinounihiker` pelo `framework-arduinoespressif32` upstream. Adicione apenas `unihiker_k10/`, `AIRecognition/` reescritas, e configure `User_Setups/Setup_K10.h` para o TFT_eSPI. |
| **ESP-IDF v5 puro** | Use `esp_lcd_panel_*`, `esp32-camera`, `esp-sr`, `esp-dl`, `lvgl` componente IDF. Maior controle, sem camada Arduino. |
| **MicroPython** | Já existe port DFRobot para K10 — outro repositório. Útil como referência para semântica esperada. |
| **Rust embassy** | `esp-hal` ESP32-S3 + drivers próprios. Bastante trabalho mas viável para os 5 primeiros itens da seção 9. |

## 11. Checklist final

- [ ] Power & XL9535 OK (LED user e backlight liga).
- [ ] I²C scan reporta `0x19, 0x20, 0x29, 0x38` (+ `0x10..0x17` se ES7243).
- [ ] LCD desenha pixels.
- [ ] LVGL flush funciona; `Canvas` desenha `Hello`.
- [ ] Font chip retorna glyph não-vazio para `'A'` e `'你'`.
- [ ] Botões A, B, A+B detectados.
- [ ] AHT20 lê T/H plausíveis.
- [ ] SC7A20H lê acel ≈ (0,0,1024) parado.
- [ ] LTR303 lê ALS proporcional à luz.
- [ ] I²S TX gera 440 Hz audível.
- [ ] I²S RX retorna RMS > 0.
- [ ] SD monta `S:/` e `lv_fs_open` lê arquivo.
- [ ] Camera streama 30 fps QVGA RGB565.
- [ ] LVGL exibe câmera + canvas overlay.
- [ ] BMP de foto válido.
- [ ] WS2812 muda cor.
- [ ] QR-code rendering (`lv_qrcode`).
- [ ] QR-code scanning (quirc).
- [ ] Face detection (bounding box correto).
- [ ] Face recognition (ENROLL → RECOGNIZE retorna ID).
- [ ] ASR wake-word + comando aciona callback.
- [ ] TTS gera áudio inteligível.

Quando todos os itens passam, a recriação está funcional.
