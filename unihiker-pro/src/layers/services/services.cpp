#include "services.h"

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <asr.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <img_converters.h>
#include <SD.h>
#include <unihiker_k10.h>
#include <who_camera.h>

extern "C" void lv_fs_fatfs_init(void);
extern SemaphoreHandle_t xSPIlMutex __attribute__((weak));

namespace unihiker_pro {

namespace {
struct LiveModeItem {
  framesize_t size;
  const char *name;
};

static const LiveModeItem kLiveModes[] = {
  {FRAMESIZE_QVGA, "QVGA 320x240"},
  {FRAMESIZE_VGA, "VGA 640x480"},
  {FRAMESIZE_SVGA, "SVGA 800x600"},
  {FRAMESIZE_XGA, "XGA 1024x768"},
  {FRAMESIZE_SXGA, "SXGA 1280x1024"},
  {FRAMESIZE_UXGA, "UXGA 1600x1200"},
};

static const uint8_t kLiveModeCount =
    (uint8_t)(sizeof(kLiveModes) / sizeof(kLiveModes[0]));

static const char *kLivePrefsNs = "cam_live_core";
static const char *kLiveRoleKey = "role";
static const char *kLiveResKey = "res";
static const char *kLivePhotoKey = "photo";
static const uint32_t kCaptureBootSettleMs = 1200;
static const uint32_t kCaptureWarmupMs = 2000;
static const uint32_t kVisionFeedbackTaskStackBytes = 8192;

static String fixedOverlayText(const String &input, size_t width) {
  String out = input;
  out.replace('\n', ' ');
  out.replace('\r', ' ');
  if (out.length() > width) {
    out = out.substring(0, width);
  }
  while (out.length() < width) {
    out += ' ';
  }
  return out;
}

enum LiveRole : uint8_t {
  LiveRoleMenu = 0,
  LiveRoleLive = 1,
  LiveRoleCaptureOnce = 2,
};

static bool gLvFsFatFsReady = false;
static QueueHandle_t gLiveCaptureQueue = nullptr;

static Status ensureSdReadyNoLoop() {
  if (!SD.begin()) {
    return Status::Error(StatusCode::IOError, "SD.begin failed");
  }
  if (!gLvFsFatFsReady) {
    lv_fs_fatfs_init();
    gLvFsFatFsReady = true;
  }
  return Status::OkStatus();
}

static bool prefsBegin(Preferences &prefs, bool readOnly) {
  return prefs.begin(kLivePrefsNs, readOnly);
}

static bool initLiveCaptureQueue(framesize_t size) {
  if (!gLiveCaptureQueue) {
    gLiveCaptureQueue = xQueueCreate(2, sizeof(camera_fb_t *));
    if (!gLiveCaptureQueue) return false;
  }
  register_camera(PIXFORMAT_RGB565, size, 1, gLiveCaptureQueue);
  return true;
}

static void drainLiveCaptureQueue() {
  if (!gLiveCaptureQueue) return;
  camera_fb_t *f = nullptr;
  while (xQueueReceive(gLiveCaptureQueue, &f, 0) == pdTRUE) {
    if (f) esp_camera_fb_return(f);
  }
}

static void warmupLiveCaptureQueue(uint32_t ms) {
  if (!gLiveCaptureQueue) return;
  uint32_t end = millis() + ms;
  while ((int32_t)(end - millis()) > 0) {
    camera_fb_t *f = nullptr;
    if (xQueueReceive(gLiveCaptureQueue, &f, pdMS_TO_TICKS(120)) == pdTRUE && f) {
      esp_camera_fb_return(f);
    }
  }
}

static String sanitizeFilenameComponent(const String &input) {
  String out;
  out.reserve(input.length());
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    bool isAlphaNum = ((c >= 'a' && c <= 'z') ||
                       (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9'));
    out += isAlphaNum ? c : '_';
  }
  while (out.indexOf("__") >= 0) out.replace("__", "_");
  if (out.length() == 0) out = "capture";
  return out;
}

static const uint8_t kI2cSdaPin = 47;
static const uint8_t kI2cSclPin = 48;
static const uint8_t kAht20I2cAddr = 0x38;
static const uint8_t kAlsI2cAddr = 0x29;
static const uint8_t kAccelI2cAddr0 = 0x18;
static const uint8_t kAccelI2cAddr1 = 0x19;

static bool probeI2cAddress(TwoWire &wire, uint8_t addr) {
  wire.beginTransmission(addr);
  return wire.endTransmission() == 0;
}

static bool toSdFsPath(const String &rawPath, String *outPath) {
  if (outPath == nullptr) return false;
  if (!rawPath.startsWith("S:/")) return false;
  *outPath = rawPath.substring(2);
  return outPath->length() > 0;
}

static void lockSpiStorageBus() {
  if (xSPIlMutex) {
    xSemaphoreTake(xSPIlMutex, portMAX_DELAY);
  }
}

static void unlockSpiStorageBus() {
  if (xSPIlMutex) {
    xSemaphoreGive(xSPIlMutex);
  }
}

static Status ensureApiDirectory(const String &apiDir) {
  String sdDir;
  if (!toSdFsPath(apiDir, &sdDir)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid storage directory");
  }
  if (!sdDir.startsWith("/")) {
    sdDir = "/" + sdDir;
  }

  lockSpiStorageBus();

  if (!SD.exists(sdDir.c_str())) {
    String partial;
    int start = 1;
    while (start < sdDir.length()) {
      int slash = sdDir.indexOf('/', start);
      String segment = (slash < 0) ? sdDir.substring(start)
                                   : sdDir.substring(start, slash);

      if (segment.length() > 0) {
        partial += "/" + segment;
        if (!SD.exists(partial.c_str())) {
          if (!SD.mkdir(partial.c_str())) {
            unlockSpiStorageBus();
            return Status::Error(StatusCode::IOError, "failed to create storage directory");
          }
        }
      }

      if (slash < 0) break;
      start = slash + 1;
    }
  }

  unlockSpiStorageBus();
  return Status::OkStatus();
}

static String fileNameFromApiPath(const String &apiPath) {
  int slash = apiPath.lastIndexOf('/');
  if (slash < 0 || slash + 1 >= (int)apiPath.length()) {
    return apiPath;
  }
  return apiPath.substring(slash + 1);
}

static const char *aiModeLabel(AiMode mode) {
  switch (mode) {
    case AiMode::Face: return "face";
    case AiMode::FaceRecognize: return "face-rec";
    case AiMode::FaceEnroll: return "face-enroll";
    case AiMode::FaceDeleteAll: return "face-clear";
    case AiMode::Cat: return "cat";
    case AiMode::Move: return "move";
    case AiMode::Code: return "qr";
    case AiMode::Ocr: return "ocr";
    case AiMode::None:
    default:
      return "none";
  }
}

static Status copyApiFile(const String &srcApiPath,
                          const String &dstApiPath,
                          size_t *outBytes) {
  if (outBytes) *outBytes = 0;

  String srcSdPath;
  String dstSdPath;
  if (!toSdFsPath(srcApiPath, &srcSdPath) || !toSdFsPath(dstApiPath, &dstSdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid storage path");
  }
  if (!srcSdPath.startsWith("/")) srcSdPath = "/" + srcSdPath;
  if (!dstSdPath.startsWith("/")) dstSdPath = "/" + dstSdPath;

  int slash = dstApiPath.lastIndexOf('/');
  if (slash < 3) {
    return Status::Error(StatusCode::InvalidArgument, "invalid destination path");
  }

  Status st = ensureApiDirectory(dstApiPath.substring(0, slash));
  if (!st.ok()) return st;

  lockSpiStorageBus();

  File src = SD.open(srcSdPath.c_str(), FILE_READ);
  if (!src) {
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to open source file");
  }

  if (SD.exists(dstSdPath.c_str())) {
    (void)SD.remove(dstSdPath.c_str());
  }

  File dst = SD.open(dstSdPath.c_str(), FILE_WRITE);
  if (!dst) {
    src.close();
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to open destination file");
  }

  uint8_t buf[2048];
  size_t total = 0;
  while (true) {
    int n = src.read(buf, sizeof(buf));
    if (n <= 0) break;
    if ((size_t)dst.write(buf, (size_t)n) != (size_t)n) {
      dst.close();
      src.close();
      unlockSpiStorageBus();
      return Status::Error(StatusCode::IOError, "failed to copy file data");
    }
    total += (size_t)n;
  }

  dst.flush();
  dst.close();
  src.close();
  unlockSpiStorageBus();

  if (outBytes) *outBytes = total;
  return Status::OkStatus();
}
}  // namespace

InputService *InputService::activeTimedController_ = nullptr;
CameraService *CameraService::activeLiveController_ = nullptr;

Status DisplayService::setBackground(uint32_t color) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  hal_.board().setScreenBackground(color);
  return Status::OkStatus();
}

Status DisplayService::lockCanvas(Canvas **canvasOut) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  if (!canvasMutex_) {
    return Status::Error(StatusCode::IOError, "display mutex unavailable");
  }
  if (xSemaphoreTakeRecursive(canvasMutex_, pdMS_TO_TICKS(300)) != pdTRUE) {
    return Status::Error(StatusCode::Busy, "display busy");
  }

  Canvas *canvas = hal_.board().canvas;
  // Adopt boot-created canvas only for first session to preserve legacy startup.
  if (!canvasSessionActive_ && canvas != nullptr && canvasSessionId_ == 0) {
    canvasSessionActive_ = true;
    ++canvasSessionId_;
  }
  if (!canvasSessionActive_ || canvas == nullptr) {
    xSemaphoreGiveRecursive(canvasMutex_);
    return Status::Error(StatusCode::NotInitialized,
                         "canvas session not initialized");
  }

  *canvasOut = canvas;
  return Status::OkStatus();
}

void DisplayService::unlockCanvas() {
  if (canvasMutex_) {
    xSemaphoreGiveRecursive(canvasMutex_);
  }
}

Status DisplayService::createCanvas() {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  if (!canvasMutex_) {
    return Status::Error(StatusCode::IOError, "display mutex unavailable");
  }
  if (xSemaphoreTakeRecursive(canvasMutex_, pdMS_TO_TICKS(300)) != pdTRUE) {
    return Status::Error(StatusCode::Busy, "display busy");
  }

  if (hal_.board().canvas == nullptr) {
    hal_.board().creatCanvas();
  }
  if (hal_.board().canvas == nullptr) {
    xSemaphoreGiveRecursive(canvasMutex_);
    return Status::Error(StatusCode::IOError, "failed to create canvas");
  }
  canvasSessionActive_ = true;
  ++canvasSessionId_;

  xSemaphoreGiveRecursive(canvasMutex_);
  return Status::OkStatus();
}

Status DisplayService::destroyCanvas() {
  if (!hal_.isReady()) {
    return Status::OkStatus();
  }
  if (!canvasMutex_) {
    return Status::Error(StatusCode::IOError, "display mutex unavailable");
  }
  if (xSemaphoreTakeRecursive(canvasMutex_, pdMS_TO_TICKS(300)) != pdTRUE) {
    return Status::Error(StatusCode::Busy, "display busy");
  }

  if (hal_.board().canvas != nullptr) {
    hal_.board().canvas->canvasClear();
    hal_.board().canvas->updateCanvas();
  }
  canvasSessionActive_ = false;
  ++canvasSessionId_;

  xSemaphoreGiveRecursive(canvasMutex_);
  return Status::OkStatus();
}

Status DisplayService::setCameraBackground(bool enabled) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  hal_.board().setBgCamerImage(enabled);
  return Status::OkStatus();
}

Status DisplayService::clearCanvas() {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasClear();
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::clearRow(uint8_t row) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasClear(row);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::clearRegion(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->clearLocalCanvas(x, y, w, h);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::setLineWidth(uint8_t w) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasSetLineWidth(w);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::setFontSize(Canvas::eFontSize_t font) {
  font_ = font;
  return Status::OkStatus();
}

Status DisplayService::drawPoint(int16_t x, int16_t y, uint32_t color) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasPoint(x, y, color);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::drawLine(int x1, int y1, int x2, int y2, uint32_t color) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasLine(x1, y1, x2, y2, color);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::drawCircle(int x, int y, int r, uint32_t color,
                                  uint32_t bgColor, bool fill) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasCircle(x, y, r, color, bgColor, fill);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::drawRect(int x, int y, int w, int h, uint32_t color,
                                uint32_t bgColor, bool fill) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasRectangle(x, y, w, h, color, bgColor, fill);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                                  const uint8_t *bitmap) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasDrawBitmap(x, y, w, h, bitmap);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::drawImage(int16_t x, int16_t y, const String &path) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasDrawImage(x, y, path);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::textRow(const String &text, uint8_t row, uint32_t color) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasText(text, row, color);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::textAt(const String &text, int16_t x, int16_t y, uint32_t color,
                              int count, bool autoClean) {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->canvasText(text, x, y, color, font_, count, autoClean);
  unlockCanvas();
  return Status::OkStatus();
}

Status DisplayService::update() {
  Canvas *canvas = nullptr;
  Status st = lockCanvas(&canvas);
  if (!st.ok()) return st;

  canvas->updateCanvas();
  unlockCanvas();
  return Status::OkStatus();
}

bool InputService::buttonAPressed() {
  return hal_.isReady() && hal_.board().buttonA != nullptr &&
         hal_.board().buttonA->isPressed();
}

bool InputService::buttonBPressed() {
  return hal_.isReady() && hal_.board().buttonB != nullptr &&
         hal_.board().buttonB->isPressed();
}

bool InputService::buttonABPressed() {
  return hal_.isReady() && hal_.board().buttonAB != nullptr &&
         hal_.board().buttonAB->isPressed();
}

bool InputService::pressed(ButtonId button) {
  switch (button) {
    case ButtonId::A:
      return buttonAPressed();
    case ButtonId::B:
      return buttonBPressed();
    case ButtonId::AB:
      return buttonABPressed();
    default:
      return false;
  }
}

Status InputService::onPress(ButtonId button, ButtonCallback callback) {
  return hal_.attachButtonPress(button, callback);
}

Status InputService::onRelease(ButtonId button, ButtonCallback callback) {
  return hal_.attachButtonRelease(button, callback);
}

uint8_t InputService::buttonIndex(ButtonId button) const {
  switch (button) {
    case ButtonId::A:
      return 0;
    case ButtonId::B:
      return 1;
    case ButtonId::AB:
      return 2;
    default:
      return 0;
  }
}

void InputService::onPressAThunk() {
  if (activeTimedController_) activeTimedController_->handleTimedPress(ButtonId::A);
}

void InputService::onPressBThunk() {
  if (activeTimedController_) activeTimedController_->handleTimedPress(ButtonId::B);
}

void InputService::onPressABThunk() {
  if (activeTimedController_) activeTimedController_->handleTimedPress(ButtonId::AB);
}

void InputService::onReleaseAThunk() {
  if (activeTimedController_) activeTimedController_->handleTimedRelease(ButtonId::A);
}

void InputService::onReleaseBThunk() {
  if (activeTimedController_) activeTimedController_->handleTimedRelease(ButtonId::B);
}

void InputService::onReleaseABThunk() {
  if (activeTimedController_) activeTimedController_->handleTimedRelease(ButtonId::AB);
}

void InputService::handleTimedPress(ButtonId button) {
  uint8_t idx = buttonIndex(button);
  if (!timedBindings_[idx].enabled) return;
  timedBindings_[idx].pressedAtMs = millis();
}

void InputService::handleTimedRelease(ButtonId button) {
  uint8_t idx = buttonIndex(button);
  TimedBinding &binding = timedBindings_[idx];
  if (!binding.enabled) return;

  uint32_t elapsed = millis() - binding.pressedAtMs;
  bool isLong = elapsed >= binding.longPressMs;
  ButtonCallback cb = isLong ? binding.longCallback : binding.shortCallback;
  if (cb) cb();
}

Status InputService::onReleaseByDuration(ButtonId button,
                                         ButtonCallback shortCallback,
                                         ButtonCallback longCallback,
                                         uint32_t longPressMs) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  if (longPressMs == 0) {
    return Status::Error(StatusCode::InvalidArgument, "longPressMs must be > 0");
  }

  ButtonCallback pressThunk = nullptr;
  ButtonCallback releaseThunk = nullptr;
  switch (button) {
    case ButtonId::A:
      pressThunk = &InputService::onPressAThunk;
      releaseThunk = &InputService::onReleaseAThunk;
      break;
    case ButtonId::B:
      pressThunk = &InputService::onPressBThunk;
      releaseThunk = &InputService::onReleaseBThunk;
      break;
    case ButtonId::AB:
      pressThunk = &InputService::onPressABThunk;
      releaseThunk = &InputService::onReleaseABThunk;
      break;
    default:
      return Status::Error(StatusCode::InvalidArgument, "invalid button");
  }

  activeTimedController_ = this;

  uint8_t idx = buttonIndex(button);
  timedBindings_[idx].enabled = true;
  timedBindings_[idx].longPressMs = longPressMs;
  timedBindings_[idx].shortCallback = shortCallback;
  timedBindings_[idx].longCallback = longCallback;
  timedBindings_[idx].pressedAtMs = millis();

  Status s1 = hal_.attachButtonPress(button, pressThunk);
  if (!s1.ok()) return s1;
  Status s2 = hal_.attachButtonRelease(button, releaseThunk);
  if (!s2.ok()) return s2;
  return Status::OkStatus();
}

Status LedService::setRgb(int8_t index, const RgbColor &color) {
  if (!hal_.isReady() || hal_.board().rgb == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "rgb not initialized");
  }
  hal_.board().rgb->write(index, color.r, color.g, color.b);
  return Status::OkStatus();
}

Status LedService::setAll(const RgbColor &color) {
  return setRgb(-1, color);
}

Status LedService::off() {
  return setAll({0, 0, 0});
}

Status LedService::setBrightness(uint8_t level) {
  if (!hal_.isReady() || hal_.board().rgb == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "rgb not initialized");
  }
  hal_.board().rgb->brightness(level);
  return Status::OkStatus();
}

Status LedService::setBacklight(bool enabled) {
  return hal_.writePin(BoardPin::LcdBacklight, enabled);
}

Status LedService::setAmplifierGain(bool enabled) {
  return hal_.writePin(BoardPin::AmpGain, enabled);
}

Status PinService::write(BoardPin pin, bool level) { return hal_.writePin(pin, level); }

bool PinService::read(BoardPin pin) { return hal_.readPin(pin); }

bool SensorService::cacheExpired(uint32_t nowMs, uint32_t lastMs,
                                 uint32_t ttlMs) const {
  if (ttlMs == 0) return true;
  return (uint32_t)(nowMs - lastMs) >= ttlMs;
}

Status SensorService::setCacheConfig(const SensorCacheConfig &config) {
  cacheConfig_ = config;
  return Status::OkStatus();
}

Status SensorService::refreshEnvironment() {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  tempC_ = hal_.aht20().getData(AHT20::eAHT20TempC);
  humidityRh_ = hal_.aht20().getData(AHT20::eAHT20HumiRH);
  envCached_ = true;
  envLastMs_ = millis();
  return Status::OkStatus();
}

Status SensorService::refreshAmbient() {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  ambientLux_ = hal_.board().readALS();
  ambientCached_ = true;
  ambientLastMs_ = millis();
  return Status::OkStatus();
}

Status SensorService::refreshMotion() {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  accelX_ = hal_.board().getAccelerometerX();
  accelY_ = hal_.board().getAccelerometerY();
  accelZ_ = hal_.board().getAccelerometerZ();
  accelCached_ = true;
  accelLastMs_ = millis();
  return Status::OkStatus();
}

Status SensorService::refreshMic() {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  micLevel_ = hal_.board().readMICData();
  micCached_ = true;
  micLastMs_ = millis();
  return Status::OkStatus();
}

Status SensorService::refreshAll() {
  Status st = refreshEnvironment();
  if (!st.ok()) return st;
  st = refreshAmbient();
  if (!st.ok()) return st;
  st = refreshMotion();
  if (!st.ok()) return st;
  return refreshMic();
}

Status SensorService::diagnose(SensorDiagnostics &out) {
  out = SensorDiagnostics();
  out.boardReady = hal_.isReady();
  if (!out.boardReady) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }

  Wire.begin(kI2cSdaPin, kI2cSclPin);
  out.i2cBusReady = true;
  out.aht20Present = probeI2cAddress(Wire, kAht20I2cAddr);
  out.alsPresent = probeI2cAddress(Wire, kAlsI2cAddr);
  out.accelPresent = probeI2cAddress(Wire, kAccelI2cAddr0) ||
                     probeI2cAddress(Wire, kAccelI2cAddr1);
  out.micAvailable = true;

  return Status::OkStatus();
}

float SensorService::temperatureC() {
  if (!hal_.isReady()) return 0.0f;
  uint32_t nowMs = millis();
  if (!envCached_ || cacheExpired(nowMs, envLastMs_, cacheConfig_.environmentMs)) {
    (void)refreshEnvironment();
  }
  return tempC_;
}

float SensorService::humidityRh() {
  if (!hal_.isReady()) return 0.0f;
  uint32_t nowMs = millis();
  if (!envCached_ || cacheExpired(nowMs, envLastMs_, cacheConfig_.environmentMs)) {
    (void)refreshEnvironment();
  }
  return humidityRh_;
}

uint16_t SensorService::ambientLux() {
  if (!hal_.isReady()) return 0;
  uint32_t nowMs = millis();
  if (!ambientCached_ ||
      cacheExpired(nowMs, ambientLastMs_, cacheConfig_.ambientMs)) {
    (void)refreshAmbient();
  }
  return ambientLux_;
}

int SensorService::accelX() {
  if (!hal_.isReady()) return 0;
  uint32_t nowMs = millis();
  if (!accelCached_ || cacheExpired(nowMs, accelLastMs_, cacheConfig_.accelMs)) {
    (void)refreshMotion();
  }
  return accelX_;
}

int SensorService::accelY() {
  if (!hal_.isReady()) return 0;
  uint32_t nowMs = millis();
  if (!accelCached_ || cacheExpired(nowMs, accelLastMs_, cacheConfig_.accelMs)) {
    (void)refreshMotion();
  }
  return accelY_;
}

int SensorService::accelZ() {
  if (!hal_.isReady()) return 0;
  uint32_t nowMs = millis();
  if (!accelCached_ || cacheExpired(nowMs, accelLastMs_, cacheConfig_.accelMs)) {
    (void)refreshMotion();
  }
  return accelZ_;
}

uint64_t SensorService::micLevel() {
  if (!hal_.isReady()) return 0;
  uint32_t nowMs = millis();
  if (!micCached_ || cacheExpired(nowMs, micLastMs_, cacheConfig_.micMs)) {
    (void)refreshMic();
  }
  return micLevel_;
}

Status CameraService::start() {
  if (previewInitialized_) {
    return showPreview(true);
  }
  return initPreview();
}

Status CameraService::stop() {
  return showPreview(false);
}

Status CameraService::killAndReboot(uint16_t delayMs) {
  Status st = showPreview(false);
  delay(delayMs);
  ESP.restart();
  return st;
}

void CameraService::onLiveAShortThunk() {
  if (activeLiveController_) activeLiveController_->handleLiveAShort();
}

void CameraService::onLiveALongThunk() {
  if (activeLiveController_) activeLiveController_->handleLiveALong();
}

void CameraService::onLiveBShortThunk() {
  if (activeLiveController_) activeLiveController_->handleLiveBShort();
}

void CameraService::onLiveBLongThunk() {
  if (activeLiveController_) activeLiveController_->handleLiveBLong();
}

void CameraService::handleLiveAShort() {
  if (!liveControllerActive_) return;
  if (liveOptions_.onAShort) {
    liveOptions_.onAShort();
    return;
  }

  cycleDefaultLiveResolution();
  drawDefaultLiveUi();
}

void CameraService::handleLiveALong() {
  if (!liveControllerActive_) return;

  // cameraStop() clears liveOptions_, so snapshot callbacks first.
  ButtonCallback onReturnContext = liveOptions_.onReturnContext;
  ButtonCallback onALong = liveOptions_.onALong;

  // Default behavior: long-A leaves live context and stops camera.
  (void)cameraStop(false);

  if (onReturnContext) onReturnContext();
  if (onALong) onALong();
}

void CameraService::handleLiveBShort() {
  if (!liveControllerActive_) return;
  if (liveOptions_.onBShort) {
    liveOptions_.onBShort();
    return;
  }

  (void)queueDefaultLiveCaptureByReboot();
}

void CameraService::handleLiveBLong() {
  if (!liveControllerActive_) return;
  ButtonCallback onBLong = liveOptions_.onBLong;
  if (onBLong) onBLong();
}

Status CameraService::cameraLive(const CameraLiveOptions &options) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  if (input_ == nullptr) {
    return Status::Error(StatusCode::NotSupported,
                         "cameraLive requires InputService linkage");
  }
  if (options.longPressMs == 0) {
    return Status::Error(StatusCode::InvalidArgument, "longPressMs must be > 0");
  }

  Status st = start();
  if (!st.ok()) return st;

  liveOptions_ = options;
  liveControllerActive_ = true;
  activeLiveController_ = this;

  loadLiveState();
  (void)saveLiveRole((uint8_t)LiveRoleLive);

  st = input_->onReleaseByDuration(ButtonId::A,
                                   &CameraService::onLiveAShortThunk,
                                   &CameraService::onLiveALongThunk,
                                   options.longPressMs);
  if (!st.ok()) return st;

  st = input_->onReleaseByDuration(ButtonId::B,
                                   &CameraService::onLiveBShortThunk,
                                   &CameraService::onLiveBLongThunk,
                                   options.longPressMs);
  if (!st.ok()) return st;

  drawDefaultLiveUi();

  return Status::OkStatus();
}

Status CameraService::cameraLiveBoot(const CameraLiveOptions &options, bool *enteredLive) {
  if (enteredLive) *enteredLive = false;
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }

  loadLiveState();
  const uint8_t role = loadLiveRoleRaw();

  if (role == (uint8_t)LiveRoleCaptureOnce) {
    // Consume capture role immediately to avoid reboot loops if capture crashes.
    (void)saveLiveRole((uint8_t)LiveRoleLive);

    // Give camera driver/sensor time to settle after reboot before grabbing frames.
    delay(kCaptureBootSettleMs);

    Status st = captureDefaultLivePhoto();

    delay(800);
    ESP.restart();
    return st;
  }

  if (role == (uint8_t)LiveRoleLive) {
    Status st = cameraLive(options);
    if (st.ok() && enteredLive) *enteredLive = true;
    return st;
  }

  return Status::OkStatus();
}

Status CameraService::cameraStop(bool hardCleanup, uint16_t rebootDelayMs) {
  liveControllerActive_ = false;
  liveOptions_ = CameraLiveOptions();

  if (hardCleanup) {
    return killAndReboot(rebootDelayMs);
  }
  return stop();
}

const char *CameraService::currentLiveResolutionName() const {
  return kLiveModes[liveResIndex_ % kLiveModeCount].name;
}

void CameraService::cycleDefaultLiveResolution() {
  liveResIndex_ = (uint8_t)((liveResIndex_ + 1) % kLiveModeCount);
  USBSerial.printf("cameraLive default: res -> %s\n", currentLiveResolutionName());
}

Status CameraService::captureDefaultLivePhoto() {
  const LiveModeItem &mode = kLiveModes[liveResIndex_ % kLiveModeCount];
  livePhotoIndex_++;

  String path = "S:/" + sanitizeFilenameComponent(String(mode.name)) +
                "_" + String(livePhotoIndex_) + ".bmp";
  String shownPath = path;
  if (shownPath.startsWith("S:")) shownPath = shownPath.substring(2);

  if (hal_.isReady() && hal_.board().canvas != nullptr) {
    hal_.board().canvas->canvasClear();
    hal_.board().canvas->canvasText("capturando...", 1, 0x00FF99);
    hal_.board().canvas->canvasText(String("res: ") + mode.name,
                                    8, 52, 0xCFE8FF,
                                    Canvas::eCNAndENFont16, 28, true);
    hal_.board().canvas->canvasText(shownPath,
                                    8, 82, 0xFFFFFF,
                                    Canvas::eCNAndENFont16, 28, true);
    hal_.board().canvas->updateCanvas();
  }

  Status st = captureHiRes(path, mode.size, nullptr);
  if (st.ok()) {
    (void)saveLivePhoto();
    USBSerial.printf("cameraLive default: saved %s\n", path.c_str());
    if (hal_.isReady() && hal_.board().canvas != nullptr) {
      hal_.board().canvas->canvasClear();
      hal_.board().canvas->canvasText("foto salva", 1, 0x00FF99);
      hal_.board().canvas->canvasText(shownPath,
                                      8, 52, 0xFFFFFF,
                                      Canvas::eCNAndENFont16, 28, true);
      hal_.board().canvas->updateCanvas();
    }
  } else {
    livePhotoIndex_--;
    USBSerial.printf("cameraLive default: capture error=%d\n", (int)st.code);
    if (hal_.isReady() && hal_.board().canvas != nullptr) {
      hal_.board().canvas->canvasClear();
      hal_.board().canvas->canvasText("erro captura", 1, 0xFF5C5C);
      hal_.board().canvas->canvasText(String("code: ") + String((int)st.code),
                                      8, 52, 0xFFFFFF,
                                      Canvas::eCNAndENFont16, 28, true);
      hal_.board().canvas->updateCanvas();
    }
  }
  return st;
}

Status CameraService::queueDefaultLiveCaptureByReboot() {
  (void)saveLiveRes();
  (void)saveLivePhoto();
  (void)saveLiveRole((uint8_t)LiveRoleCaptureOnce);
  USBSerial.println("cameraLive default: capture armed, rebooting...");
  delay(150);
  ESP.restart();
  return Status::OkStatus();
}

void CameraService::drawDefaultLiveUi() {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return;
  }
  hal_.board().canvas->canvasClear();
  hal_.board().canvas->canvasText("camera live", 1, 0xFFFFFF);
  hal_.board().canvas->canvasText(String("res: ") + currentLiveResolutionName(),
                                  8, 46, 0xCFE8FF,
                                  Canvas::eCNAndENFont16, 28, true);
  hal_.board().canvas->canvasText("A<2s:res  B<2s:foto", 8, 78, 0x66CCFF,
                                  Canvas::eCNAndENFont16, 28, true);
  hal_.board().canvas->canvasText("A>2s:sair+limpar", 8, 108, 0x66CCFF,
                                  Canvas::eCNAndENFont16, 28, true);
  hal_.board().canvas->updateCanvas();
}

void CameraService::loadLiveState() {
  Preferences prefs;
  if (!prefsBegin(prefs, true)) {
    liveResIndex_ = 0;
    livePhotoIndex_ = 0;
    return;
  }

  uint32_t res = prefs.getUInt(kLiveResKey, 0);
  uint32_t photo = prefs.getUInt(kLivePhotoKey, 0);
  prefs.end();

  if (kLiveModeCount == 0) {
    liveResIndex_ = 0;
  } else if (res >= kLiveModeCount) {
    liveResIndex_ = (uint8_t)(kLiveModeCount - 1);
  } else {
    liveResIndex_ = (uint8_t)res;
  }
  livePhotoIndex_ = photo;
}

bool CameraService::saveLiveRole(uint8_t role) {
  Preferences prefs;
  if (!prefsBegin(prefs, false)) return false;
  size_t n = prefs.putUInt(kLiveRoleKey, role);
  prefs.end();
  return n > 0;
}

bool CameraService::saveLiveRes() {
  Preferences prefs;
  if (!prefsBegin(prefs, false)) return false;
  size_t n = prefs.putUInt(kLiveResKey, liveResIndex_);
  prefs.end();
  return n > 0;
}

bool CameraService::saveLivePhoto() {
  Preferences prefs;
  if (!prefsBegin(prefs, false)) return false;
  size_t n = prefs.putUInt(kLivePhotoKey, livePhotoIndex_);
  prefs.end();
  return n > 0;
}

uint8_t CameraService::loadLiveRoleRaw() {
  Preferences prefs;
  if (!prefsBegin(prefs, true)) return (uint8_t)LiveRoleMenu;
  uint32_t role = prefs.getUInt(kLiveRoleKey, (uint32_t)LiveRoleMenu);
  prefs.end();
  if (role > (uint32_t)LiveRoleCaptureOnce) return (uint8_t)LiveRoleMenu;
  return (uint8_t)role;
}

Status CameraService::initPreview() {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  hal_.board().initBgCamerImage();
  hal_.board().setBgCamerImage(true);
  previewInitialized_ = true;
  previewActive_ = true;
  return Status::OkStatus();
}

Status CameraService::showPreview(bool enabled) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }

  // Safe no-op: stopping preview before first init should not crash LVGL internals.
  if (!previewInitialized_ && !enabled) {
    previewActive_ = false;
    return Status::OkStatus();
  }

  // If preview was never initialized, initialize it on first enable request.
  if (!previewInitialized_ && enabled) {
    return initPreview();
  }

  hal_.board().setBgCamerImage(enabled);
  previewActive_ = enabled;
  return Status::OkStatus();
}

Status CameraService::capture(const String &path) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  if (path.isEmpty()) {
    return Status::Error(StatusCode::InvalidArgument, "path is empty");
  }

  Status sdStatus = ensureSdReadyNoLoop();
  if (!sdStatus.ok()) {
    return sdStatus;
  }

  hal_.board().photoSaveToTFCard(path);
  return Status::OkStatus();
}

Status CameraService::captureHiRes(const String &path, framesize_t framesize,
                                   ProgressCallback onProgress) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  if (path.isEmpty()) {
    return Status::Error(StatusCode::InvalidArgument, "path is empty");
  }

  Status sdStatus = ensureSdReadyNoLoop();
  if (!sdStatus.ok()) {
    return sdStatus;
  }

  if (!initLiveCaptureQueue(framesize)) {
    return Status::Error(StatusCode::IOError, "failed to init capture queue");
  }

  if (onProgress) onProgress(0);
  drainLiveCaptureQueue();
  warmupLiveCaptureQueue(kCaptureWarmupMs);
  drainLiveCaptureQueue();

  camera_fb_t *fb = nullptr;
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    drainLiveCaptureQueue();
    if (xQueueReceive(gLiveCaptureQueue, &fb, pdMS_TO_TICKS(1500)) == pdTRUE && fb) {
      break;
    }
    fb = nullptr;
    delay(60);
  }

  if (fb == nullptr) {
    return Status::Error(StatusCode::IOError, "capture timeout");
  }
  if (fb->format != PIXFORMAT_RGB565) {
    esp_camera_fb_return(fb);
    return Status::Error(StatusCode::NotSupported, "camera format is not RGB565");
  }

  int32_t w = (int32_t)fb->width;
  int32_t h = (int32_t)fb->height;
  if (fb->len != (size_t)w * (size_t)h * 2U) {
    esp_camera_fb_return(fb);
    return Status::Error(StatusCode::IOError, "frame length mismatch");
  }

  if (onProgress) onProgress(20);

  uint8_t *bmpBuf = nullptr;
  size_t bmpLen = 0;
  bool convOk = fmt2bmp(fb->buf, fb->len,
                        (uint16_t)w, (uint16_t)h,
                        PIXFORMAT_RGB565,
                        &bmpBuf, &bmpLen);
  esp_camera_fb_return(fb);

  if (!convOk || bmpBuf == nullptr || bmpLen == 0) {
    if (bmpBuf) free(bmpBuf);
    return Status::Error(StatusCode::IOError, "fmt2bmp conversion failed");
  }

  String sdPath = path;
  if (sdPath.startsWith("S:")) sdPath = sdPath.substring(2);

  if (SD.exists(sdPath.c_str())) SD.remove(sdPath.c_str());
  File f = SD.open(sdPath.c_str(), FILE_WRITE);
  if (!f) {
    free(bmpBuf);
    return Status::Error(StatusCode::IOError, "failed to open file on SD");
  }

  if (onProgress) onProgress(35);
  size_t written = 0;
  while (written < bmpLen) {
    size_t n = bmpLen - written;
    if (n > 4096) n = 4096;
    if (f.write(bmpBuf + written, n) != n) {
      f.close();
      free(bmpBuf);
      return Status::Error(StatusCode::IOError, "failed to write bmp");
    }
    written += n;
  }

  f.flush();
  f.close();
  free(bmpBuf);

  if (onProgress) onProgress(100);
  return Status::OkStatus();
}

// BMP RGB565 header (66 bytes: 14 file header + 40 DIB + 12 color masks)
struct BmpHdr {
  uint16_t bfType;
  uint32_t fileSize;
  uint16_t reserved1, reserved2;
  uint32_t offset;
  uint32_t headerSize;
  int32_t  width, height;
  uint16_t planes, bitsPerPixel;
  uint32_t compression, dataSize;
  int32_t  hRes, vRes;
  uint32_t colors, importantColors;
  uint32_t maskR, maskG, maskB;
} __attribute__((packed));

Status CameraService::writeBmpToSd(const String &path, camera_fb_t *fb,
                                   ProgressCallback onProgress) {
  // BMP path uses SD Arduino API (path without LVGL "S:/" prefix)
  String sdPath = path;
  if (sdPath.startsWith("S:")) sdPath = sdPath.substring(2);  // "S:/foo.bmp" → "/foo.bmp"

  File f = SD.open(sdPath.c_str(), FILE_WRITE);
  if (!f) {
    return Status::Error(StatusCode::IOError, "failed to open file on SD");
  }

  const int32_t w = (int32_t)fb->width;
  const int32_t h = (int32_t)fb->height;
  const uint32_t rowBytes = (uint32_t)w * 2;
  const uint32_t pixelData = rowBytes * (uint32_t)h;

  BmpHdr hdr;
  hdr.bfType         = 0x4D42;
  hdr.fileSize       = sizeof(BmpHdr) + pixelData;
  hdr.reserved1      = 0;
  hdr.reserved2      = 0;
  hdr.offset         = sizeof(BmpHdr);
  hdr.headerSize     = 40;
  hdr.width          = w;
  hdr.height         = h;       // positive → bottom-up; we'll write rows bottom-up
  hdr.planes         = 1;
  hdr.bitsPerPixel   = 16;
  hdr.compression    = 3;       // BI_BITFIELDS
  hdr.dataSize       = pixelData;
  hdr.hRes           = 0;
  hdr.vRes           = 0;
  hdr.colors         = 0;
  hdr.importantColors = 0;
  hdr.maskR          = 0xF800;
  hdr.maskG          = 0x07E0;
  hdr.maskB          = 0x001F;

  f.write(reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr));

  // Write pixel data bottom-up (BMP convention), byte-swap each pixel RGB565
  // Progress goes from 25% to 93% during pixel write
  const uint16_t *pixels = reinterpret_cast<const uint16_t *>(fb->buf);
  // Allocate a row buffer on the stack (max width 1600 * 2 = 3200 bytes)
  static uint8_t rowBuf[1600 * 2];
  const uint8_t progressStart = 25;
  const uint8_t progressEnd   = 93;

  for (int32_t row = h - 1; row >= 0; row--) {
    const uint16_t *src = pixels + row * w;
    for (int32_t col = 0; col < w; col++) {
      uint16_t px = src[col];
      rowBuf[col * 2]     = (uint8_t)(px & 0xFF);        // low byte first (little-endian)
      rowBuf[col * 2 + 1] = (uint8_t)(px >> 8);
    }
    f.write(rowBuf, rowBytes);

    if (onProgress) {
      // Notify every 5% change to avoid flooding the display update
      uint8_t pct = progressStart +
                    (uint8_t)(((uint32_t)(h - 1 - row) * (progressEnd - progressStart)) /
                               (uint32_t)(h - 1));
      static uint8_t lastPct = 0;
      if (pct != lastPct) {
        onProgress(pct);
        lastPct = pct;
      }
    }
  }

  f.flush();
  f.close();
  return Status::OkStatus();
}

Status StorageService::initSd() {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  return ensureSdReadyNoLoop();
}

Status StorageService::healthCheck(StorageHealth &out, bool writeProbe) {
  out = StorageHealth();

  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }

  Status st = ensureSdReadyNoLoop();
  if (!st.ok()) {
    return st;
  }

  out.sdReady = true;

#if defined(CARD_NONE)
  out.cardPresent = SD.cardType() != CARD_NONE;
#else
  out.cardPresent = true;
#endif

  out.totalBytes = (uint64_t)SD.totalBytes();
  out.usedBytes = (uint64_t)SD.usedBytes();

  if (!writeProbe) {
    out.readWriteOk = out.cardPresent;
    return Status::OkStatus();
  }

  static const uint8_t kProbeData[] = {0x55, 0xAA, 0x10, 0x4B};
  st = writeBinaryFile("health_probe.bin", kProbeData, sizeof(kProbeData));
  if (!st.ok()) {
    out.readWriteOk = false;
    return st;
  }

  bool exists = false;
  (void)fileExists("health_probe.bin", &exists);
  uint32_t size = 0;
  (void)fileSize("health_probe.bin", &size);
  (void)removeFile("health_probe.bin");

  out.readWriteOk = exists && size == sizeof(kProbeData);
  if (!out.readWriteOk) {
    return Status::Error(StatusCode::IOError, "storage probe failed");
  }
  return Status::OkStatus();
}

Status StorageService::savePhotoBmp(const String &path) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  Status sdStatus = ensureSdReadyNoLoop();
  if (!sdStatus.ok()) {
    return sdStatus;
  }
  hal_.board().photoSaveToTFCard(path);
  return Status::OkStatus();
}

Status StorageService::resolveApiPath(const String &fileNameOrPath,
                                      const String &defaultDir,
                                      const String &dirOverride,
                                      String *outPath) const {
  if (outPath == nullptr) {
    return Status::Error(StatusCode::InvalidArgument, "outPath is null");
  }

  String path = fileNameOrPath;
  path.trim();
  if (path.length() == 0) {
    return Status::Error(StatusCode::InvalidArgument, "path is empty");
  }

  if (path.startsWith("S:/")) {
    *outPath = path;
    return Status::OkStatus();
  }

  const String &baseDir = (dirOverride.length() > 0) ? dirOverride : defaultDir;
  *outPath = joinPath(baseDir, path);
  return Status::OkStatus();
}

Status StorageService::writeWavHeader(File &file,
                                      uint32_t dataSize,
                                      uint32_t sampleRate,
                                      uint16_t channels,
                                      uint16_t bitsPerSample) const {
  if (!file) {
    return Status::Error(StatusCode::NotInitialized, "wav file not open");
  }
  if (sampleRate == 0 || channels == 0 || bitsPerSample == 0) {
    return Status::Error(StatusCode::InvalidArgument, "invalid wav format");
  }

  uint8_t header[44];
  memset(header, 0, sizeof(header));

  const uint32_t byteRate = sampleRate * (uint32_t)channels * (uint32_t)(bitsPerSample / 8U);
  const uint16_t blockAlign = (uint16_t)(channels * (uint16_t)(bitsPerSample / 8U));
  const uint32_t riffSize = dataSize + 36U;

  header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
  header[4] = (uint8_t)(riffSize & 0xFF);
  header[5] = (uint8_t)((riffSize >> 8) & 0xFF);
  header[6] = (uint8_t)((riffSize >> 16) & 0xFF);
  header[7] = (uint8_t)((riffSize >> 24) & 0xFF);
  header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';
  header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
  header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;
  header[20] = 1; header[21] = 0;
  header[22] = (uint8_t)(channels & 0xFF);
  header[23] = (uint8_t)((channels >> 8) & 0xFF);
  header[24] = (uint8_t)(sampleRate & 0xFF);
  header[25] = (uint8_t)((sampleRate >> 8) & 0xFF);
  header[26] = (uint8_t)((sampleRate >> 16) & 0xFF);
  header[27] = (uint8_t)((sampleRate >> 24) & 0xFF);
  header[28] = (uint8_t)(byteRate & 0xFF);
  header[29] = (uint8_t)((byteRate >> 8) & 0xFF);
  header[30] = (uint8_t)((byteRate >> 16) & 0xFF);
  header[31] = (uint8_t)((byteRate >> 24) & 0xFF);
  header[32] = (uint8_t)(blockAlign & 0xFF);
  header[33] = (uint8_t)((blockAlign >> 8) & 0xFF);
  header[34] = (uint8_t)(bitsPerSample & 0xFF);
  header[35] = (uint8_t)((bitsPerSample >> 8) & 0xFF);
  header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
  header[40] = (uint8_t)(dataSize & 0xFF);
  header[41] = (uint8_t)((dataSize >> 8) & 0xFF);
  header[42] = (uint8_t)((dataSize >> 16) & 0xFF);
  header[43] = (uint8_t)((dataSize >> 24) & 0xFF);

  size_t wrote = file.write(header, sizeof(header));
  if (wrote != sizeof(header)) {
    return Status::Error(StatusCode::IOError, "failed to write wav header");
  }
  return Status::OkStatus();
}

String StorageService::normalizeDirectory(const String &dir) const {
  String out = dir;
  out.trim();
  if (out.length() == 0) {
    return "S:/";
  }

  if (out.startsWith("S:/")) {
    // already normalized
  } else if (out.startsWith("S:")) {
    out = "S:/" + out.substring(2);
  } else if (out.startsWith("/")) {
    out = "S:" + out;
  } else {
    out = "S:/" + out;
  }

  while (out.length() > 3 && out.endsWith("/")) {
    out.remove(out.length() - 1);
  }
  return out;
}

String StorageService::normalizeFileName(const String &fileName) const {
  String out = fileName;
  out.trim();

  if (out.startsWith("S:/")) {
    int slash = out.lastIndexOf('/');
    out = (slash >= 0) ? out.substring(slash + 1) : out;
  } else if (out.startsWith("/")) {
    int slash = out.lastIndexOf('/');
    out = (slash >= 0) ? out.substring(slash + 1) : out;
  }

  if (out.length() == 0) {
    out = "unnamed.bin";
  }
  return out;
}

String StorageService::joinPath(const String &dir, const String &fileName) const {
  if (fileName.startsWith("S:/")) {
    return fileName;
  }

  String folder = normalizeDirectory(dir);
  String name = normalizeFileName(fileName);
  if (folder.endsWith("/")) {
    return folder + name;
  }
  return folder + "/" + name;
}

Status StorageService::ensureDirectoryExists(const String &dir) const {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  Status sdStatus = ensureSdReadyNoLoop();
  if (!sdStatus.ok()) return sdStatus;

  String apiDir = normalizeDirectory(dir);
  String sdDir;
  if (!toSdFsPath(apiDir, &sdDir)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid storage directory");
  }
  if (!sdDir.startsWith("/")) {
    sdDir = "/" + sdDir;
  }
  if (sdDir == "/") {
    return Status::OkStatus();
  }

  String partial;
  int start = 1;
  while (start < sdDir.length()) {
    int slash = sdDir.indexOf('/', start);
    String segment = (slash < 0) ? sdDir.substring(start) : sdDir.substring(start, slash);

    if (segment.length() > 0) {
      partial += "/" + segment;
      if (!SD.exists(partial)) {
        if (!SD.mkdir(partial)) {
          return Status::Error(StatusCode::IOError, "failed to create storage directory");
        }
      }
    }

    if (slash < 0) break;
    start = slash + 1;
  }

  return Status::OkStatus();
}

Status StorageService::setImageDirectory(const String &dir) {
  imageDir_ = normalizeDirectory(dir);
  return Status::OkStatus();
}

Status StorageService::setAudioDirectory(const String &dir) {
  audioDir_ = normalizeDirectory(dir);
  return Status::OkStatus();
}

Status StorageService::setDataDirectory(const String &dir) {
  dataDir_ = normalizeDirectory(dir);
  return Status::OkStatus();
}

String StorageService::imageDirectory() const { return imageDir_; }

String StorageService::audioDirectory() const { return audioDir_; }

String StorageService::dataDirectory() const { return dataDir_; }

String StorageService::imagePath(const String &fileName,
                                 const String &dirOverride) const {
  const String &dir = (dirOverride.length() > 0) ? dirOverride : imageDir_;
  return joinPath(dir, fileName);
}

String StorageService::audioPath(const String &fileName,
                                 const String &dirOverride) const {
  const String &dir = (dirOverride.length() > 0) ? dirOverride : audioDir_;
  return joinPath(dir, fileName);
}

String StorageService::dataPath(const String &fileName,
                                const String &dirOverride) const {
  const String &dir = (dirOverride.length() > 0) ? dirOverride : dataDir_;
  return joinPath(dir, fileName);
}

Status StorageService::ensureDirectories() {
  Status st = ensureDirectoryExists(imageDir_);
  if (!st.ok()) return st;
  st = ensureDirectoryExists(audioDir_);
  if (!st.ok()) return st;
  return ensureDirectoryExists(dataDir_);
}

Status StorageService::writeRgb565Bmp(const String &fileNameOrPath,
                                      int32_t width,
                                      int32_t height,
                                      const uint16_t *pixels,
                                      const String &dirOverride) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  if (width <= 0 || height <= 0 || pixels == nullptr) {
    return Status::Error(StatusCode::InvalidArgument, "invalid bmp params");
  }

  String apiPath = fileNameOrPath;
  if (!apiPath.startsWith("S:/")) {
    apiPath = imagePath(fileNameOrPath, dirOverride);
  }

  int slash = apiPath.lastIndexOf('/');
  if (slash < 3) {
    return Status::Error(StatusCode::InvalidArgument, "invalid bmp path");
  }

  Status st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  String targetDir = apiPath.substring(0, slash);
  st = ensureDirectoryExists(targetDir);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid bmp path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  uint64_t rowBytes64 = (uint64_t)(uint32_t)width * 2ULL;
  uint64_t pixelData64 = rowBytes64 * (uint64_t)(uint32_t)height;
  if (rowBytes64 > SIZE_MAX || pixelData64 > 0xFFFFFFFFULL) {
    return Status::Error(StatusCode::InvalidArgument, "bmp dimensions too large");
  }

  if (SD.exists(sdPath.c_str())) {
    (void)SD.remove(sdPath.c_str());
  }

  File f = SD.open(sdPath.c_str(), FILE_WRITE);
  if (!f) {
    return Status::Error(StatusCode::IOError, "failed to open bmp for write");
  }

  const uint32_t rowBytes = (uint32_t)rowBytes64;
  const uint32_t pixelData = (uint32_t)pixelData64;

  BmpHdr hdr;
  hdr.bfType          = 0x4D42;
  hdr.fileSize        = (uint32_t)sizeof(BmpHdr) + pixelData;
  hdr.reserved1       = 0;
  hdr.reserved2       = 0;
  hdr.offset          = sizeof(BmpHdr);
  hdr.headerSize      = 40;
  hdr.width           = width;
  hdr.height          = height;
  hdr.planes          = 1;
  hdr.bitsPerPixel    = 16;
  hdr.compression     = 3;
  hdr.dataSize        = pixelData;
  hdr.hRes            = 0;
  hdr.vRes            = 0;
  hdr.colors          = 0;
  hdr.importantColors = 0;
  hdr.maskR           = 0xF800;
  hdr.maskG           = 0x07E0;
  hdr.maskB           = 0x001F;

  if (f.write(reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr)) != sizeof(hdr)) {
    f.close();
    return Status::Error(StatusCode::IOError, "failed to write bmp header");
  }

  uint8_t *rowBuf = reinterpret_cast<uint8_t *>(malloc((size_t)rowBytes));
  if (!rowBuf) {
    f.close();
    return Status::Error(StatusCode::IOError, "bmp row buffer alloc failed");
  }

  bool ok = true;
  for (int32_t row = height - 1; row >= 0; --row) {
    const uint16_t *src = pixels + (size_t)row * (size_t)width;
    for (int32_t col = 0; col < width; ++col) {
      uint16_t px = src[col];
      rowBuf[col * 2] = (uint8_t)(px & 0xFF);
      rowBuf[col * 2 + 1] = (uint8_t)(px >> 8);
    }

    if (f.write(rowBuf, rowBytes) != rowBytes) {
      ok = false;
      break;
    }
  }

  free(rowBuf);
  f.flush();
  f.close();

  if (!ok) {
    return Status::Error(StatusCode::IOError, "failed to write bmp data");
  }

  return Status::OkStatus();
}

Status StorageService::beginWavRecord(const String &fileNameOrPath,
                                      uint32_t sampleRate,
                                      uint16_t channels,
                                      uint16_t bitsPerSample,
                                      const String &dirOverride) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  if (sampleRate == 0 || channels == 0 || bitsPerSample == 0) {
    return Status::Error(StatusCode::InvalidArgument, "invalid wav format");
  }
  if (wavFile_) {
    return Status::Error(StatusCode::Busy, "wav writer already active");
  }

  String apiPath;
  Status st = resolveApiPath(fileNameOrPath, audioDir_, dirOverride, &apiPath);
  if (!st.ok()) return st;

  int slash = apiPath.lastIndexOf('/');
  if (slash < 3) {
    return Status::Error(StatusCode::InvalidArgument, "invalid wav path");
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  String targetDir = apiPath.substring(0, slash);
  st = ensureDirectoryExists(targetDir);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid wav path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  lockSpiStorageBus();
  if (SD.exists(sdPath.c_str())) {
    (void)SD.remove(sdPath.c_str());
  }

  wavFile_ = SD.open(sdPath.c_str(), FILE_WRITE);
  if (!wavFile_) {
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to open wav for write");
  }

  wavDataBytes_ = 0;
  wavSampleRate_ = sampleRate;
  wavChannels_ = channels;
  wavBitsPerSample_ = bitsPerSample;
  wavApiPath_ = apiPath;
  wavSdPath_ = sdPath;

  st = writeWavHeader(wavFile_, 0, wavSampleRate_, wavChannels_, wavBitsPerSample_);
  if (!st.ok()) {
    wavFile_.close();
    (void)SD.remove(sdPath.c_str());
    wavApiPath_ = "";
    wavSdPath_ = "";
    wavDataBytes_ = 0;
    unlockSpiStorageBus();
    return st;
  }

  wavFile_.flush();
  unlockSpiStorageBus();
  return Status::OkStatus();
}

Status StorageService::appendWavRecord(const uint8_t *data, size_t bytes) {
  if (!wavFile_) {
    return Status::Error(StatusCode::NotInitialized, "wav writer not active");
  }
  if (bytes == 0) {
    return Status::OkStatus();
  }
  if (data == nullptr) {
    return Status::Error(StatusCode::InvalidArgument, "wav chunk is null");
  }

  lockSpiStorageBus();
  size_t wrote = wavFile_.write(data, bytes);
  unlockSpiStorageBus();

  if (wrote != bytes) {
    return Status::Error(StatusCode::IOError, "failed to append wav data");
  }

  if (wavDataBytes_ > 0xFFFFFFFFUL - (uint32_t)bytes) {
    return Status::Error(StatusCode::InvalidArgument, "wav data too large");
  }
  wavDataBytes_ += (uint32_t)bytes;
  return Status::OkStatus();
}

Status StorageService::endWavRecord(bool keepFile) {
  if (!wavFile_) {
    return Status::Error(StatusCode::NotInitialized, "wav writer not active");
  }

  lockSpiStorageBus();

  if (!keepFile) {
    wavFile_.close();
    (void)SD.remove(wavSdPath_.c_str());
    wavApiPath_ = "";
    wavSdPath_ = "";
    wavDataBytes_ = 0;
    unlockSpiStorageBus();
    return Status::OkStatus();
  }

  if (!wavFile_.seek(0)) {
    wavFile_.close();
    (void)SD.remove(wavSdPath_.c_str());
    wavApiPath_ = "";
    wavSdPath_ = "";
    wavDataBytes_ = 0;
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to seek wav header");
  }

  Status st = writeWavHeader(wavFile_, wavDataBytes_, wavSampleRate_, wavChannels_,
                             wavBitsPerSample_);
  if (!st.ok()) {
    wavFile_.close();
    (void)SD.remove(wavSdPath_.c_str());
    wavApiPath_ = "";
    wavSdPath_ = "";
    wavDataBytes_ = 0;
    unlockSpiStorageBus();
    return st;
  }

  wavFile_.flush();
  wavFile_.close();

  File verify = SD.open(wavSdPath_.c_str(), FILE_READ);
  bool fileOk = verify && verify.size() > 44;
  if (verify) verify.close();

  wavApiPath_ = "";
  wavSdPath_ = "";
  uint32_t finalBytes = wavDataBytes_;
  wavDataBytes_ = 0;

  unlockSpiStorageBus();

  if (!fileOk || finalBytes == 0) {
    return Status::Error(StatusCode::IOError, "wav output not created");
  }
  return Status::OkStatus();
}

Status StorageService::writeTextFile(const String &fileNameOrPath,
                                     const String &content,
                                     const String &dirOverride) {
  String apiPath;
  Status st = resolveApiPath(fileNameOrPath, dataDir_, dirOverride, &apiPath);
  if (!st.ok()) return st;

  int slash = apiPath.lastIndexOf('/');
  if (slash < 3) {
    return Status::Error(StatusCode::InvalidArgument, "invalid text path");
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  String targetDir = apiPath.substring(0, slash);
  st = ensureDirectoryExists(targetDir);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid text path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  lockSpiStorageBus();
  if (SD.exists(sdPath.c_str())) {
    (void)SD.remove(sdPath.c_str());
  }

  File f = SD.open(sdPath.c_str(), FILE_WRITE);
  if (!f) {
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to open text file");
  }

  const char *raw = content.c_str();
  size_t len = content.length();
  size_t wrote = (len == 0) ? 0 : f.write(reinterpret_cast<const uint8_t *>(raw), len);
  f.flush();
  f.close();
  unlockSpiStorageBus();

  if (wrote != len) {
    return Status::Error(StatusCode::IOError, "failed to write text file");
  }
  return Status::OkStatus();
}

Status StorageService::appendTextFile(const String &fileNameOrPath,
                                      const String &content,
                                      const String &dirOverride) {
  String apiPath;
  Status st = resolveApiPath(fileNameOrPath, dataDir_, dirOverride, &apiPath);
  if (!st.ok()) return st;

  int slash = apiPath.lastIndexOf('/');
  if (slash < 3) {
    return Status::Error(StatusCode::InvalidArgument, "invalid text path");
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  String targetDir = apiPath.substring(0, slash);
  st = ensureDirectoryExists(targetDir);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid text path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  lockSpiStorageBus();
#if defined(FILE_APPEND)
  File f = SD.open(sdPath.c_str(), FILE_APPEND);
#else
  File f = SD.open(sdPath.c_str(), FILE_WRITE);
  if (f) {
    (void)f.seek(f.size());
  }
#endif
  if (!f) {
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to open text file append");
  }

  const char *raw = content.c_str();
  size_t len = content.length();
  size_t wrote = (len == 0) ? 0 : f.write(reinterpret_cast<const uint8_t *>(raw), len);
  f.flush();
  f.close();
  unlockSpiStorageBus();

  if (wrote != len) {
    return Status::Error(StatusCode::IOError, "failed to append text file");
  }
  return Status::OkStatus();
}

Status StorageService::writeBinaryFile(const String &fileNameOrPath,
                                       const uint8_t *data,
                                       size_t bytes,
                                       const String &dirOverride) {
  if (bytes > 0 && data == nullptr) {
    return Status::Error(StatusCode::InvalidArgument, "binary data is null");
  }

  String apiPath;
  Status st = resolveApiPath(fileNameOrPath, dataDir_, dirOverride, &apiPath);
  if (!st.ok()) return st;

  int slash = apiPath.lastIndexOf('/');
  if (slash < 3) {
    return Status::Error(StatusCode::InvalidArgument, "invalid binary path");
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  String targetDir = apiPath.substring(0, slash);
  st = ensureDirectoryExists(targetDir);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid binary path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  lockSpiStorageBus();
  if (SD.exists(sdPath.c_str())) {
    (void)SD.remove(sdPath.c_str());
  }

  File f = SD.open(sdPath.c_str(), FILE_WRITE);
  if (!f) {
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to open binary file");
  }

  size_t wrote = (bytes == 0) ? 0 : f.write(data, bytes);
  f.flush();
  f.close();
  unlockSpiStorageBus();

  if (wrote != bytes) {
    return Status::Error(StatusCode::IOError, "failed to write binary file");
  }
  return Status::OkStatus();
}

Status StorageService::appendBinaryFile(const String &fileNameOrPath,
                                        const uint8_t *data,
                                        size_t bytes,
                                        const String &dirOverride) {
  if (bytes > 0 && data == nullptr) {
    return Status::Error(StatusCode::InvalidArgument, "binary data is null");
  }

  String apiPath;
  Status st = resolveApiPath(fileNameOrPath, dataDir_, dirOverride, &apiPath);
  if (!st.ok()) return st;

  int slash = apiPath.lastIndexOf('/');
  if (slash < 3) {
    return Status::Error(StatusCode::InvalidArgument, "invalid binary path");
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  String targetDir = apiPath.substring(0, slash);
  st = ensureDirectoryExists(targetDir);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid binary path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  lockSpiStorageBus();
#if defined(FILE_APPEND)
  File f = SD.open(sdPath.c_str(), FILE_APPEND);
#else
  File f = SD.open(sdPath.c_str(), FILE_WRITE);
  if (f) {
    (void)f.seek(f.size());
  }
#endif
  if (!f) {
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to open binary file append");
  }

  size_t wrote = (bytes == 0) ? 0 : f.write(data, bytes);
  f.flush();
  f.close();
  unlockSpiStorageBus();

  if (wrote != bytes) {
    return Status::Error(StatusCode::IOError, "failed to append binary file");
  }
  return Status::OkStatus();
}

Status StorageService::readTextFile(const String &fileNameOrPath,
                                    String *outContent,
                                    size_t maxBytes,
                                    const String &dirOverride) {
  if (outContent == nullptr) {
    return Status::Error(StatusCode::InvalidArgument, "outContent is null");
  }
  if (maxBytes == 0) {
    return Status::Error(StatusCode::InvalidArgument, "maxBytes must be > 0");
  }

  String apiPath;
  Status st = resolveApiPath(fileNameOrPath, dataDir_, dirOverride, &apiPath);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid text path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  lockSpiStorageBus();
  File f = SD.open(sdPath.c_str(), FILE_READ);
  if (!f) {
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to open text file read");
  }

  outContent->remove(0);
  outContent->reserve(maxBytes);
  size_t count = 0;
  while (f.available() && count < maxBytes) {
    int c = f.read();
    if (c < 0) break;
    *outContent += (char)c;
    count++;
  }

  bool truncated = f.available();
  f.close();
  unlockSpiStorageBus();

  lastReadBytes_ = count;
  if (truncated) {
    return Status::Error(StatusCode::IOError, "text exceeds maxBytes");
  }
  return Status::OkStatus();
}

Status StorageService::readBinaryFile(const String &fileNameOrPath,
                                      uint8_t *buffer,
                                      size_t capacity,
                                      size_t *outBytes,
                                      const String &dirOverride) {
  if (buffer == nullptr || capacity == 0) {
    return Status::Error(StatusCode::InvalidArgument, "invalid read buffer");
  }

  String apiPath;
  Status st = resolveApiPath(fileNameOrPath, dataDir_, dirOverride, &apiPath);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid binary path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  lockSpiStorageBus();
  File f = SD.open(sdPath.c_str(), FILE_READ);
  if (!f) {
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to open binary file read");
  }

  size_t readN = f.read(buffer, capacity);
  bool truncated = f.available();
  f.close();
  unlockSpiStorageBus();

  if (outBytes) {
    *outBytes = readN;
  }
  lastReadBytes_ = readN;

  if (truncated) {
    return Status::Error(StatusCode::IOError, "binary buffer too small");
  }
  return Status::OkStatus();
}

Status StorageService::fileExists(const String &fileNameOrPath,
                                  bool *outExists,
                                  const String &dirOverride) {
  if (outExists == nullptr) {
    return Status::Error(StatusCode::InvalidArgument, "outExists is null");
  }

  String apiPath;
  Status st = resolveApiPath(fileNameOrPath, dataDir_, dirOverride, &apiPath);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid exists path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  lockSpiStorageBus();
  *outExists = SD.exists(sdPath.c_str());
  unlockSpiStorageBus();
  return Status::OkStatus();
}

Status StorageService::fileSize(const String &fileNameOrPath,
                                uint32_t *outSize,
                                const String &dirOverride) {
  if (outSize == nullptr) {
    return Status::Error(StatusCode::InvalidArgument, "outSize is null");
  }

  String apiPath;
  Status st = resolveApiPath(fileNameOrPath, dataDir_, dirOverride, &apiPath);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid size path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  lockSpiStorageBus();
  File f = SD.open(sdPath.c_str(), FILE_READ);
  if (!f) {
    unlockSpiStorageBus();
    return Status::Error(StatusCode::IOError, "failed to open file for size");
  }

  *outSize = (uint32_t)f.size();
  f.close();
  unlockSpiStorageBus();
  return Status::OkStatus();
}

Status StorageService::removeFile(const String &fileNameOrPath,
                                  const String &dirOverride) {
  String apiPath;
  Status st = resolveApiPath(fileNameOrPath, dataDir_, dirOverride, &apiPath);
  if (!st.ok()) return st;

  String sdPath;
  if (!toSdFsPath(apiPath, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "invalid remove path prefix");
  }
  if (!sdPath.startsWith("/")) {
    sdPath = "/" + sdPath;
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) return st;

  lockSpiStorageBus();
  bool exists = SD.exists(sdPath.c_str());
  bool removed = true;
  if (exists) {
    removed = SD.remove(sdPath.c_str());
  }
  unlockSpiStorageBus();

  if (!removed) {
    return Status::Error(StatusCode::IOError, "failed to remove file");
  }
  return Status::OkStatus();
}

Status StorageService::writeJson(const String &fileNameOrPath,
                                 const String &json,
                                 const String &dirOverride) {
  return writeTextFile(fileNameOrPath, json, dirOverride);
}

Status StorageService::writeCsv(const String &fileNameOrPath,
                                const String &csv,
                                const String &dirOverride) {
  return writeTextFile(fileNameOrPath, csv, dirOverride);
}

Status AudioService::lockState() {
  if (!stateMutex_) {
    return Status::Error(StatusCode::IOError, "audio mutex unavailable");
  }
  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(300)) != pdTRUE) {
    return Status::Error(StatusCode::Busy, "audio state busy");
  }
  return Status::OkStatus();
}

void AudioService::unlockState() {
  if (stateMutex_) {
    xSemaphoreGive(stateMutex_);
  }
}

Status AudioService::ensureAudioReadyLocked() {
  if (!boardHal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  if (!i2sReady_) {
    boardHal_.board().initI2S();
    i2sReady_ = true;
  }
  return Status::OkStatus();
}

Status AudioService::beginSessionLocked(SessionType type) {
  if (activeSession_ != SessionType::None && activeSession_ != type) {
    return Status::Error(StatusCode::Busy, "audio session busy");
  }
  activeSession_ = type;
  return Status::OkStatus();
}

void AudioService::endSessionLocked(SessionType type) {
  if (activeSession_ == type) {
    activeSession_ = SessionType::None;
  }
}

Status AudioService::playBuiltIn(Melodies melody, MelodyOptions options) {
  Status st = lockState();
  if (!st.ok()) return st;

  st = ensureAudioReadyLocked();
  if (!st.ok()) {
    unlockState();
    return st;
  }

  st = beginSessionLocked(SessionType::BuiltIn);
  if (!st.ok()) {
    unlockState();
    return st;
  }
  unlockState();

  audioHal_.music().playMusic(melody, options);

  if (options == Once || options == Forever) {
    st = lockState();
    if (!st.ok()) return st;
    endSessionLocked(SessionType::BuiltIn);
    unlockState();
  }

  return Status::OkStatus();
}

Status AudioService::stopBuiltIn() {
  Status st = lockState();
  if (!st.ok()) return st;
  if (!boardHal_.isReady()) {
    unlockState();
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }

  audioHal_.music().stopPlayTone();
  endSessionLocked(SessionType::BuiltIn);
  unlockState();
  return Status::OkStatus();
}

Status AudioService::playFile(const String &path) {
  if (path.length() == 0) {
    return Status::Error(StatusCode::InvalidArgument, "path is empty");
  }
  String sdPath;
  if (!toSdFsPath(path, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "path must start with S:/");
  }

  Status st = lockState();
  if (!st.ok()) return st;

  st = ensureAudioReadyLocked();
  if (!st.ok()) {
    unlockState();
    return st;
  }

  if (activeSession_ == SessionType::File) {
    audioHal_.music().stopPlayAudio();
  }

  st = beginSessionLocked(SessionType::File);
  if (!st.ok()) {
    unlockState();
    return st;
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) {
    endSessionLocked(SessionType::File);
    unlockState();
    return st;
  }
  if (!SD.exists(sdPath)) {
    endSessionLocked(SessionType::File);
    unlockState();
    return Status::Error(StatusCode::IOError, "audio file not found");
  }
  unlockState();

  audioHal_.music().playTFCardAudio(path);
  return Status::OkStatus();
}

Status AudioService::stopFile() {
  Status st = lockState();
  if (!st.ok()) return st;
  if (!boardHal_.isReady()) {
    unlockState();
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }

  audioHal_.music().stopPlayAudio();
  endSessionLocked(SessionType::File);
  unlockState();
  return Status::OkStatus();
}

Status AudioService::recordFile(const String &path, uint8_t seconds) {
  if (path.length() == 0) {
    return Status::Error(StatusCode::InvalidArgument, "path is empty");
  }
  String sdPath;
  if (!toSdFsPath(path, &sdPath)) {
    return Status::Error(StatusCode::InvalidArgument, "path must start with S:/");
  }
  if (seconds == 0) {
    return Status::Error(StatusCode::InvalidArgument, "seconds must be > 0");
  }

  Status st = lockState();
  if (!st.ok()) return st;

  st = ensureAudioReadyLocked();
  if (!st.ok()) {
    unlockState();
    return st;
  }

  st = beginSessionLocked(SessionType::Recording);
  if (!st.ok()) {
    unlockState();
    return st;
  }

  st = ensureSdReadyNoLoop();
  if (!st.ok()) {
    endSessionLocked(SessionType::Recording);
    unlockState();
    return st;
  }

  // Remove stale file so failed writes cannot be mistaken as successful output.
  if (SD.exists(sdPath)) {
    (void)SD.remove(sdPath);
  }
  unlockState();

  audioHal_.music().recordSaveToTFCard(path, seconds);

  st = lockState();
  if (!st.ok()) return st;

  bool fileOk = false;
  File f = SD.open(sdPath, FILE_READ);
  if (f) {
    fileOk = f.size() > 44;
    f.close();
  }

  endSessionLocked(SessionType::Recording);
  unlockState();

  if (!fileOk) {
    return Status::Error(StatusCode::IOError, "record output not created");
  }

  return Status::OkStatus();
}

Status VisionService::init() {
  if (!boardHal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }

  Status st = lockState();
  if (!st.ok()) return st;

  if (initialized_) {
    unlockState();
    return Status::OkStatus();
  }

  auto status = visionHal_.init();
  if (!status.ok()) {
    unlockState();
    return status;
  }
  currentMode_ = AiMode::None;
  activeHalMode_ = AiMode::None;
  liveAimActive_ = false;
  liveFeedbackEnabled_ = true;
  initialized_ = true;
  modeSwitchCount_ = 0;
  unlockState();
  return Status::OkStatus();
}

Status VisionService::setMode(AiMode mode) {
  Status st = lockState();
  if (!st.ok()) return st;

  if (!initialized_) {
    unlockState();
    return Status::Error(StatusCode::NotInitialized, "vision not initialized");
  }

  AiMode targetHalMode = mapToHalMode(mode);
  bool commandMode = isFaceCommandMode(mode);

  // In capture workflow, keep only the selected AI mode and defer HAL loading
  // until a workflow that really needs live AI processing is active.
  if (workflowMode_ == VisionWorkflowMode::CaptureReview) {
    currentMode_ = mode;
    modeSwitchCount_++;
    unlockState();
    return Status::OkStatus();
  }

  if (mode == currentMode_ && targetHalMode == activeHalMode_ && !commandMode) {
    unlockState();
    return Status::OkStatus();
  }

  if (targetHalMode != activeHalMode_) {
    auto status = visionHal_.switchMode(targetHalMode);
    if (!status.ok()) {
      unlockState();
      return status;
    }
    activeHalMode_ = targetHalMode;
  }

  if (commandMode) {
    visionHal_.ai().sendFaceCmd(mapFaceCommand(mode));
  }

  currentMode_ = mode;
  modeSwitchCount_++;
  unlockState();
  return Status::OkStatus();
}

Status VisionService::setWorkflowMode(VisionWorkflowMode workflowMode) {
  Status st = lockState();
  if (!st.ok()) return st;

  if (!initialized_) {
    unlockState();
    return Status::Error(StatusCode::NotInitialized, "vision not initialized");
  }

  workflowMode_ = workflowMode;
  unlockState();
  return Status::OkStatus();
}

Status VisionService::startLiveAim(bool drawHints) {
  if (!cameraService_) {
    return Status::Error(StatusCode::NotSupported,
                         "live aim requires camera service linkage");
  }

  Status st = setWorkflowMode(VisionWorkflowMode::LiveAim);
  if (!st.ok()) return st;

  st = cameraService_->start();
  if (!st.ok()) {
    setWorkflowResult(false, "camera/live", st.message);
    return st;
  }

  liveFeedbackEnabled_ = drawHints;
  liveAimActive_ = true;
  if (displayService_) {
    (void)displayService_->setCameraBackground(true);
  }

  String summary = describeCurrentPerception();

  if (liveFeedbackEnabled_ && displayService_) {
    String title = fixedOverlayText("vision live aim", 22);
    String summaryLine = fixedOverlayText(summary, 28);
    String hint = fixedOverlayText("mire o alvo no centro", 26);

    (void)displayService_->clearCanvas();
    (void)displayService_->setFontSize(Canvas::eCNAndENFont16);
    (void)displayService_->textAt(title, 8, 10, 0xFFFFFF, 24, false);
    (void)displayService_->textAt(summaryLine, 8, 32, 0xCFE8FF, 32, false);
    (void)displayService_->drawLine(113, 160, 127, 160, 0x66FF66);
    (void)displayService_->drawLine(120, 153, 120, 167, 0x66FF66);
    (void)displayService_->textAt(hint, 8, 292, 0xDDEAFF, 28, false);
    (void)displayService_->update();
  } else if (displayService_) {
    // Keep plain camera preview when feedback overlay is disabled.
    (void)displayService_->clearCanvas();
    (void)displayService_->update();
  }

  setWorkflowResult(true, "camera/live", summary);

  if (liveFeedbackEnabled_) {
    ensureLiveFeedbackTask();
  } else {
    stopLiveFeedbackTask();
  }

  return Status::OkStatus();
}

Status VisionService::stopLiveAim() {
  if (!cameraService_) {
    return Status::Error(StatusCode::NotSupported,
                         "live aim requires camera service linkage");
  }

  liveAimActive_ = false;
  stopLiveFeedbackTask();

  Status st = cameraService_->stop();
  if (!st.ok()) {
    setWorkflowResult(false, "camera/live", st.message);
    return st;
  }

  // Leave live with AI pipeline idle to avoid background "searching" state.
  Status aiStop = setMode(AiMode::None);
  if (!aiStop.ok()) {
    setWorkflowResult(false, "camera/live", aiStop.message);
    return aiStop;
  }

  liveFeedbackEnabled_ = true;

  setWorkflowResult(true, "camera/live", "live aim stopped");
  return Status::OkStatus();
}

Status VisionService::setLiveFeedbackEnabled(bool enabled) {
  liveFeedbackEnabled_ = enabled;

  if (liveAimActive_ && enabled) {
    Status st = refreshLiveAimFeedback(true);
    if (!st.ok()) return st;
    ensureLiveFeedbackTask();
    return Status::OkStatus();
  }

  if (liveAimActive_ && !enabled && displayService_) {
    stopLiveFeedbackTask();
    // Keep camera preview, hide overlay text/reticle.
    (void)displayService_->setCameraBackground(true);
    (void)displayService_->clearCanvas();
    (void)displayService_->update();
  }

  return Status::OkStatus();
}

Status VisionService::setLiveFeedbackPeriodMs(uint32_t periodMs) {
  if (periodMs < 40 || periodMs > 2000) {
    return Status::Error(StatusCode::InvalidArgument,
                         "live feedback period must be in [40,2000] ms");
  }

  liveFeedbackPeriodMs_ = periodMs;
  return Status::OkStatus();
}

Status VisionService::refreshLiveAimFeedback(bool drawAimReticle) {
  if (!liveAimActive_) {
    return Status::Error(StatusCode::Busy, "live aim not active");
  }

  String summary = describeCurrentPerception();
  bool det = detected();

  if (!liveFeedbackEnabled_) {
    setWorkflowResult(true, "camera/live", summary);
    lastWorkflowResult_.detected = det;
    lastWorkflowResult_.recognized = recognized();
    lastWorkflowResult_.recognitionId = recognitionId();
    return Status::OkStatus();
  }

  if (displayService_) {
    String title = fixedOverlayText("vision live aim", 22);
    String summaryLine = fixedOverlayText(summary, 30);
    String statusLine = fixedOverlayText(det ? "detectado" : "varrendo cena", 24);
    uint32_t color = det ? 0x66FF66 : 0xCFE8FF;
    (void)displayService_->setFontSize(Canvas::eCNAndENFont16);
    (void)displayService_->textAt(title, 8, 10, 0xFFFFFF, 24, false);
    (void)displayService_->textAt(summaryLine, 8, 32, color, 34, false);
    if (drawAimReticle) {
      (void)displayService_->drawLine(113, 160, 127, 160, 0x66FF66);
      (void)displayService_->drawLine(120, 153, 120, 167, 0x66FF66);
    }
    (void)displayService_->textAt(statusLine, 8, 292, color, 28, false);
    (void)displayService_->update();
  }

  setWorkflowResult(true, "camera/live", summary);
  lastWorkflowResult_.detected = det;
  lastWorkflowResult_.recognized = recognized();
  lastWorkflowResult_.recognitionId = recognitionId();
  return Status::OkStatus();
}

String VisionService::describeCurrentPerception() {
  if (!initialized_) return String("vision nao inicializada");

  Status st = lockState();
  if (!st.ok()) return String("vision state busy");

  AiMode mode = currentMode_;
  VisionWorkflowMode workflow = workflowMode_;
  unlockState();

  return buildPerceptionSummary(mode, workflow);
}

Status VisionService::captureAndReview(const String &fileNameOrPath,
                                       framesize_t framesize,
                                       const String &dirOverride) {
  // TODO(capture): postpone deeper capture reliability tuning and static-image
  // AI dispatch implementation to a dedicated follow-up step.
  if (!cameraService_ || !storageService_) {
    return Status::Error(StatusCode::NotSupported,
                         "capture-review requires camera+storage linkage");
  }

  Status st = setWorkflowMode(VisionWorkflowMode::CaptureReview);
  if (!st.ok()) return st;

  String targetPath = fileNameOrPath;
  if (!targetPath.startsWith("S:/")) {
    targetPath = storageService_->imagePath(fileNameOrPath, dirOverride);
  }

  const String aiTempDir = "S:/data/ai_tmp";
  const String stagedAiPath = storageService_->dataPath(fileNameFromApiPath(targetPath), aiTempDir);

  // Current K10 stack is unstable when re-registering camera at new frame sizes
  // during workflow transitions. Keep capture path on the already registered
  // preview queue to avoid ESP_ERR_INVALID_STATE / reset loops.
  (void)framesize;
  st = cameraService_->start();
  if (!st.ok()) {
    setWorkflowResult(false, targetPath, st.message);
    return st;
  }

  st = cameraService_->capture(targetPath);
  (void)cameraService_->stop();
  if (!st.ok()) {
    setWorkflowResult(false, targetPath, st.message);
    return st;
  }

  size_t stagedBytes = 0;
  st = copyApiFile(targetPath, stagedAiPath, &stagedBytes);
  if (!st.ok()) {
    setWorkflowResult(false, stagedAiPath, st.message);
    return st;
  }

  if (displayService_) {
    (void)displayService_->clearCanvas();
    (void)displayService_->drawImage(0, 0, targetPath);
    (void)displayService_->setFontSize(Canvas::eCNAndENFont16);
    (void)displayService_->textAt("capture review", 8, 8, 0xFFFFFF, 22, true);
    (void)displayService_->update();
  }

  AiMode modeForDispatch = AiMode::None;
  Status modeSt = lockState();
  if (modeSt.ok()) {
    modeForDispatch = currentMode_;
    unlockState();
  }

  String summary = String("captured->") + targetPath +
                   String("; staged->") + stagedAiPath +
                   String("; mode=") + aiModeLabel(modeForDispatch);

  // Static-image inference for face/qr/cat/move is not available in current on-device stack.
  // Keep data staged for AI pipeline while preserving selected mode in telemetry.
  summary += String("; static-image inference pending");

  setWorkflowResult(true, stagedAiPath, summary, stagedBytes);
  lastWorkflowResult_.detected = detected();
  return Status::OkStatus();
}

Status VisionService::analyzeInputText(const String &payload) {
  String text = payload;
  text.trim();
  if (text.length() == 0) {
    setWorkflowResult(false, "text-inline", "empty text payload");
    return Status::Error(StatusCode::InvalidArgument, "empty text payload");
  }

  Status st = setWorkflowMode(VisionWorkflowMode::InputReader);
  if (!st.ok()) return st;

  String lower = text;
  for (size_t i = 0; i < lower.length(); i++) {
    char c = lower[i];
    if (c >= 'A' && c <= 'Z') lower.setCharAt(i, c + ('a' - 'A'));
  }

  String summary = "plain text";
  if (text.startsWith("{") || text.startsWith("[")) {
    summary = "json-like text";
  } else if (text.indexOf(',') >= 0 && text.indexOf('\n') >= 0) {
    summary = "csv-like text";
  } else if (lower.startsWith("http://") || lower.startsWith("https://")) {
    summary = "url text";
  }

  setWorkflowResult(true, "text-inline", summary, text.length());

  if (displayService_) {
    String preview = text.substring(0, 42);
    (void)displayService_->setBackground(0xFFFFFF);
    (void)displayService_->clearCanvas();
    (void)displayService_->textRow("input reader", 1, 0x000000);
    (void)displayService_->setFontSize(Canvas::eCNAndENFont16);
    (void)displayService_->textAt(summary, 10, 70, 0x006600, 28, true);
    (void)displayService_->textAt(preview, 10, 96, 0x000000, 36, true);
    (void)displayService_->update();
  }

  return Status::OkStatus();
}

Status VisionService::analyzeInputFile(const String &fileNameOrPath,
                                       const String &dirOverride) {
  if (!storageService_) {
    return Status::Error(StatusCode::NotSupported,
                         "input file reader requires storage linkage");
  }

  Status st = setWorkflowMode(VisionWorkflowMode::InputReader);
  if (!st.ok()) return st;

  bool found = false;
  String inputPath = resolveInputPath(fileNameOrPath, &found);

  if (!found && dirOverride.length() > 0) {
    String overridePath = storageService_->dataPath(fileNameOrPath, dirOverride);
    bool exists = false;
    (void)storageService_->fileExists(overridePath, &exists);
    if (exists) {
      inputPath = overridePath;
      found = true;
    }
  }

  if (!found || inputPath.length() == 0) {
    setWorkflowResult(false, fileNameOrPath, "input file not found");
    return Status::Error(StatusCode::IOError, "input file not found");
  }

  String ext = extensionLower(inputPath);
  if (isTextExtension(ext)) {
    String content;
    st = storageService_->readTextFile(inputPath, &content, 12288);
    if (!st.ok()) {
      setWorkflowResult(false, inputPath, st.message);
      return st;
    }

    String summary = String("text file (") + ext + ")";
    setWorkflowResult(true, inputPath, summary, storageService_->lastReadBytes());

    if (displayService_) {
      String preview = content.substring(0, 42);
      (void)displayService_->setBackground(0xFFFFFF);
      (void)displayService_->clearCanvas();
      (void)displayService_->textRow("input file", 1, 0x000000);
      (void)displayService_->setFontSize(Canvas::eCNAndENFont16);
      (void)displayService_->textAt(summary, 10, 70, 0x006600, 28, true);
      (void)displayService_->textAt(preview, 10, 96, 0x000000, 36, true);
      (void)displayService_->update();
    }
    return Status::OkStatus();
  }

  if (isImageExtension(ext)) {
    if (displayService_) {
      (void)displayService_->clearCanvas();
      (void)displayService_->drawImage(0, 0, inputPath);
      (void)displayService_->setFontSize(Canvas::eCNAndENFont16);
      (void)displayService_->textAt("image input loaded", 8, 8, 0xFFFFFF, 26, true);
      (void)displayService_->update();
    }
    setWorkflowResult(true, inputPath, "image file loaded");
    return Status::OkStatus();
  }

  uint32_t size = 0;
  st = storageService_->fileSize(inputPath, &size);
  if (!st.ok()) {
    setWorkflowResult(false, inputPath, st.message);
    return st;
  }

  String summary = isAudioExtension(ext) ? "audio file indexed" : "binary file indexed";
  setWorkflowResult(true, inputPath, summary, size);

  if (displayService_) {
    (void)displayService_->setBackground(0xFFFFFF);
    (void)displayService_->clearCanvas();
    (void)displayService_->textRow("input file", 1, 0x000000);
    (void)displayService_->setFontSize(Canvas::eCNAndENFont16);
    (void)displayService_->textAt(summary, 10, 70, 0x006600, 28, true);
    (void)displayService_->textAt(String("bytes: ") + String((unsigned long)size),
                                  10, 96, 0x000000, 30, true);
    (void)displayService_->update();
  }

  return Status::OkStatus();
}

Status VisionService::analyzeInputBinary(const uint8_t *data, size_t bytes) {
  if (data == nullptr || bytes == 0) {
    setWorkflowResult(false, "memory-binary", "invalid binary payload");
    return Status::Error(StatusCode::InvalidArgument, "invalid binary payload");
  }

  Status st = setWorkflowMode(VisionWorkflowMode::InputReader);
  if (!st.ok()) return st;

  uint32_t checksum = 0;
  for (size_t i = 0; i < bytes; i++) checksum += data[i];

  String preview;
  for (size_t i = 0; i < bytes && i < 8; i++) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02X ", data[i]);
    preview += buf;
  }

  String summary = String("binary checksum=") + String((unsigned long)checksum);
  setWorkflowResult(true, "memory-binary", summary, bytes);

  if (displayService_) {
    (void)displayService_->setBackground(0xFFFFFF);
    (void)displayService_->clearCanvas();
    (void)displayService_->textRow("input binary", 1, 0x000000);
    (void)displayService_->setFontSize(Canvas::eCNAndENFont16);
    (void)displayService_->textAt(summary, 10, 70, 0x006600, 30, true);
    (void)displayService_->textAt(preview, 10, 96, 0x000000, 36, true);
    (void)displayService_->update();
  }

  return Status::OkStatus();
}

Status VisionService::analyzeInputAny(const String &payloadOrPath) {
  bool found = false;
  String path = resolveInputPath(payloadOrPath, &found);
  if (found && path.length() > 0) {
    return analyzeInputFile(path);
  }

  if (payloadOrPath.startsWith("S:/")) {
    setWorkflowResult(false, payloadOrPath, "input file not found");
    return Status::Error(StatusCode::IOError, "input file not found");
  }

  return analyzeInputText(payloadOrPath);
}

Status VisionService::runOcrOnInput(const String &payloadOrPath, String *outText) {
  Status st = setWorkflowMode(VisionWorkflowMode::Ocr);
  if (!st.ok()) return st;

  st = setMode(AiMode::Ocr);
  if (!st.ok()) return st;

  if (outText) {
    outText->remove(0);
  }

  setWorkflowResult(false, payloadOrPath,
                    "OCR engine not available in current firmware", 0, false);
  return Status::Error(StatusCode::NotSupported,
                       "OCR engine not available in current firmware");
}

bool VisionService::detected() {
  if (!initialized_) return false;

  Status st = lockState();
  if (!st.ok()) return false;

  AiMode activeMode = currentMode_;
  unlockState();

  auto &ai = visionHal_.ai();
  switch (activeMode) {
    case AiMode::Face:
    case AiMode::FaceRecognize:
    case AiMode::FaceEnroll:
    case AiMode::FaceDeleteAll:
      return ai.isDetectContent(AIRecognition::Face);
    case AiMode::Cat:
      return ai.isDetectContent(AIRecognition::Cat);
    case AiMode::Move:
      return ai.isDetectContent(AIRecognition::Move);
    case AiMode::Code:
      return ai.isDetectContent(AIRecognition::Code);
    case AiMode::Ocr:
      return false;
    case AiMode::None:
    default:
      return false;
  }
}

bool VisionService::recognized() {
  if (!initialized_) return false;
  return visionHal_.ai().isRecognized();
}

int VisionService::recognitionId() {
  if (!initialized_) return -1;
  return visionHal_.ai().getRecognitionID();
}

Status VisionService::setMotionThreshold(uint8_t threshold) {
  if (!initialized_) {
    return Status::Error(StatusCode::NotInitialized, "vision not initialized");
  }
  if (threshold < 10 || threshold > 200) {
    return Status::Error(StatusCode::InvalidArgument,
                         "motion threshold must be in [10,200]");
  }
  visionHal_.ai().setMotinoThreshold(threshold);
  return Status::OkStatus();
}

int VisionService::faceData(AIRecognition::eFaceOrCatData_t type) {
  if (!initialized_) return 0;
  return visionHal_.ai().getFaceData(type);
}

int VisionService::catData(AIRecognition::eFaceOrCatData_t type) {
  if (!initialized_) return 0;
  return visionHal_.ai().getCatData(type);
}

String VisionService::qrPayload() {
  if (!initialized_) return String();
  return visionHal_.ai().getQrCodeContent();
}

Status VisionService::lockState() {
  if (!stateMutex_) {
    return Status::Error(StatusCode::IOError, "vision mutex unavailable");
  }
  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(300)) != pdTRUE) {
    return Status::Error(StatusCode::Busy, "vision state busy");
  }
  return Status::OkStatus();
}

void VisionService::unlockState() {
  if (stateMutex_) {
    xSemaphoreGive(stateMutex_);
  }
}

bool VisionService::isFaceCommandMode(AiMode mode) const {
  switch (mode) {
    case AiMode::FaceRecognize:
    case AiMode::FaceEnroll:
    case AiMode::FaceDeleteAll:
      return true;
    default:
      return false;
  }
}

AiMode VisionService::mapToHalMode(AiMode mode) const {
  switch (mode) {
    case AiMode::Face:
    case AiMode::FaceRecognize:
    case AiMode::FaceEnroll:
    case AiMode::FaceDeleteAll:
      return AiMode::Face;
    case AiMode::Cat:
      return AiMode::Cat;
    case AiMode::Move:
      return AiMode::Move;
    case AiMode::Code:
      return AiMode::Code;
    case AiMode::Ocr:
      return AiMode::None;
    case AiMode::None:
    default:
      return AiMode::None;
  }
}

recognizer_state_t VisionService::mapFaceCommand(AiMode mode) const {
  switch (mode) {
    case AiMode::FaceEnroll:
      return ENROLL;
    case AiMode::FaceDeleteAll:
      return DELETEALL;
    case AiMode::FaceRecognize:
    default:
      return RECOGNIZE;
  }
}

bool VisionService::isTextExtension(const String &extLower) const {
  return extLower == "txt" || extLower == "json" || extLower == "csv" ||
         extLower == "log" || extLower == "md";
}

bool VisionService::isImageExtension(const String &extLower) const {
  return extLower == "bmp" || extLower == "jpg" || extLower == "jpeg" ||
         extLower == "png" || extLower == "gif";
}

bool VisionService::isAudioExtension(const String &extLower) const {
  return extLower == "wav" || extLower == "pcm";
}

String VisionService::extensionLower(const String &path) const {
  int dot = path.lastIndexOf('.');
  if (dot < 0 || dot + 1 >= (int)path.length()) {
    return String();
  }

  String ext = path.substring(dot + 1);
  for (size_t i = 0; i < ext.length(); i++) {
    char c = ext[i];
    if (c >= 'A' && c <= 'Z') ext.setCharAt(i, c + ('a' - 'A'));
  }
  return ext;
}

String VisionService::resolveInputPath(const String &fileNameOrPath, bool *found) const {
  if (found) *found = false;
  if (!storageService_) return String();

  auto checkPath = [&](const String &p) -> bool {
    bool exists = false;
    Status st = storageService_->fileExists(p, &exists);
    return st.ok() && exists;
  };

  if (fileNameOrPath.startsWith("S:/")) {
    bool exists = checkPath(fileNameOrPath);
    if (found) *found = exists;
    return exists ? fileNameOrPath : String();
  }

  String candidates[4] = {
    storageService_->dataPath(fileNameOrPath),
    storageService_->imagePath(fileNameOrPath),
    storageService_->audioPath(fileNameOrPath),
    String("S:/") + fileNameOrPath,
  };

  for (size_t i = 0; i < 4; i++) {
    if (checkPath(candidates[i])) {
      if (found) *found = true;
      return candidates[i];
    }
  }

  return String();
}

String VisionService::buildPerceptionSummary(AiMode mode, VisionWorkflowMode workflowMode) {
  auto &ai = visionHal_.ai();

  if (workflowMode == VisionWorkflowMode::CaptureReview) {
    return String("Captura pronta (IA apos foto)");
  }

  if (ai.isDetectContent(AIRecognition::Code)) {
    String payload = ai.getQrCodeContent();
    payload.replace('\n', ' ');
    payload.trim();
    if (payload.length() == 0) {
      return String("QR detectado");
    }
    if (payload.length() > 24) {
      payload = payload.substring(0, 24) + "...";
    }
    return String("QR: ") + payload;
  }

  if (ai.isDetectContent(AIRecognition::Face)) {
    if (ai.isRecognized()) {
      int id = ai.getRecognitionID();
      if (id >= 0) {
        return String("Rosto reconhecido id=") + String(id);
      }
      return String("Rosto reconhecido");
    }
    return String("Rosto detectado");
  }

  if (ai.isDetectContent(AIRecognition::Cat)) {
    return String("Objeto/cat detectado");
  }

  if (ai.isDetectContent(AIRecognition::Move)) {
    return String("Movimento detectado");
  }

  if (workflowMode == VisionWorkflowMode::Ocr) {
    return String("Texto: OCR indisponivel");
  }

  switch (mode) {
    case AiMode::Code:
      return String("Procurando QR...");
    case AiMode::Ocr:
      return String("Texto: OCR indisponivel");
    case AiMode::Face:
    case AiMode::FaceRecognize:
    case AiMode::FaceEnroll:
    case AiMode::FaceDeleteAll:
      return String("Procurando rosto...");
    case AiMode::Cat:
      return String("Procurando objeto/cat...");
    case AiMode::Move:
      return String("Procurando movimento...");
    case AiMode::None:
    default:
      return String("IA aguardando modo (texto requer OCR)");
  }
}

void VisionService::liveFeedbackTaskThunk(void *arg) {
  VisionService *self = static_cast<VisionService *>(arg);
  if (!self) {
    vTaskDelete(nullptr);
    return;
  }

  while (self->liveAimActive_ &&
         self->liveFeedbackEnabled_ &&
         !self->liveFeedbackStopRequested_) {
    (void)self->refreshLiveAimFeedback(true);
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(self->liveFeedbackPeriodMs_)) > 0) {
      break;
    }
  }

  self->liveFeedbackTask_ = nullptr;
  vTaskDelete(nullptr);
}

void VisionService::ensureLiveFeedbackTask() {
  if (liveFeedbackTask_ != nullptr) {
    return;
  }

  liveFeedbackStopRequested_ = false;

  BaseType_t ok = xTaskCreate(&VisionService::liveFeedbackTaskThunk,
                              "vision_feedback",
                              kVisionFeedbackTaskStackBytes,
                              this,
                              1,
                              &liveFeedbackTask_);
  if (ok != pdPASS) {
    liveFeedbackTask_ = nullptr;
  }
}

void VisionService::stopLiveFeedbackTask() {
  liveFeedbackStopRequested_ = true;

  TaskHandle_t task = liveFeedbackTask_;
  if (task != nullptr) {
    xTaskNotifyGive(task);

    uint32_t t0 = millis();
    while (liveFeedbackTask_ != nullptr && (millis() - t0) < 800) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

void VisionService::setWorkflowResult(bool ok,
                                      const String &source,
                                      const String &summary,
                                      size_t analyzedBytes,
                                      bool ocrSupported) {
  lastWorkflowResult_.ok = ok;
  lastWorkflowResult_.workflow = workflowMode_;
  lastWorkflowResult_.mode = currentMode_;
  lastWorkflowResult_.detected = initialized_ ? detected() : false;
  lastWorkflowResult_.recognized = initialized_ ? recognized() : false;
  lastWorkflowResult_.recognitionId = lastWorkflowResult_.recognized ? recognitionId() : -1;
  lastWorkflowResult_.ocrSupported = ocrSupported;
  lastWorkflowResult_.analyzedBytes = analyzedBytes;
  lastWorkflowResult_.source = source;
  lastWorkflowResult_.summary = summary;
}

void SpeechService::begin(uint8_t mode, uint8_t lang, uint16_t wakeUpMs) {
  speechHal_.asr().asrInit(mode, lang, wakeUpMs);
}

void SpeechService::addCommand(uint8_t id, const String &phrase) {
  speechHal_.asr().addASRCommand(id, phrase);
}

bool SpeechService::detectCommand(uint8_t id) { return speechHal_.asr().isDetectCmdID(id); }

void SpeechService::speak(const String &text) { speechHal_.asr().speak(text); }

}  // namespace unihiker_pro
