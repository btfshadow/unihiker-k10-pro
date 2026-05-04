#pragma once

#include "../hal/hal_interfaces.h"

#include <Arduino.h>
#include <AIRecognition.h>
#include <esp_camera.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <unihiker_k10.h>

namespace unihiker_pro {

class DisplayService {
 public:
  explicit DisplayService(IBoardHal &hal)
      : hal_(hal),
        font_(Canvas::eCNAndENFont24),
        canvasMutex_(xSemaphoreCreateRecursiveMutex()),
        canvasSessionActive_(false),
        canvasSessionId_(0) {}

  DisplayService(const DisplayService &) = delete;
  DisplayService &operator=(const DisplayService &) = delete;

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
  Status lockCanvas(Canvas **canvasOut);
  void unlockCanvas();

  IBoardHal &hal_;
  Canvas::eFontSize_t font_;
  SemaphoreHandle_t canvasMutex_;
  bool canvasSessionActive_;
  uint32_t canvasSessionId_;
};

class InputService {
 public:
  static constexpr uint32_t kDefaultLongPressMs = 2000;

  explicit InputService(IBoardHal &hal) : hal_(hal) {}

  bool buttonAPressed();
  bool buttonBPressed();
  bool buttonABPressed();
  bool pressed(ButtonId button);
  Status onPress(ButtonId button, ButtonCallback callback);
  Status onRelease(ButtonId button, ButtonCallback callback);

  // Global homogeneous timing controller: action runs on release.
  // press < longPressMs => shortCallback, press >= longPressMs => longCallback.
  Status onReleaseByDuration(ButtonId button,
                             ButtonCallback shortCallback,
                             ButtonCallback longCallback,
                             uint32_t longPressMs = kDefaultLongPressMs);

 private:
  struct TimedBinding {
    bool enabled = false;
    uint32_t pressedAtMs = 0;
    uint32_t longPressMs = kDefaultLongPressMs;
    ButtonCallback shortCallback = nullptr;
    ButtonCallback longCallback = nullptr;
  };

  static InputService *activeTimedController_;

  static void onPressAThunk();
  static void onPressBThunk();
  static void onPressABThunk();
  static void onReleaseAThunk();
  static void onReleaseBThunk();
  static void onReleaseABThunk();

  void handleTimedPress(ButtonId button);
  void handleTimedRelease(ButtonId button);
  uint8_t buttonIndex(ButtonId button) const;

  IBoardHal &hal_;
  TimedBinding timedBindings_[3];
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

struct SensorCacheConfig {
  uint32_t environmentMs = 1000;
  uint32_t ambientMs = 500;
  uint32_t accelMs = 120;
  uint32_t micMs = 120;
};

struct SensorDiagnostics {
  bool boardReady = false;
  bool i2cBusReady = false;
  bool aht20Present = false;
  bool alsPresent = false;
  bool accelPresent = false;
  bool micAvailable = false;
};

class SensorService {
 public:
  explicit SensorService(IBoardHal &hal)
      : hal_(hal),
        cacheConfig_(),
        tempC_(0.0f),
        humidityRh_(0.0f),
        ambientLux_(0),
        accelX_(0),
        accelY_(0),
        accelZ_(0),
        micLevel_(0),
        envCached_(false),
        ambientCached_(false),
        accelCached_(false),
        micCached_(false),
        envLastMs_(0),
        ambientLastMs_(0),
        accelLastMs_(0),
        micLastMs_(0) {}

  Status setCacheConfig(const SensorCacheConfig &config);
  Status refreshEnvironment();
  Status refreshAmbient();
  Status refreshMotion();
  Status refreshMic();
  Status refreshAll();
  Status diagnose(SensorDiagnostics &out);

  float temperatureC();
  float humidityRh();
  uint16_t ambientLux();
  int accelX();
  int accelY();
  int accelZ();
  uint64_t micLevel();

 private:
  bool cacheExpired(uint32_t nowMs, uint32_t lastMs, uint32_t ttlMs) const;

  IBoardHal &hal_;
  SensorCacheConfig cacheConfig_;

  float tempC_;
  float humidityRh_;
  uint16_t ambientLux_;
  int accelX_;
  int accelY_;
  int accelZ_;
  uint64_t micLevel_;

  bool envCached_;
  bool ambientCached_;
  bool accelCached_;
  bool micCached_;

  uint32_t envLastMs_;
  uint32_t ambientLastMs_;
  uint32_t accelLastMs_;
  uint32_t micLastMs_;
};

// Callback de progresso: recebe valor 0–100 (porcentagem).
using ProgressCallback = void (*)(uint8_t percent);

struct CameraLiveOptions {
  // Optional app callbacks for each button gesture.
  ButtonCallback onAShort = nullptr;
  ButtonCallback onALong = nullptr;
  ButtonCallback onBShort = nullptr;
  ButtonCallback onBLong = nullptr;

  // Called after default B-long behavior (cameraStop) to return UI/context.
  ButtonCallback onReturnContext = nullptr;

  // Threshold used by timed release controller.
  uint32_t longPressMs = InputService::kDefaultLongPressMs;
};

struct StorageHealth {
  bool sdReady = false;
  bool cardPresent = false;
  bool readWriteOk = false;
  uint64_t totalBytes = 0;
  uint64_t usedBytes = 0;
};

class CameraService {
 public:
  explicit CameraService(IBoardHal &hal, InputService *input = nullptr)
      : hal_(hal),
        input_(input),
        previewInitialized_(false),
        previewActive_(false),
  liveControllerActive_(false),
  liveResIndex_(0),
  livePhotoIndex_(0) {}

  // Simple lifecycle helpers for app-level usage.
  // start(): starts preview pipeline.
  // stop(): hides preview (soft stop).
  // killAndReboot(): hard stop by rebooting MCU.
  Status start();
  Status stop();
  Status killAndReboot(uint16_t delayMs = 120);

  // High-level helper for app usage:
  // - starts camera preview
  // - routes A/B short/long on release using configured threshold
  // - A long default: cameraStop() + onReturnContext callback
  Status cameraLive(const CameraLiveOptions &options = CameraLiveOptions());

  // Boot helper for reboot-based capture flow.
  // - CaptureOnce role: captures one frame and reboots back to Live role.
  // - Live role: enters cameraLive automatically.
  // - Menu role: does nothing (caller should draw menu).
  Status cameraLiveBoot(const CameraLiveOptions &options, bool *enteredLive = nullptr);

  // cameraStop() soft-stops preview and clears live controller state.
  // hardCleanup=true performs reboot-based hard stop.
  Status cameraStop(bool hardCleanup = false, uint16_t rebootDelayMs = 120);

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

  static CameraService *activeLiveController_;
  static void onLiveAShortThunk();
  static void onLiveALongThunk();
  static void onLiveBShortThunk();
  static void onLiveBLongThunk();
  void handleLiveAShort();
  void handleLiveALong();
  void handleLiveBShort();
  void handleLiveBLong();
  void drawDefaultLiveUi();
  void cycleDefaultLiveResolution();
  Status captureDefaultLivePhoto();
  Status queueDefaultLiveCaptureByReboot();
  const char *currentLiveResolutionName() const;
  void loadLiveState();
  bool saveLiveRole(uint8_t role);
  bool saveLiveRes();
  bool saveLivePhoto();
  uint8_t loadLiveRoleRaw();

  IBoardHal &hal_;
  InputService *input_;
  bool previewInitialized_;
  bool previewActive_;
  bool liveControllerActive_;
  uint8_t liveResIndex_;
  uint32_t livePhotoIndex_;
  CameraLiveOptions liveOptions_;
};

class StorageService {
 public:
  explicit StorageService(IBoardHal &hal)
      : hal_(hal),
        imageDir_("S:/images"),
        audioDir_("S:/audio"),
        dataDir_("S:/data"),
      lastReadBytes_(0),
        wavDataBytes_(0),
        wavSampleRate_(16000),
        wavChannels_(2),
        wavBitsPerSample_(16) {}

  Status initSd();
    Status healthCheck(StorageHealth &out, bool writeProbe = true);
  Status savePhotoBmp(const String &path);

  // Default folders by format (user can override via setters or per call).
  Status setImageDirectory(const String &dir);
  Status setAudioDirectory(const String &dir);
  Status setDataDirectory(const String &dir);
  String imageDirectory() const;
  String audioDirectory() const;
  String dataDirectory() const;

  // Builds full API paths like "S:/images/photo.bmp" and
  // "S:/audio/clip.wav". Optional dirOverride can customize target folder.
  String imagePath(const String &fileName, const String &dirOverride = "") const;
  String audioPath(const String &fileName, const String &dirOverride = "") const;
  String dataPath(const String &fileName, const String &dirOverride = "") const;

  // Creates default format directories if missing.
  Status ensureDirectories();

  // Writes an RGB565 BMP file (16-bit BI_BITFIELDS).
  // - fileNameOrPath: file name (uses default/custom image dir) or full S:/ path
  // - dirOverride: optional directory used when fileNameOrPath is only a file name
  Status writeRgb565Bmp(const String &fileNameOrPath,
                        int32_t width,
                        int32_t height,
                        const uint16_t *pixels,
                        const String &dirOverride = "");

  // Centralized WAV (PCM) writer lifecycle for streaming record flows.
  // beginWavRecord() opens/creates target file and writes placeholder header.
  // appendWavRecord() appends raw PCM payload chunks.
  // endWavRecord() rewrites final header; keepFile=false discards partial file.
  Status beginWavRecord(const String &fileNameOrPath,
                        uint32_t sampleRate = 16000,
                        uint16_t channels = 2,
                        uint16_t bitsPerSample = 16,
                        const String &dirOverride = "");
  Status appendWavRecord(const uint8_t *data, size_t bytes);
  Status endWavRecord(bool keepFile = true);
  uint32_t wavRecordedBytes() const { return wavDataBytes_; }

  // Generic helpers for metadata/log formats.
  Status writeTextFile(const String &fileNameOrPath,
                       const String &content,
                       const String &dirOverride = "");
  Status appendTextFile(const String &fileNameOrPath,
                        const String &content,
                        const String &dirOverride = "");
  Status writeBinaryFile(const String &fileNameOrPath,
                         const uint8_t *data,
                         size_t bytes,
                         const String &dirOverride = "");
  Status appendBinaryFile(const String &fileNameOrPath,
                          const uint8_t *data,
                          size_t bytes,
                          const String &dirOverride = "");
  Status readTextFile(const String &fileNameOrPath,
                      String *outContent,
                      size_t maxBytes = 8192,
                      const String &dirOverride = "");
  Status readBinaryFile(const String &fileNameOrPath,
                        uint8_t *buffer,
                        size_t capacity,
                        size_t *outBytes,
                        const String &dirOverride = "");
  Status fileExists(const String &fileNameOrPath,
                    bool *outExists,
                    const String &dirOverride = "");
  Status fileSize(const String &fileNameOrPath,
                  uint32_t *outSize,
                  const String &dirOverride = "");
  Status removeFile(const String &fileNameOrPath,
                    const String &dirOverride = "");
  size_t lastReadBytes() const { return lastReadBytes_; }

  // Convenience aliases for common text formats.
  Status writeJson(const String &fileNameOrPath,
                   const String &json,
                   const String &dirOverride = "");
  Status writeCsv(const String &fileNameOrPath,
                  const String &csv,
                  const String &dirOverride = "");

 private:
  Status resolveApiPath(const String &fileNameOrPath,
                        const String &defaultDir,
                        const String &dirOverride,
                        String *outPath) const;
  Status writeWavHeader(File &file,
                        uint32_t dataSize,
                        uint32_t sampleRate,
                        uint16_t channels,
                        uint16_t bitsPerSample) const;
  String normalizeDirectory(const String &dir) const;
  String normalizeFileName(const String &fileName) const;
  String joinPath(const String &dir, const String &fileName) const;
  Status ensureDirectoryExists(const String &dir) const;

  IBoardHal &hal_;
  String imageDir_;
  String audioDir_;
  String dataDir_;
  size_t lastReadBytes_;

  File wavFile_;
  String wavApiPath_;
  String wavSdPath_;
  uint32_t wavDataBytes_;
  uint32_t wavSampleRate_;
  uint16_t wavChannels_;
  uint16_t wavBitsPerSample_;
};

class AudioService {
 public:
  AudioService(IBoardHal &boardHal, IAudioHal &audioHal)
      : boardHal_(boardHal),
        audioHal_(audioHal),
        stateMutex_(xSemaphoreCreateMutex()),
        i2sReady_(false),
        activeSession_(SessionType::None) {}

  Status playBuiltIn(Melodies melody, MelodyOptions options = OnceInBackground);
  Status stopBuiltIn();
  Status playFile(const String &path);
  Status stopFile();
  Status recordFile(const String &path, uint8_t seconds);

 private:
  enum class SessionType : uint8_t {
    None = 0,
    BuiltIn,
    File,
    Recording,
  };

  Status lockState();
  void unlockState();
  Status ensureAudioReadyLocked();
  Status beginSessionLocked(SessionType type);
  void endSessionLocked(SessionType type);

  IBoardHal &boardHal_;
  IAudioHal &audioHal_;
  SemaphoreHandle_t stateMutex_;
  bool i2sReady_;
  SessionType activeSession_;
};

class VisionService {
 public:
  VisionService(IBoardHal &boardHal, IVisionHal &visionHal)
      : boardHal_(boardHal),
        visionHal_(visionHal),
        currentMode_(AiMode::None),
        initialized_(false),
        stateMutex_(xSemaphoreCreateMutex()),
        modeSwitchCount_(0) {}

  Status init();
  Status setMode(AiMode mode);
  AiMode mode() const { return currentMode_; }
  uint32_t modeSwitchCount() const { return modeSwitchCount_; }
  bool detected();
  int faceData(AIRecognition::eFaceOrCatData_t type);
  int catData(AIRecognition::eFaceOrCatData_t type);
  String qrPayload();

 private:
  Status lockState();
  void unlockState();

  IBoardHal &boardHal_;
  IVisionHal &visionHal_;
  AiMode currentMode_;
  bool initialized_;
  SemaphoreHandle_t stateMutex_;
  uint32_t modeSwitchCount_;
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
