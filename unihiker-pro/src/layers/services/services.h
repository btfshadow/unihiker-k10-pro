#pragma once

#include "../hal/hal_interfaces.h"

#include <Arduino.h>
#include <AIRecognition.h>
#include <esp_camera.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <unihiker_k10.h>

class WebServer;

namespace unihiker_pro {

class VisionService;

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

struct WifiConnectOptions {
  uint32_t timeoutMs = 12000;
  bool allowScanFallback = true;
  bool useStoredRadioHints = true;
  bool persistOnSuccess = true;
};

struct WifiScanResult {
  String ssid;
  String bssid;
  int32_t rssi = -127;
  uint8_t channel = 0;
  uint8_t encryption = 0;
  bool hidden = false;
};

struct WifiLinkStats {
  bool connected = false;
  String ssid;
  String bssid;
  int32_t rssi = -127;
  uint8_t channel = 0;
  uint8_t qualityPercent = 0;
  String localIp;
  String gatewayIp;
  String subnetMask;
  String dns1;
  String dns2;
  uint32_t connectedSinceMs = 0;
  uint32_t reconnectCount = 0;
  size_t knownProfiles = 0;
};

struct WifiContextSnapshot {
  bool connected = false;
  uint8_t statusCode = 0;
  String ssid;
  String bssid;
  String localIp;
  String gatewayIp;
  String subnetMask;
  String dns1;
  String dns2;
  String stationMac;
  int32_t rssi = -127;
  uint8_t qualityPercent = 0;
  uint8_t channel = 0;
  uint32_t connectedSinceMs = 0;
  uint32_t reconnectCount = 0;
  uint32_t successfulConnectCount = 0;
  size_t knownProfiles = 0;
  uint32_t updatedAtMs = 0;
};

struct MdnsLinkStats {
  bool running = false;
  String host;
  String instance;
  String service;
  String proto;
  uint16_t port = 0;
  uint32_t startedAtMs = 0;
};

struct HttpServerStats {
  bool running = false;
  uint16_t port = 0;
  uint32_t startedAtMs = 0;
  uint32_t requestCount = 0;
  bool exposeAnalysis = true;
};

enum class SpeechProfile {
  Auto = 0,
  Chinese,
  English,
  PortugueseBrazil,
};

struct SpeechCommandEntry {
  uint8_t id;
  String phrase;
};

enum class VisionWorkflowMode : uint8_t {
  LiveAim = 0,
  CaptureReview,
  InputReader,
  Ocr,
};

struct VisionWorkflowResult {
  bool ok = false;
  VisionWorkflowMode workflow = VisionWorkflowMode::LiveAim;
  AiMode mode = AiMode::None;
  bool detected = false;
  bool recognized = false;
  int recognitionId = -1;
  bool ocrSupported = false;
  size_t analyzedBytes = 0;
  String source;
  String summary;
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

class ConnectivityService {
 public:
  ConnectivityService()
      : stateMutex_(xSemaphoreCreateMutex()),
        wifiStarted_(false),
        autoReconnectEnabled_(true),
        connectedSinceMs_(0),
        reconnectCount_(0),
        successfulConnectCount_(0),
        profileCount_(0),
        lastConnectedIndex_(-1),
        profilesLoaded_(false) {}

  Status begin(bool autoReconnect = true);
  Status setAutoReconnect(bool enabled);
  bool autoReconnectEnabled() const { return autoReconnectEnabled_; }

  Status addKnownNetwork(const String &ssid, const String &password);
  Status removeKnownNetwork(const String &ssid);
  Status clearKnownNetworks();
  size_t knownNetworkCount() const { return profileCount_; }

  Status connect(const String &ssid,
                 const String &password,
                 const WifiConnectOptions &options = WifiConnectOptions());
  Status connectKnown(const WifiConnectOptions &options = WifiConnectOptions());
  Status disconnect(bool eraseConfig = false);

  bool connected() const;
  String ssid() const;
  String bssid() const;
  int32_t rssi() const;
  String localIp() const;
  String gatewayIp() const;
  String subnetMask() const;
  String dnsIp(uint8_t index = 0) const;

  Status scan(WifiScanResult *outEntries,
              size_t capacity,
              size_t *outCount,
              bool showHidden = true,
              bool passive = false,
              uint32_t maxMsPerChan = 120);

  Status analyzeEnvironment(String *outReport,
                            size_t topN = 8,
                            bool showHidden = true);

  Status parseWifiQrPayload(const String &payload,
                            String *outSsid,
                            String *outPassword,
                            bool *outHidden = nullptr) const;
  Status connectFromQrPayload(const String &payload,
                              const WifiConnectOptions &options = WifiConnectOptions());
  Status connectFromVisionQr(VisionService &vision,
                             const WifiConnectOptions &options = WifiConnectOptions());
  Status waitAndConnectFromVisionQr(VisionService &vision,
                                    const WifiConnectOptions &options = WifiConnectOptions(),
                                    uint32_t timeoutMs = 30000,
                                    uint32_t pollMs = 180,
                                    String *outPayload = nullptr);

  Status wifiContext(WifiContextSnapshot &out, bool refresh = true) const;
  Status linkStats(WifiLinkStats &out) const;

  Status startMdns(const String &host,
                   const String &instance = "unihiker-pro",
                   const String &service = "unihiker",
                   const String &proto = "tcp",
                   uint16_t port = 80);
  Status stopMdns();
  bool mdnsRunning() const { return mdnsRunning_; }
  Status mdnsLinkStats(MdnsLinkStats &out) const;
  Status mdnsDiagnostics(String *outReport, bool queryNetwork = true);

  Status startHttpServer(uint16_t port = 80, bool exposeAnalysis = true);
  Status stopHttpServer();
  bool httpServerRunning() const { return httpRunning_; }
  Status httpServerStats(HttpServerStats &out) const;
  Status httpHandleClient();

 private:
  struct KnownProfile {
    String ssid;
    String password;
    uint8_t channel = 0;
    uint8_t bssid[6] = {0, 0, 0, 0, 0, 0};
    bool hasBssid = false;
    uint32_t successCount = 0;
    int32_t lastRssi = -127;
  };

  static constexpr size_t kMaxProfiles = 8;
  static constexpr size_t kScanScratchCapacity = 24;
  static constexpr const char *kPrefsNamespace = "wifi_sdk";

  Status lockState() const;
  void unlockState() const;
  Status ensureWifiStartedLocked();
  Status loadProfilesFromPrefsLocked();
  Status saveProfilesToPrefsLocked();
  int findProfileIndexBySsidLocked(const String &ssid) const;
  void compactProfilesLocked();
  void recordSuccessfulConnectLocked(int profileIndex,
                                     int32_t rssi,
                                     uint8_t channel,
                                     const uint8_t *bssid);
  Status attemptConnectLocked(const String &ssid,
                              const String &password,
                              uint32_t timeoutMs,
                              uint8_t channelHint,
                              const uint8_t *bssidHint,
                              bool useHints);
  Status connectProfileIndexLocked(int profileIndex,
                                   const WifiConnectOptions &options,
                                   uint8_t channelHint,
                                   const uint8_t *bssidHint,
                                   bool allowHintOverride);
  void refreshWifiContextLocked() const;

  SemaphoreHandle_t stateMutex_;
  bool wifiStarted_;
  bool autoReconnectEnabled_;
  uint32_t connectedSinceMs_;
  uint32_t reconnectCount_;
  uint32_t successfulConnectCount_;
  KnownProfile profiles_[kMaxProfiles];
  size_t profileCount_;
  int lastConnectedIndex_;
  bool profilesLoaded_;
  bool mdnsRunning_ = false;
  String mdnsHost_;
  String mdnsInstance_;
  String mdnsService_;
  String mdnsProto_;
  uint16_t mdnsPort_ = 0;
  uint32_t mdnsStartedAtMs_ = 0;

  WebServer *httpServer_ = nullptr;
  bool httpRunning_ = false;
  uint16_t httpPort_ = 0;
  uint32_t httpStartedAtMs_ = 0;
  uint32_t httpRequestCount_ = 0;
  bool httpExposeAnalysis_ = true;
  mutable WifiContextSnapshot wifiContext_;
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
  VisionService(IBoardHal &boardHal,
                IVisionHal &visionHal,
                CameraService *cameraService = nullptr,
                StorageService *storageService = nullptr,
                DisplayService *displayService = nullptr)
      : boardHal_(boardHal),
        visionHal_(visionHal),
        cameraService_(cameraService),
        storageService_(storageService),
        displayService_(displayService),
        workflowMode_(VisionWorkflowMode::LiveAim),
        currentMode_(AiMode::None),
        activeHalMode_(AiMode::None),
        liveAimActive_(false),
        liveFeedbackEnabled_(true),
        liveFeedbackStopRequested_(false),
        liveFeedbackTask_(nullptr),
        liveFeedbackPeriodMs_(120),
        initialized_(false),
        stateMutex_(xSemaphoreCreateMutex()),
        modeSwitchCount_(0) {}

  Status init();
  Status setMode(AiMode mode);
  AiMode mode() const { return currentMode_; }
  uint32_t modeSwitchCount() const { return modeSwitchCount_; }
  Status setWorkflowMode(VisionWorkflowMode workflowMode);
  VisionWorkflowMode workflowMode() const { return workflowMode_; }
  const VisionWorkflowResult &lastWorkflowResult() const { return lastWorkflowResult_; }

  // Phase 1: live camera assist mode for aiming.
  Status startLiveAim(bool drawHints = true);
  Status stopLiveAim();
  Status setLiveFeedbackEnabled(bool enabled);
  bool liveFeedbackEnabled() const { return liveFeedbackEnabled_; }
  Status setLiveFeedbackPeriodMs(uint32_t periodMs);
  uint32_t liveFeedbackPeriodMs() const { return liveFeedbackPeriodMs_; }
  Status refreshLiveAimFeedback(bool drawAimReticle = true);
  String describeCurrentPerception();
  bool liveAimActive() const { return liveAimActive_; }

  // Phase 2: capture hi-res and review saved image.
  Status captureAndReview(const String &fileNameOrPath,
                          framesize_t framesize = FRAMESIZE_UXGA,
                          const String &dirOverride = "");

  // Phase 3: generic input reader (text/file/binary).
  Status analyzeInputText(const String &payload);
  Status analyzeInputFile(const String &fileNameOrPath,
                          const String &dirOverride = "");
  Status analyzeInputBinary(const uint8_t *data, size_t bytes);
  Status analyzeInputAny(const String &payloadOrPath);

  // Phase 4: OCR entrypoint with explicit fallback when OCR engine is absent.
  Status runOcrOnInput(const String &payloadOrPath, String *outText = nullptr);

  bool detected();
  bool recognized();
  int recognitionId();
  Status setMotionThreshold(uint8_t threshold);
  int faceData(AIRecognition::eFaceOrCatData_t type);
  int catData(AIRecognition::eFaceOrCatData_t type);
  String qrPayload();

 private:
  bool isFaceCommandMode(AiMode mode) const;
  AiMode mapToHalMode(AiMode mode) const;
  recognizer_state_t mapFaceCommand(AiMode mode) const;
  bool isTextExtension(const String &extLower) const;
  bool isImageExtension(const String &extLower) const;
  bool isAudioExtension(const String &extLower) const;
  String extensionLower(const String &path) const;
  String resolveInputPath(const String &fileNameOrPath, bool *found) const;
  String buildPerceptionSummary(AiMode mode, VisionWorkflowMode workflowMode);
  static void liveFeedbackTaskThunk(void *arg);
  void ensureLiveFeedbackTask();
  void stopLiveFeedbackTask();
  void setWorkflowResult(bool ok,
                         const String &source,
                         const String &summary,
                         size_t analyzedBytes = 0,
                         bool ocrSupported = false);

  Status lockState();
  void unlockState();

  IBoardHal &boardHal_;
  IVisionHal &visionHal_;
  CameraService *cameraService_;
  StorageService *storageService_;
  DisplayService *displayService_;
  VisionWorkflowMode workflowMode_;
  VisionWorkflowResult lastWorkflowResult_;
  AiMode currentMode_;
  AiMode activeHalMode_;
  bool liveAimActive_;
  bool liveFeedbackEnabled_;
  volatile bool liveFeedbackStopRequested_;
  TaskHandle_t liveFeedbackTask_;
  uint32_t liveFeedbackPeriodMs_;
  bool initialized_;
  SemaphoreHandle_t stateMutex_;
  uint32_t modeSwitchCount_;
};

class SpeechService {
 public:
  explicit SpeechService(ISpeechHal &speechHal) : speechHal_(speechHal) {}

  void begin(uint8_t mode = 0, uint8_t lang = 0, uint16_t wakeUpMs = 6000);
  Status beginWithProfile(SpeechProfile profile,
                          uint8_t mode = 0,
                          uint16_t wakeUpMs = 6000,
                          bool allowFallbackToEnglish = true);
  Status beginAuto(uint8_t mode = 0,
                   uint16_t wakeUpMs = 6000,
                   bool allowFallbackToEnglish = true);

  Status initTts(uint8_t speed = 2);

  void addCommand(uint8_t id, const String &phrase);
  Status resetCommandRegistry();
  Status queueCommand(uint8_t id, const String &phrase);
  Status applyQueuedCommands();
  size_t queuedCommandCount() const { return queuedCommandCount_; }

  bool detectCommand(uint8_t id);
  bool wakeDetected();
  bool ttsReady() const;
  void speak(const String &text);

  bool initialized() const { return initialized_; }
  SpeechProfile requestedProfile() const { return requestedProfile_; }
  SpeechProfile activeProfile() const { return activeProfile_; }
  uint8_t activeLang() const { return activeLang_; }
  uint8_t activeMode() const { return activeMode_; }
  uint16_t activeWakeUpMs() const { return activeWakeUpMs_; }
  bool fallbackToEnglishApplied() const { return fallbackToEnglishApplied_; }
  Status lastInitStatus() const { return lastInitStatus_; }
  String initSummary() const;

  static const char *profileLabel(SpeechProfile profile);

 private:
  Status beginInternal(SpeechProfile requested,
                       uint8_t mode,
                       uint8_t lang,
                       uint16_t wakeUpMs,
                       bool fallbackApplied,
                       const char *statusMessage,
                       bool emitProfileTelemetry);

  ISpeechHal &speechHal_;
  SpeechProfile requestedProfile_ = SpeechProfile::Auto;
  SpeechProfile activeProfile_ = SpeechProfile::Auto;
  uint8_t activeLang_ = 0;
  uint8_t activeMode_ = 0;
  uint16_t activeWakeUpMs_ = 6000;
  static constexpr size_t kMaxQueuedCommands = 24;
  SpeechCommandEntry queuedCommands_[kMaxQueuedCommands];
  size_t queuedCommandCount_ = 0;
  bool ttsInitAttempted_ = false;
  bool initialized_ = false;
  bool fallbackToEnglishApplied_ = false;
  Status lastInitStatus_ = Status::Error(StatusCode::NotInitialized,
                                         "speech not initialized");
};

}  // namespace unihiker_pro
