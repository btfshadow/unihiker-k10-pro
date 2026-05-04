#include <Arduino.h>
#include <SD.h>
#include <esp_camera.h>
#include <string.h>
#include <img_converters.h>
#include <Preferences.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;

static bool g_busy = false;
static bool g_cameraInited = false;
static bool g_sdReady = false;
static int g_photoIndex = 0;
static uint8_t g_resIndex = 0;

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

static uint8_t g_decodeModeIndex = 0;

struct CaptureMode {
  framesize_t size;
  const char *name;
};

struct BmpHdr {
  uint16_t bfType;
  uint32_t fileSize;
  uint16_t reserved1;
  uint16_t reserved2;
  uint32_t offset;
  uint32_t headerSize;
  int32_t width;
  int32_t height;
  uint16_t planes;
  uint16_t bitsPerPixel;
  uint32_t compression;
  uint32_t dataSize;
  int32_t hResolution;
  int32_t vResolution;
  uint32_t colors;
  uint32_t importantColors;
  uint32_t maskR;
  uint32_t maskG;
  uint32_t maskB;
} __attribute__((packed));

static const CaptureMode kModes[] = {
  {FRAMESIZE_QVGA, "QVGA 320x240"},
  {FRAMESIZE_HVGA, "HVGA 480x320"},
  {FRAMESIZE_VGA, "VGA 640x480"},
  {FRAMESIZE_SVGA, "SVGA 800x600"},
  {FRAMESIZE_XGA, "XGA 1024x768"},
  {FRAMESIZE_SXGA, "SXGA 1280x1024"},
  {FRAMESIZE_UXGA, "UXGA 1600x1200"},  // 2MP
};

static const DecodeMode kDecodeModes[] = {
  {"RGB565", ColorMode::Rgb565},
};

static const camera_config_t kCamCfgBase = {
    .pin_pwdn = -1,
    .pin_reset = -1,
    .pin_xclk = 7,
    .pin_sscb_sda = 47,
    .pin_sscb_scl = 48,
    .pin_d7 = 6,
    .pin_d6 = 15,
    .pin_d5 = 16,
    .pin_d4 = 18,
    .pin_d3 = 9,
    .pin_d2 = 11,
    .pin_d1 = 10,
    .pin_d0 = 8,
    .pin_vsync = 4,
    .pin_href = 5,
    .pin_pclk = 17,
    .xclk_freq_hz = 16000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
};



static void drawProgress(uint8_t pct, const String &label) {
  const int16_t x = 10;
  const int16_t y = 200;
  const int16_t w = 220;
  const int16_t h = 16;

  board.display().clearRegion(0, 0, 240, 120);
  board.display().textAt(label, 10, 10, 0xFFFFFF, 26, true);

  board.display().drawRect(x - 1, y - 1, w + 2, h + 2, 0xFFFFFF, 0x333333, false);
  board.display().drawRect(x, y, w, h, 0x333333, 0x333333, true);

  if (pct > 0) {
    int16_t fillW = (int16_t)((uint32_t)pct * w / 100U);
    board.display().drawRect(x, y, fillW, h, 0x00CC66, 0x00CC66, true);
  }

  board.display().setFontSize(Canvas::eCNAndENFont16);
  board.display().textAt(String(pct) + "%", x + 95, y - 22, 0xFFFFFF, 8, true);
  board.display().setFontSize(Canvas::eCNAndENFont24);
  board.display().update();
}

static bool ensureSdReady() {
  if (g_sdReady) return true;

  for (int i = 0; i < 5; i++) {
    if (SD.begin()) {
      g_sdReady = true;
      return true;
    }
    delay(120);
  }
  return false;
}

static String sanitizeForFilename(const String &input) {
  String out;
  out.reserve(input.length());

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) {
      out += c;
    } else {
      out += '_';
    }
  }

  while (out.indexOf("__") >= 0) {
    out.replace("__", "_");
  }
  out.trim();
  if (out.length() == 0) {
    out = "cfg";
  }
  return out;
}

static String buildCapturePath(const CaptureMode &mode, const DecodeMode &decode, int32_t w, int32_t h, int photoIndex) {
  String modeTag = sanitizeForFilename(String(mode.name));
  String decodeTag = sanitizeForFilename(String(decode.name));
  String sizeTag = String(w) + "x" + String(h);
  return String("/") + modeTag + "_" + decodeTag + "_" + sizeTag + "_" + String(photoIndex) + ".bmp";
}

static bool cameraInit(uint8_t resIdx) {
  if (!g_cameraInited) {
    // Init with the exact resolution we need. DMA descriptors are sized per
    // frame_size at init time — set_framesize() alone corrupts the buffer
    // layout (wrong DMA node stride). Full reinit is required per resolution.
    camera_config_t cfg = kCamCfgBase;
    cfg.frame_size = kModes[resIdx].size;

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
      USBSerial.printf("camera init failed: 0x%x\n", err);
      return false;
    }
    g_cameraInited = true;
  }

  USBSerial.printf("camera ready: %s\n", kModes[resIdx].name);
  return true;
}

static bool saveJpeg(const char *path, camera_fb_t *fb) {
  if (SD.exists(path)) SD.remove(path);

  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;

  const uint8_t *ptr = fb->buf;
  size_t total = fb->len;
  size_t written = 0;
  const size_t chunk = 4096;

  while (written < total) {
    size_t n = total - written;
    if (n > chunk) n = chunk;
    if (file.write(ptr + written, n) != n) {
      file.close();
      return false;
    }
    written += n;
    uint8_t pct = 35 + (uint8_t)((written * 60U) / total);
    drawProgress(pct, "Saving JPG...");
  }

  file.flush();
  file.close();
  return true;
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
  // Process COLOR TRANSFORM IN-PLACE on the camera buffer.
  // We hold fb until after fmt2bmp completes to avoid an extra malloc.
  // Peak PSRAM = camera_frame (held) + bmpBuf output only.
  int32_t w = (int32_t)fb->width;
  int32_t h = (int32_t)fb->height;
  size_t srcLen = (size_t)fb->len;  // pass full allocated buffer size to fmt2bmp

  const DecodeMode &decode = kDecodeModes[g_decodeModeIndex];
  if (decode.colorMode != ColorMode::Rgb565) {
    uint16_t *pixels = (uint16_t *)fb->buf;
    size_t pxCount = (size_t)w * (size_t)h;
    for (size_t i = 0; i < pxCount; i++) {
      pixels[i] = applyColorMode(pixels[i], decode.colorMode);
    }
  }

  drawProgress(40, "Converting BMP...");
  uint8_t *bmpBuf = nullptr;
  size_t bmpLen = 0;
  bool convOk = fmt2bmp(fb->buf, srcLen, (uint16_t)w, (uint16_t)h, PIXFORMAT_RGB565, &bmpBuf, &bmpLen);

  // Return camera buffer as soon as conversion is done
  esp_camera_fb_return(fb);

  if (!convOk || bmpBuf == nullptr || bmpLen == 0) {
    USBSerial.printf("saveBmp: fmt2bmp failed convOk=%d bmpBuf=%p bmpLen=%u\n",
                     (int)convOk, bmpBuf, (unsigned)bmpLen);
    if (bmpBuf) free(bmpBuf);
    return false;
  }

  USBSerial.printf("saveBmp: bmpLen=%u writing to %s\n", (unsigned)bmpLen, path);

  if (SD.exists(path)) SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    free(bmpBuf);
    return false;
  }

  // NOTE: No drawProgress() calls here while bmpBuf is alive.
  // For large frames (UXGA=3.9MB), the canvas clearRegion allocation fails
  // when PSRAM is occupied by bmpBuf → avoid ALL display ops during write.
  size_t written = 0;
  const size_t chunk = 4096;
  while (written < bmpLen) {
    size_t n = bmpLen - written;
    if (n > chunk) n = chunk;
    if (file.write(bmpBuf + written, n) != n) {
      file.close();
      free(bmpBuf);
      return false;
    }
    written += n;
    if ((written % (chunk * 64)) == 0) {
      USBSerial.printf("  writing %u/%u KB\n",
                       (unsigned)(written / 1024), (unsigned)(bmpLen / 1024));
    }
  }

  file.flush();
  file.close();
  free(bmpBuf);
  return true;
}

static camera_fb_t *captureFrameWithRetry(const char *modeName, uint8_t retries) {
  if (!g_cameraInited) return nullptr;

  // Discard warmup frames so exposure/AWB settle
  for (uint8_t w = 0; w < 3; w++) {
    camera_fb_t *warm = esp_camera_fb_get();
    if (warm) esp_camera_fb_return(warm);
    delay(30);
  }

  for (uint8_t a = 0; a < retries; a++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb != nullptr) {
      USBSerial.printf("capture ok attempt=%u mode=%s frame=%ux%u len=%u fmt=%d\n",
                       (unsigned)(a + 1), modeName,
                       (unsigned)fb->width, (unsigned)fb->height,
                       (unsigned)fb->len, (int)fb->format);
      return fb;
    }
    USBSerial.printf("capture failed attempt=%u mode=%s\n", (unsigned)(a + 1), modeName);
    delay(60);
  }
  return nullptr;
}

static bool captureToSd(const CaptureMode &mode, String &outPath, String &errMsg) {
  if (!ensureSdReady()) {
    errMsg = "SD not ready";
    return false;
  }
  if (!g_cameraInited) {
    errMsg = "camera not ready";
    return false;
  }

  g_busy = true;
  drawProgress(0, "Capturing...");
  drawProgress(30, "Waiting frame...");
  camera_fb_t *fb = captureFrameWithRetry(mode.name, 3);
  if (!fb) {
    g_busy = false;
    errMsg = "queue capture timeout";
    return false;
  }

  USBSerial.printf("captured frame: %ux%u len=%u fmt=%d mode=%s\n",
                   (unsigned)fb->width,
                   (unsigned)fb->height,
                   (unsigned)fb->len,
                   (int)fb->format,
                   mode.name);

  bool ok = false;
  g_photoIndex++;

  if (fb->format != PIXFORMAT_RGB565) {
    errMsg = "queue returned non-RGB565";
    esp_camera_fb_return(fb);
    fb = nullptr;
    ok = false;
  } else {
    int32_t w = (int32_t)fb->width;
    int32_t h = (int32_t)fb->height;
    size_t len = (size_t)fb->len;
    size_t expectedLen = (size_t)w * (size_t)h * 2U;

    if (w <= 0 || h <= 0 || len < expectedLen) {
      errMsg = "invalid frame geometry/len";
      USBSerial.printf("frame mismatch: w=%d h=%d len=%u expected=%u\n",
                       (int)w, (int)h, (unsigned)len, (unsigned)expectedLen);
      esp_camera_fb_return(fb);
      fb = nullptr;
      ok = false;
    } else {
      outPath = buildCapturePath(mode, kDecodeModes[g_decodeModeIndex], w, h, g_photoIndex);
      // saveBmp takes ownership of fb and returns it to the camera driver after fmt2bmp
      ok = saveBmp(outPath.c_str(), fb);
      fb = nullptr;  // fb was returned inside saveBmp
      if (!ok) errMsg = "save BMP failed";
    }
  }

  if (fb != nullptr) {
    esp_camera_fb_return(fb);
    fb = nullptr;
  }
  drawProgress(100, ok ? "Done" : "Failed");
  delay(200);

  g_busy = false;
  return ok;
}

static void drawIdleUi() {
  board.display().setBackground(0x1A1A1A);
  board.display().clearCanvas();
  board.display().textRow("camera fullres capture", 1, 0xFFFFFF);
  board.display().textAt(String("Res: ") + kModes[g_resIndex].name, 10, 62, 0xFFFFFF, 29, true);
  board.display().textAt(String("Decode: ") + kDecodeModes[g_decodeModeIndex].name,
                         10, 92, 0xFFFFFF, 28, true);
  board.display().textAt("A: cycle res+reboot", 10, 132, 0x66CCFF, 24, true);
  board.display().textAt("B: capture", 10, 180, 0x66CCFF, 20, true);
  board.display().update();
}

void onButtonA() {
  if (g_busy) return;

  uint8_t nextIdx = (g_resIndex + 1) % (sizeof(kModes) / sizeof(kModes[0]));
  USBSerial.printf("A: selecting %s, rebooting...\n", kModes[nextIdx].name);

  board.display().clearCanvas();
  board.display().textAt("Switching to:", 10, 40, 0xFFFFFF, 24, true);
  board.display().textAt(kModes[nextIdx].name, 10, 80, 0x00FF66, 28, true);
  board.display().textAt("Rebooting...", 10, 130, 0xFFD080, 26, true);
  board.display().update();
  delay(600);

  // Persist new index so boot reads it after restart
  Preferences prefs;
  prefs.begin("cam", false);
  prefs.putUChar("resIdx", nextIdx);
  prefs.end();

  ESP.restart();
}

void onButtonB() {
  if (g_busy) return;

  String path;
  String err;
  bool ok = captureToSd(kModes[g_resIndex], path, err);

  if (ok) {
    USBSerial.printf("saved: S:%s\n", path.c_str());
    board.display().clearCanvas();
    board.display().textAt("Capture OK", 10, 20, 0x00FF66, 18, true);
    board.display().textAt(String("S:") + path, 10, 50, 0xFFFFFF, 28, true);
    board.display().textAt(String("Res: ") + kModes[g_resIndex].name, 10, 82, 0xCCCCCC, 29, true);
    board.display().textAt(String("Decode: ") + kDecodeModes[g_decodeModeIndex].name,
                           10, 110, 0xCCCCCC, 28, true);
    if (err.length() > 0) {
      board.display().textAt(err, 10, 146, 0xFFD080, 26, true);
    }
    board.display().textAt("A fixed / B capture", 10, 176, 0x66CCFF, 24, true);
    board.display().update();
  } else {
    USBSerial.printf("capture failed: %s\n", err.c_str());
    board.display().clearCanvas();
    board.display().textAt("Capture FAIL", 10, 20, 0xFF4444, 18, true);
    board.display().textAt(err, 10, 50, 0xFFFFFF, 26, true);
    board.display().textAt(String("Decode: ") + kDecodeModes[g_decodeModeIndex].name,
                           10, 82, 0xAAAAAA, 28, true);
    board.display().update();
  }
}

void setup() {
  USBSerial.begin(115200);
  unsigned long t0 = millis();
  while (!USBSerial && millis() - t0 < 8000) {
    delay(10);
  }

  BootOptions boot;
  boot.initScreen = true;
  boot.createCanvas = true;
  boot.initCameraBackground = false;
  boot.initSd = false;
  boot.initAi = false;

  board.begin(boot);
  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  // Read persisted resolution index (default HVGA=1)
  Preferences prefs;
  prefs.begin("cam", true);
  g_resIndex = prefs.getUChar("resIdx", 1);
  prefs.end();
  if (g_resIndex >= sizeof(kModes) / sizeof(kModes[0])) g_resIndex = 1;

  cameraInit(g_resIndex);

  drawIdleUi();

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);

  USBSerial.println("camera_fullres_capture boot");
}

void loop() {
  delay(50);
}
