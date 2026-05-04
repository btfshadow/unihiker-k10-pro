#pragma once

#include "../hal/hal_interfaces.h"

#include <Arduino.h>
#include <AIRecognition.h>
#include <esp_camera.h>
#include <unihiker_k10.h>

namespace unihiker_pro {

class DisplayService {
 public:
  explicit DisplayService(IBoardHal &hal)
      : hal_(hal), font_(Canvas::eCNAndENFont24) {}

  // Background / session
  Status setBackground(uint32_t color);
  Status createCanvas();
  Status destroyCanvas();
  Status setCameraBackground(bool enabled);

  // Canvas primitives
  Status clearCanvas();
  Status clearRow(uint8_t row);
  Status clearRegion(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  Status setLineWidth(uint8_t w);
  Status setFontSize(Canvas::eFontSize_t font);
  Status drawPoint(int16_t x, int16_t y, uint32_t color);
  Status drawLine(int x1, int y1, int x2, int y2, uint32_t color);
  Status drawCircle(int x, int y, int r, uint32_t color,
                    uint32_t bgColor = 0xFFFFFF, bool fill = false);
  Status drawRect(int x, int y, int w, int h, uint32_t color,
                  uint32_t bgColor = 0xFFFFFF, bool fill = false);
  Status drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                    const uint8_t *bitmap);
  Status drawImage(int16_t x, int16_t y, const String &path);

  // Text
  Status textRow(const String &text, uint8_t row, uint32_t color = 0x000000);
  Status textAt(const String &text, int16_t x, int16_t y, uint32_t color = 0x000000,
                int count = 20, bool autoClean = true);

  Status update();

 private:
  IBoardHal &hal_;
  Canvas::eFontSize_t font_;
};

class InputService {
 public:
  explicit InputService(IBoardHal &hal) : hal_(hal) {}

  bool buttonAPressed();
  bool buttonBPressed();
  bool buttonABPressed();
  bool pressed(ButtonId button);
  Status onPress(ButtonId button, ButtonCallback callback);
  Status onRelease(ButtonId button, ButtonCallback callback);

 private:
  IBoardHal &hal_;
};

class LedService {
 public:
  explicit LedService(IBoardHal &hal) : hal_(hal) {}

  Status setRgb(int8_t index, const RgbColor &color);
  Status setAll(const RgbColor &color);
  Status off();
  Status setBrightness(uint8_t level);
  Status setBacklight(bool enabled);
  Status setAmplifierGain(bool enabled);

 private:
  IBoardHal &hal_;
};

class PinService {
 public:
  explicit PinService(IBoardHal &hal) : hal_(hal) {}

  Status write(BoardPin pin, bool level);
  bool read(BoardPin pin);

 private:
  IBoardHal &hal_;
};

class SensorService {
 public:
  explicit SensorService(IBoardHal &hal) : hal_(hal) {}

  float temperatureC();
  float humidityRh();
  uint16_t ambientLux();
  int accelX();
  int accelY();
  int accelZ();
  uint64_t micLevel();

 private:
  IBoardHal &hal_;
};

// Callback de progresso: recebe valor 0–100 (porcentagem).
using ProgressCallback = void (*)(uint8_t percent);

class CameraService {
 public:
  explicit CameraService(IBoardHal &hal) : hal_(hal), previewActive_(false) {}

  // Inicia pipeline de câmera e exibe preview no fundo da tela.
  Status initPreview();

  // Liga/desliga o preview de câmera no fundo da tela.
  Status showPreview(bool enabled);

  // Captura na resolução de preview (QVGA 320x240) — rápido.
  // Path deve usar prefixo "S:/" (ex: "S:/photo.bmp").
  Status capture(const String &path);

  // Captura em alta resolução com escrita direta no SD e callback de progresso.
  // onProgress(0..100) é chamado a cada linha gravada.
  // framesize: FRAMESIZE_UXGA (1600x1200), FRAMESIZE_SXGA (1280x1024), etc.
  Status captureHiRes(const String &path,
                      framesize_t framesize = FRAMESIZE_UXGA,
                      ProgressCallback onProgress = nullptr);

  bool isPreviewActive() const { return previewActive_; }

 private:
  // Grava BMP RGB565 direto no SD com notificações de progresso por linha.
  Status writeBmpToSd(const String &path, camera_fb_t *fb,
                      ProgressCallback onProgress);

  IBoardHal &hal_;
  bool previewActive_;
};

class StorageService {
 public:
  explicit StorageService(IBoardHal &hal) : hal_(hal) {}

  Status initSd();
  Status savePhotoBmp(const String &path);

 private:
  IBoardHal &hal_;
};

class AudioService {
 public:
  AudioService(IBoardHal &boardHal, IAudioHal &audioHal)
      : boardHal_(boardHal), audioHal_(audioHal) {}

  Status playBuiltIn(Melodies melody, MelodyOptions options = OnceInBackground);
  Status stopBuiltIn();
  Status playFile(const String &path);
  Status stopFile();
  Status recordFile(const String &path, uint8_t seconds);

 private:
  IBoardHal &boardHal_;
  IAudioHal &audioHal_;
};

class VisionService {
 public:
  VisionService(IBoardHal &boardHal, IVisionHal &visionHal)
      : boardHal_(boardHal), visionHal_(visionHal), currentMode_(AiMode::None) {}

  Status init();
  Status setMode(AiMode mode);
  bool detected();
  int faceData(AIRecognition::eFaceOrCatData_t type);
  int catData(AIRecognition::eFaceOrCatData_t type);
  String qrPayload();

 private:
  IBoardHal &boardHal_;
  IVisionHal &visionHal_;
  AiMode currentMode_;
};

class SpeechService {
 public:
  explicit SpeechService(ISpeechHal &speechHal) : speechHal_(speechHal) {}

  void begin(uint8_t mode = 0, uint8_t lang = 0, uint16_t wakeUpMs = 6000);
  void addCommand(uint8_t id, const String &phrase);
  bool detectCommand(uint8_t id);
  void speak(const String &text);

 private:
  ISpeechHal &speechHal_;
};

}  // namespace unihiker_pro
