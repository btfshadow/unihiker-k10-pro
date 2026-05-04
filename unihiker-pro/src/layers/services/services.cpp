#include "services.h"

#include <asr.h>
#include <esp_camera.h>
#include <SD.h>
#include <unihiker_k10.h>

namespace unihiker_pro {

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

Status CameraService::initPreview() {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  hal_.board().initBgCamerImage();
  hal_.board().setBgCamerImage(true);
  previewActive_ = true;
  return Status::OkStatus();
}

Status CameraService::showPreview(bool enabled) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
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

  if (framesize != FRAMESIZE_QVGA) {
    return Status::Error(
        StatusCode::NotSupported,
        "high-res capture is not supported with active SDK preview pipeline");
  }

  if (onProgress) onProgress(0);
  // Descarta um frame antigo para reduzir ghosting.
  camera_fb_t *stale = esp_camera_fb_get();
  if (stale) {
    esp_camera_fb_return(stale);
  }
  if (onProgress) onProgress(20);

  // Captura frame no modo atual (QVGA no pipeline legado)
  camera_fb_t *fb = esp_camera_fb_get();
  Status result = Status::OkStatus();
  if (fb == nullptr) {
    result = Status::Error(StatusCode::IOError, "failed to capture frame");
  } else {
    if (onProgress) onProgress(35);
    result = writeBmpToSd(path, fb, onProgress);
    esp_camera_fb_return(fb);
  }

  if (onProgress) onProgress(95);
  if (onProgress) onProgress(100);
  return result;
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
  hal_.board().initSDFile();
  return Status::OkStatus();
}

Status StorageService::savePhotoBmp(const String &path) {
  if (!hal_.isReady()) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
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
