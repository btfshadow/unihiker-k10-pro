#include <Arduino.h>
#include <Preferences.h>
#include <SD.h>
#include <esp_camera.h>
#include <img_converters.h>
#include <string.h>
#include <who_camera.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;
Preferences g_prefs;

static const char *kPrefsNs = "cam_live_hires";
static const char *kRoleKey = "role";
static const char *kResKey = "res_idx";
static const char *kPhotoKey = "photo_idx";

enum class BootRole : uint8_t {
  Preview = 0,
  CaptureOnce = 1,
};

struct CaptureMode {
  framesize_t size;
  const char *name;
};

enum class ColorMode : uint8_t {
  Rgb565 = 0,
  Rgb565ByteSwapped,
  Bgr565,
  Bgr565ByteSwapped,
};

struct DecodeMode {
  const char *name;
  ColorMode colorMode;
};

static const CaptureMode kCaptureModes[] = {
  {FRAMESIZE_QVGA, "QVGA 320x240"},
  {FRAMESIZE_VGA, "VGA 640x480"},
  {FRAMESIZE_SVGA, "SVGA 800x600"},
  {FRAMESIZE_XGA, "XGA 1024x768"},
  {FRAMESIZE_SXGA, "SXGA 1280x1024"},
  {FRAMESIZE_UXGA, "UXGA 1600x1200"},
};

static QueueHandle_t g_captureQueue = nullptr;
static uint8_t g_captureResIndex = 0;
static int32_t g_photoIndex = 0;
static BootRole g_role = BootRole::Preview;
static uint8_t g_decodeModeIndex = 0;

static const DecodeMode kDecodeModes[] = {
  {"RGB565", ColorMode::Rgb565},
};

static uint8_t modeCount() {
  return (uint8_t)(sizeof(kCaptureModes) / sizeof(kCaptureModes[0]));
}

static uint8_t clampModeIndex(uint32_t raw) {
  uint8_t count = modeCount();
  if (count == 0) return 0;
  if (raw >= count) return (uint8_t)(count - 1);
  return (uint8_t)raw;
}

static bool prefsBegin(bool readOnly) {
  return g_prefs.begin(kPrefsNs, readOnly);
}

static void loadStateFromPrefs() {
  uint8_t defaultRes = 2;  // SXGA default
  if (defaultRes >= modeCount()) defaultRes = 0;

  if (!prefsBegin(true)) {
    g_role = BootRole::Preview;
    g_captureResIndex = defaultRes;
    g_photoIndex = 0;
    return;
  }

  uint32_t roleRaw = g_prefs.getUInt(kRoleKey, (uint32_t)BootRole::Preview);
  uint32_t resRaw = g_prefs.getUInt(kResKey, defaultRes);
  uint32_t idxRaw = g_prefs.getUInt(kPhotoKey, 0);
  g_prefs.end();

  g_role = (roleRaw == (uint32_t)BootRole::CaptureOnce) ? BootRole::CaptureOnce : BootRole::Preview;
  g_captureResIndex = clampModeIndex(resRaw);
  g_photoIndex = (int32_t)idxRaw;
}

static bool saveRole(BootRole role) {
  if (!prefsBegin(false)) return false;
  size_t n = g_prefs.putUInt(kRoleKey, (uint32_t)role);
  g_prefs.end();
  return n > 0;
}

static bool saveResIndex(uint8_t index) {
  if (!prefsBegin(false)) return false;
  size_t n = g_prefs.putUInt(kResKey, (uint32_t)clampModeIndex(index));
  g_prefs.end();
  return n > 0;
}

static bool savePhotoIndex(int32_t idx) {
  if (!prefsBegin(false)) return false;
  size_t n = g_prefs.putUInt(kPhotoKey, (uint32_t)(idx < 0 ? 0 : idx));
  g_prefs.end();
  return n > 0;
}

static bool ensureSdReady() {
  for (int i = 0; i < 5; i++) {
    if (SD.begin()) return true;
    delay(120);
  }
  return false;
}

static String sanitize(const String &input) {
  String out;
  out.reserve(input.length());
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      out += c;
    } else {
      out += '_';
    }
  }
  while (out.indexOf("__") >= 0) out.replace("__", "_");
  if (out.length() == 0) out = "mode";
  return out;
}

static String buildCapturePath(const CaptureMode &mode, int32_t w, int32_t h, int32_t photoIndex) {
  return String("/") + sanitize(mode.name) + "_" + String(w) + "x" + String(h) + "_" + String(photoIndex) + ".bmp";
}

static bool initCaptureQueue(const CaptureMode &mode) {
  if (g_captureQueue == nullptr) {
    g_captureQueue = xQueueCreate(2, sizeof(camera_fb_t *));
    if (g_captureQueue == nullptr) return false;
  }
  register_camera(PIXFORMAT_RGB565, mode.size, 1, g_captureQueue);
  return true;
}

static void drainCaptureQueue() {
  if (g_captureQueue == nullptr) return;
  camera_fb_t *stale = nullptr;
  while (xQueueReceive(g_captureQueue, &stale, 0) == pdTRUE) {
    if (stale) esp_camera_fb_return(stale);
  }
}

static camera_fb_t *captureFrame(uint8_t retries, const char *modeName) {
  if (g_captureQueue == nullptr) return nullptr;

  for (uint8_t a = 0; a < retries; a++) {
    drainCaptureQueue();

    camera_fb_t *fb = nullptr;
    if (xQueueReceive(g_captureQueue, &fb, pdMS_TO_TICKS(1500)) == pdTRUE && fb) {
      USBSerial.printf("capture ok attempt=%u mode=%s frame=%ux%u len=%u fmt=%d\n",
                       (unsigned)(a + 1),
                       modeName,
                       (unsigned)fb->width,
                       (unsigned)fb->height,
                       (unsigned)fb->len,
                       (int)fb->format);
      return fb;
    }

    USBSerial.printf("capture timeout attempt=%u mode=%s\n",
                     (unsigned)(a + 1), modeName);
    delay(60);
  }

  return nullptr;
}

static void warmupAndDiscardFrames(uint32_t settleMs) {
  if (g_captureQueue == nullptr) return;

  uint32_t deadline = millis() + settleMs;
  uint32_t discarded = 0;

  while ((int32_t)(deadline - millis()) > 0) {
    camera_fb_t *fb = nullptr;
    if (xQueueReceive(g_captureQueue, &fb, pdMS_TO_TICKS(120)) == pdTRUE && fb != nullptr) {
      esp_camera_fb_return(fb);
      discarded++;
    }
  }

  USBSerial.printf("warmup done: discarded=%u frames in %u ms\n",
                   (unsigned)discarded, (unsigned)settleMs);
}

static uint16_t applyColorMode(uint16_t px, ColorMode mode) {
  uint16_t value = px;

  if (mode == ColorMode::Rgb565ByteSwapped || mode == ColorMode::Bgr565ByteSwapped) {
    value = (uint16_t)((value >> 8) | (value << 8));
  }

  if (mode == ColorMode::Bgr565 || mode == ColorMode::Bgr565ByteSwapped) {
    value = (uint16_t)(((value & 0x001F) << 11) |
                       (value & 0x07E0) |
                       ((value & 0xF800) >> 11));
  }

  return value;
}

static bool saveBmp(const char *path, camera_fb_t *fb) {
  int32_t w = (int32_t)fb->width;
  int32_t h = (int32_t)fb->height;
  size_t srcLen = (size_t)fb->len;

  const DecodeMode &decode = kDecodeModes[g_decodeModeIndex];
  if (decode.colorMode != ColorMode::Rgb565) {
    uint16_t *pixels = (uint16_t *)fb->buf;
    size_t pxCount = (size_t)w * (size_t)h;
    for (size_t i = 0; i < pxCount; i++) {
      pixels[i] = applyColorMode(pixels[i], decode.colorMode);
    }
  }

  uint8_t *bmpBuf = nullptr;
  size_t bmpLen = 0;
  bool convOk = fmt2bmp(fb->buf, srcLen, (uint16_t)w, (uint16_t)h, PIXFORMAT_RGB565, &bmpBuf, &bmpLen);
  esp_camera_fb_return(fb);

  if (!convOk || bmpBuf == nullptr || bmpLen == 0) {
    if (bmpBuf) free(bmpBuf);
    return false;
  }

  if (SD.exists(path)) SD.remove(path);
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    free(bmpBuf);
    return false;
  }

  const size_t chunk = 4096;
  size_t written = 0;
  while (written < bmpLen) {
    size_t n = bmpLen - written;
    if (n > chunk) n = chunk;
    if (f.write(bmpBuf + written, n) != n) {
      f.close();
      free(bmpBuf);
      return false;
    }
    written += n;
  }

  f.flush();
  f.close();
  free(bmpBuf);
  return true;
}

static bool runCaptureOnce(String &savedPath, String &err) {
  const CaptureMode &mode = kCaptureModes[g_captureResIndex];

  if (!ensureSdReady()) {
    err = "SD not ready";
    return false;
  }
  if (!initCaptureQueue(mode)) {
    err = "camera init failed";
    return false;
  }

  // Warmup strategy: flush queue + allow AE/AWB settle on fresh boot.
  drainCaptureQueue();
  warmupAndDiscardFrames(1500);
  drainCaptureQueue();

  camera_fb_t *fb = captureFrame(3, mode.name);
  if (!fb) {
    err = "capture timeout";
    return false;
  }

  if (fb->format != PIXFORMAT_RGB565) {
    err = "non-RGB565 frame";
    esp_camera_fb_return(fb);
    return false;
  }

  int32_t w = (int32_t)fb->width;
  int32_t h = (int32_t)fb->height;
  size_t expected = (size_t)w * (size_t)h * 2U;
  if ((size_t)fb->len != expected) {
    err = "frame len mismatch";
    esp_camera_fb_return(fb);
    return false;
  }

  g_photoIndex++;
  savedPath = String("/") + sanitize(mode.name) + "_" + sanitize(kDecodeModes[g_decodeModeIndex].name) +
              "_" + String(w) + "x" + String(h) + "_" + String(g_photoIndex) + ".bmp";
  bool ok = saveBmp(savedPath.c_str(), fb);
  if (!ok) {
    err = "save failed";
    return false;
  }

  (void)savePhotoIndex(g_photoIndex);
  return true;
}

static void drawPreviewUi() {
  board.display().setBackground(0x101018);
  board.display().clearCanvas();
  board.display().textRow("live + hires capture", 1, 0xFFFFFF);
  board.display().textAt(String("Live: QVGA background"), 10, 42, 0xBBBBBB, 24, true);
  board.display().textAt(String("Capture: ") + kCaptureModes[g_captureResIndex].name, 10, 72, 0xFFFFFF, 26, true);
  board.display().textAt("A: next capture res", 10, 118, 0x66CCFF, 22, true);
  board.display().textAt("B: capture (reboot)", 10, 150, 0x66CCFF, 22, true);
  board.display().textAt("(capture reboots back)", 10, 182, 0x8FAED9, 20, true);
  board.display().update();
}

void onButtonA() {
  uint8_t next = (uint8_t)((g_captureResIndex + 1) % modeCount());
  g_captureResIndex = next;
  (void)saveResIndex(g_captureResIndex);
  drawPreviewUi();
  USBSerial.printf("next capture mode: %s\n", kCaptureModes[g_captureResIndex].name);
}

void onButtonB() {
  board.display().clearCanvas();
  board.display().textAt("Preparing capture", 10, 50, 0x00FF66, 20, true);
  board.display().textAt("Rebooting...", 10, 90, 0xFFFFFF, 24, true);
  board.display().update();

  bool okRole = saveRole(BootRole::CaptureOnce);
  bool okRes = saveResIndex(g_captureResIndex);
  USBSerial.printf("capture trigger role=%d saveRole=%d saveRes=%d\n", (int)BootRole::CaptureOnce, (int)okRole, (int)okRes);
  delay(300);
  ESP.restart();
}

static void setupPreviewMode() {
  drawPreviewUi();
  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);
  USBSerial.printf("preview mode active, capture=%s\n", kCaptureModes[g_captureResIndex].name);
}

static void setupCaptureMode() {
  board.display().setBackground(0x121212);
  board.display().clearCanvas();
  board.display().textAt("Capturing Hi-Res", 10, 30, 0x00FF66, 20, true);
  board.display().textAt(String("Mode: ") + kCaptureModes[g_captureResIndex].name, 10, 64, 0xFFFFFF, 24, true);
  board.display().textAt("Please wait...", 10, 98, 0xCCCCCC, 24, true);
  board.display().update();

  String path;
  String err;
  bool ok = runCaptureOnce(path, err);

  if (ok) {
    USBSerial.printf("saved: S:%s\n", path.c_str());
    board.display().clearCanvas();
    board.display().textAt("Capture OK", 10, 26, 0x00FF66, 20, true);
    board.display().textAt(String("S:") + path, 10, 62, 0xFFFFFF, 28, true);
    board.display().textAt("Returning to live...", 10, 168, 0x66CCFF, 22, true);
  } else {
    USBSerial.printf("capture failed: %s\n", err.c_str());
    board.display().clearCanvas();
    board.display().textAt("Capture FAIL", 10, 26, 0xFF4444, 20, true);
    board.display().textAt(err, 10, 62, 0xFFFFFF, 26, true);
    board.display().textAt("Back to live anyway", 10, 168, 0xFFCC66, 22, true);
  }
  board.display().update();

  (void)saveRole(BootRole::Preview);
  delay(900);
  ESP.restart();
}

void setup() {
  USBSerial.begin(115200);
  unsigned long t0 = millis();
  while (!USBSerial && millis() - t0 < 6000) delay(10);

  loadStateFromPrefs();

  BootOptions boot;
  boot.initScreen = true;
  boot.createCanvas = true;
  boot.initCameraBackground = (g_role == BootRole::Preview);
  boot.initSd = false;
  boot.initAi = false;

  board.begin(boot);
  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  USBSerial.printf("boot role=%u captureMode=%s photoIndex=%d\n",
                   (unsigned)g_role,
                   kCaptureModes[g_captureResIndex].name,
                   (int)g_photoIndex);

  if (g_role == BootRole::CaptureOnce) {
    setupCaptureMode();
  } else {
    setupPreviewMode();
  }
}

void loop() {
  delay(30);
}
