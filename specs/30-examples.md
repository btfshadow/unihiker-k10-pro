# 30 — Catálogo dos exemplos `unihiker_k10`

Os 30 sketches em [external_packages/framework-arduinounihiker/libraries/unihiker_k10/examples/](../external_packages/framework-arduinounihiker/libraries/unihiker_k10/examples/)
exercitam a totalidade da API pública. A tabela mostra cada exemplo, os
métodos chamados e a feature demonstrada (corroborado pelo
`Arduino IDE_Platform IO Examples - UNIHIKER Documentation.pdf`).

| Exemplo | APIs / Classes | Feature |
|---------|----------------|---------|
| `BackGround` | `setScreenBackground(0xRRGGBB)` | Cor sólida de fundo |
| `Text` | `canvas->canvasText(text, row, color)` | Texto por linha |
| `TextonImage` | `canvasDrawImage` + `canvasText(x,y,…)` | Texto sobre imagem do SD |
| `Dot` | `canvasPoint(x,y,color)` | Pontos randômicos |
| `Line` | `canvasSetLineWidth`, `canvasLine` | Linhas (funnel/meteoros) |
| `Circle` | `canvasCircle(x,y,r,c,bg,fill)` | Círculos preenchidos |
| `Rectangle` | `canvasRectangle(x,y,w,h,c,bg,fill)` | Retângulos |
| `qrCode` | `canvasDrawCode(str)` | QR display |
| `Locallmage` | `canvasDrawImage(0,0,"S:/file.bmp")` | Imagem do SD |
| `Photo` | `initBgCamerImage`, `setBgCamerImage`, `photoSaveToTFCard` | Câmera + foto BMP |
| `Button` | `buttonA->isPressed()`, `setPressedCallback` | Botões A/B/AB |
| `ButtonInterrupt` | `setPressedCallback` para A, B, AB | Callbacks |
| `AnalogInput` | `analogRead(P1)`, `analogWrite(P0,…)` | A/D, PWM em P0/P1 |
| `DigitalInput` | `pinMode(P0,…)`, `digitalRead(P1)` | GPIO nativo |
| `DigitalOutput` | `digital_write(eP2,HIGH)`, `digital_read(eP3)` | GPIO via XL9535 |
| `PWM` | `analogWrite(P0,…)` | PWM K10 |
| `Pedometer` | `getStrength()` | Contador de passos |
| `AccelerateBall` | `getAccelerometerX/Y` | "Bola" gravidade |
| `LED` | `rgb->write(idx, color)`, `rgb->brightness(0..9)`, `setRangeColor` | RGB on-board |
| `PlayBuildinMusic` | `Music`, `playMusic(BIRTHDAY)`, `playTone`, `stopPlayTone` | Áudio built-in |
| `TFCardMusic` | `playTFCardAudio`, `recordSaveToTFCard`, `stopPlayAudio` | Gravar/tocar WAV |
| `FaceDetect` | `AIRecognition::initAi`, `switchAiMode(Face)`, `getFaceData(...)` | Detecção de rosto |
| `FaceRecognition` | `sendFaceCmd(ENROLL/RECOGNIZE)`, `isRecognized()`, `getRecognitionID()` | Reconhecimento |
| `DogCatFaceDetect` | `switchAiMode(Cat)`, `getCatData(...)` | Cat/dog faces |
| `MoveDetect` | `switchAiMode(Move)`, `setMotinoThreshold(50)`, `isDetectContent(Move)` | Diferença de quadros |
| `qrCodeScan` | `switchAiMode(Code)`, `getQrCodeContent()` | QR scanner via câmera |
| `SpeechRecognitionCN` | `ASR::asrInit(CONTINUOUS, CN_MODE, 6000)`, `addASRCommand(id, "…")`, `isWakeUp`, `isDetectCmdID` | Comandos de voz CN |
| `SpeechRecognitionEN` | idem com `EN_MODE` | Comandos de voz EN |
| `SpeechSynthesisCN` | `ASR::setAsrSpeed`, `ASR::speak("...")` | TTS Mandarin |
| `UartReceive`/`UartSend` | `Serial.begin/print/read` | UART tradicional |

## Pinout dos exemplos GPIO (lembrete)

- `P0` e `P1` — GPIOs nativos do ESP32-S3 (analog/PWM).
- `eP2`, `eP3`, `eP4`, `eP6`, `eP8`–`eP15` — apenas digital, via XL9535.
- `eP5_KeyA`, `eP11_KeyB` — botões on-board, conflitam com edge connector.

## Padrões observados nos exemplos

1. Sempre começa com `k10.begin()`.
2. Para usar tela: `k10.initScreen(2)` + `k10.creatCanvas()`.
3. Para câmera: `initBgCamerImage()` antes de `setBgCamerImage()` e
   `creatCanvas()`.
4. Para AI: ordem específica `initAi()` → `initBgCamerImage()` →
   `setBgCamerImage(false)` → `creatCanvas()` → `switchAiMode(NoMode)` →
   `setBgCamerImage(true)` → `switchAiMode(<modo desejado>)`.
5. Para áudio: instanciar `Music music;` separado de `k10`.
6. Para SD: `k10.initSDFile()` antes de qualquer path `S:/`.
7. Para ASR: aguardar `while (asr._asrState == 0) delay(100);` antes de
   `addASRCommand`.
