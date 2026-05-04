#include "services.h"

#include <Arduino.h>
#include <Preferences.h>
#include <asr.h>
#include <esp_camera.h>
#include <img_converters.h>
#include <SD.h>
#include <unihiker_k10.h>
#include <who_camera.h>

extern "C" void lv_fs_fatfs_init(void);

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

Status DisplayService::createCanvas() {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  hal_.board().creatCanvas();
  return Status::OkStatus();
}

Status DisplayService::destroyCanvas() {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::OkStatus();
  }
  hal_.board().canvas->canvasClear();
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
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasClear();
  return Status::OkStatus();
}

Status DisplayService::clearRow(uint8_t row) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasClear(row);
  return Status::OkStatus();
}

Status DisplayService::clearRegion(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->clearLocalCanvas(x, y, w, h);
  return Status::OkStatus();
}

Status DisplayService::setLineWidth(uint8_t w) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasSetLineWidth(w);
  return Status::OkStatus();
}

Status DisplayService::setFontSize(Canvas::eFontSize_t font) {
  font_ = font;
  return Status::OkStatus();
}

Status DisplayService::drawPoint(int16_t x, int16_t y, uint32_t color) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasPoint(x, y, color);
  return Status::OkStatus();
}

Status DisplayService::drawLine(int x1, int y1, int x2, int y2, uint32_t color) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasLine(x1, y1, x2, y2, color);
  return Status::OkStatus();
}

Status DisplayService::drawCircle(int x, int y, int r, uint32_t color,
                                  uint32_t bgColor, bool fill) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasCircle(x, y, r, color, bgColor, fill);
  return Status::OkStatus();
}

Status DisplayService::drawRect(int x, int y, int w, int h, uint32_t color,
                                uint32_t bgColor, bool fill) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasRectangle(x, y, w, h, color, bgColor, fill);
  return Status::OkStatus();
}

Status DisplayService::drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                                  const uint8_t *bitmap) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasDrawBitmap(x, y, w, h, bitmap);
  return Status::OkStatus();
}

Status DisplayService::drawImage(int16_t x, int16_t y, const String &path) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasDrawImage(x, y, path);
  return Status::OkStatus();
}

Status DisplayService::textRow(const String &text, uint8_t row, uint32_t color) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasText(text, row, color);
  return Status::OkStatus();
}

Status DisplayService::textAt(const String &text, int16_t x, int16_t y, uint32_t color,
                              int count, bool autoClean) {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->canvasText(text, x, y, color, font_, count, autoClean);
  return Status::OkStatus();
}

Status DisplayService::update() {
  if (!hal_.isReady() || hal_.board().canvas == nullptr) {
    return Status::Error(StatusCode::NotInitialized, "canvas not initialized");
  }
  hal_.board().canvas->updateCanvas();
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

float SensorService::temperatureC() { return hal_.aht20().getData(AHT20::eAHT20TempC); }

float SensorService::humidityRh() { return hal_.aht20().getData(AHT20::eAHT20HumiRH); }

uint16_t SensorService::ambientLux() { return hal_.board().readALS(); }

int SensorService::accelX() { return hal_.board().getAccelerometerX(); }

int SensorService::accelY() { return hal_.board().getAccelerometerY(); }

int SensorService::accelZ() { return hal_.board().getAccelerometerZ(); }

uint64_t SensorService::micLevel() { return hal_.board().readMICData(); }

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

Status AudioService::playBuiltIn(Melodies melody, MelodyOptions options) {
  if (!boardHal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  audioHal_.music().playMusic(melody, options);
  return Status::OkStatus();
}

Status AudioService::stopBuiltIn() {
  audioHal_.music().stopPlayTone();
  return Status::OkStatus();
}

Status AudioService::playFile(const String &path) {
  audioHal_.music().playTFCardAudio(path);
  return Status::OkStatus();
}

Status AudioService::stopFile() {
  audioHal_.music().stopPlayAudio();
  return Status::OkStatus();
}

Status AudioService::recordFile(const String &path, uint8_t seconds) {
  audioHal_.music().recordSaveToTFCard(path, seconds);
  return Status::OkStatus();
}

Status VisionService::init() {
  auto status = visionHal_.init();
  if (!status.ok()) {
    return status;
  }
  return Status::OkStatus();
}

Status VisionService::setMode(AiMode mode) {
  auto status = visionHal_.switchMode(mode);
  if (!status.ok()) {
    return status;
  }
  currentMode_ = mode;
  return Status::OkStatus();
}

bool VisionService::detected() {
  auto &ai = visionHal_.ai();
  switch (currentMode_) {
    case AiMode::Face:
      return ai.isDetectContent(AIRecognition::Face);
    case AiMode::Cat:
      return ai.isDetectContent(AIRecognition::Cat);
    case AiMode::Move:
      return ai.isDetectContent(AIRecognition::Move);
    case AiMode::Code:
      return ai.isDetectContent(AIRecognition::Code);
    case AiMode::None:
    default:
      return false;
  }
}

int VisionService::faceData(AIRecognition::eFaceOrCatData_t type) {
  return visionHal_.ai().getFaceData(type);
}

int VisionService::catData(AIRecognition::eFaceOrCatData_t type) {
  return visionHal_.ai().getCatData(type);
}

String VisionService::qrPayload() { return visionHal_.ai().getQrCodeContent(); }

void SpeechService::begin(uint8_t mode, uint8_t lang, uint16_t wakeUpMs) {
  speechHal_.asr().asrInit(mode, lang, wakeUpMs);
}

void SpeechService::addCommand(uint8_t id, const String &phrase) {
  speechHal_.asr().addASRCommand(id, phrase);
}

bool SpeechService::detectCommand(uint8_t id) { return speechHal_.asr().isDetectCmdID(id); }

void SpeechService::speak(const String &text) { speechHal_.asr().speak(text); }

}  // namespace unihiker_pro
