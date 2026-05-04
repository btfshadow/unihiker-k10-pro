#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;

static String gImageDir = "S:/images";
static String gAudioDir = "S:/audio";
static String gDataDir = "S:/data";

static const int kBmpW = 48;
static const int kBmpH = 48;
static uint16_t gBmpPixels[kBmpW * kBmpH];
static const int kWavFrames = 800;
static int16_t gWavPcm[kWavFrames * 2];
static const uint8_t kBinSampleA[] = {0x10, 0x20, 0x30, 0x40};
static const uint8_t kBinSampleB[] = {0x50, 0x60, 0x70, 0x80};

static void buildSampleBmpPattern() {
  for (int y = 0; y < kBmpH; y++) {
    for (int x = 0; x < kBmpW; x++) {
      uint8_t r5 = (uint8_t)((x * 31) / (kBmpW - 1));
      uint8_t g6 = (uint8_t)((y * 63) / (kBmpH - 1));
      uint8_t b5 = (uint8_t)((((x + y) % kBmpW) * 31) / (kBmpW - 1));
      gBmpPixels[y * kBmpW + x] = (uint16_t)((r5 << 11) | (g6 << 5) | b5);
    }
  }
}

static void buildSampleWavPattern() {
  for (int i = 0; i < kWavFrames; i++) {
    int16_t sample = (int16_t)(((i % 64) - 32) * 600);
    gWavPcm[i * 2] = sample;
    gWavPcm[i * 2 + 1] = sample;
  }
}

static void drawStatus(const String &title, const String &line1, const String &line2,
                       uint32_t color = 0x000000) {
  auto &d = board.display();
  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.textRow("storage smoke", 1, 0x000000);
  d.setFontSize(Canvas::eCNAndENFont16);
  d.textAt(title, 10, 56, color, 28, true);
  d.textAt(line1, 10, 82, color, 30, true);
  d.textAt(line2, 10, 108, color, 30, true);
  d.textAt(gDataDir, 10, 134, 0x666666, 28, true);
  d.textAt("A: custom dirs", 10, 260, 0x444444, 22, true);
  d.textAt("B: reset default", 10, 285, 0x444444, 22, true);
  d.update();
}

static void applyDirectories(const String &imageDir,
                             const String &audioDir,
                             const String &dataDir) {
  gImageDir = imageDir;
  gAudioDir = audioDir;
  gDataDir = dataDir;

  (void)board.storage().setImageDirectory(gImageDir);
  (void)board.storage().setAudioDirectory(gAudioDir);
  (void)board.storage().setDataDirectory(gDataDir);

  Status st = board.storage().initSd();
  if (!st.ok()) {
    USBSerial.printf("initSd error: %s\n", st.message);
    drawStatus("SD init failed", st.message, "check card", 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  StorageHealth health;
  st = board.storage().healthCheck(health, true);
  USBSerial.printf("health: code=%d ready=%d card=%d rw=%d total=%llu used=%llu\n",
                   (int)st.code,
                   health.sdReady ? 1 : 0,
                   health.cardPresent ? 1 : 0,
                   health.readWriteOk ? 1 : 0,
                   (unsigned long long)health.totalBytes,
                   (unsigned long long)health.usedBytes);
  if (!st.ok()) {
    drawStatus("health failed", st.message, "check SD card", 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().ensureDirectories();
  String imgPath = board.storage().imagePath("sample.bmp");
  String wavPath = board.storage().audioPath("sample.wav");
  String jsonPath = board.storage().dataPath("sample.json");
  String csvPath = board.storage().dataPath("sample.csv");
  String txtPath = board.storage().dataPath("sample.txt");
  String binPath = board.storage().dataPath("sample.bin");
  String deletePath = board.storage().dataPath("delete_me.tmp");

  USBSerial.printf("dirs image=%s audio=%s data=%s\n",
                   gImageDir.c_str(), gAudioDir.c_str(), gDataDir.c_str());
  USBSerial.printf("paths bmp=%s wav=%s json=%s csv=%s txt=%s bin=%s\n",
                   imgPath.c_str(), wavPath.c_str(), jsonPath.c_str(), csvPath.c_str(),
                   txtPath.c_str(), binPath.c_str());
  USBSerial.printf("ensureDirectories: %s\n", st.ok() ? "ok" : st.message);

  if (!st.ok()) {
    drawStatus("dir create failed", st.message, "", 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().writeRgb565Bmp("sample.bmp", kBmpW, kBmpH, gBmpPixels);
  USBSerial.printf("writeRgb565Bmp: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("bmp write failed", st.message, imgPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().beginWavRecord("sample.wav", 16000, 2, 16);
  if (st.ok()) {
    const uint8_t *pcmBytes = reinterpret_cast<const uint8_t *>(gWavPcm);
    const size_t pcmLen = sizeof(gWavPcm);
    st = board.storage().appendWavRecord(pcmBytes, pcmLen / 2);
    if (st.ok()) {
      st = board.storage().appendWavRecord(pcmBytes + (pcmLen / 2), pcmLen - (pcmLen / 2));
    }
    if (st.ok()) {
      st = board.storage().endWavRecord(true);
    } else {
      (void)board.storage().endWavRecord(false);
    }
  }
  USBSerial.printf("writeWav: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("wav write failed", st.message, wavPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().writeJson("sample.json",
                                 "{\"device\":\"unihiker-pro\",\"ok\":true}\n");
  USBSerial.printf("writeJson: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("json write failed", st.message, jsonPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().writeCsv("sample.csv", "key,value\nframes,800\nrate,16000\n");
  USBSerial.printf("writeCsv: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("csv write failed", st.message, csvPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().appendTextFile("sample.csv", "channels,2\n");
  USBSerial.printf("appendCsv: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("csv append failed", st.message, csvPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().writeTextFile("sample.txt", "line1\n");
  if (st.ok()) {
    st = board.storage().appendTextFile("sample.txt", "line2\n");
  }
  USBSerial.printf("writeTxt: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("txt write failed", st.message, txtPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().writeBinaryFile("sample.bin", kBinSampleA, sizeof(kBinSampleA));
  if (st.ok()) {
    st = board.storage().appendBinaryFile("sample.bin", kBinSampleB, sizeof(kBinSampleB));
  }
  USBSerial.printf("writeBin: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("bin write failed", st.message, binPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  String jsonRead;
  st = board.storage().readTextFile("sample.json", &jsonRead, 2048);
  if (st.ok() && jsonRead.indexOf("\"ok\":true") < 0) {
    st = Status::Error(StatusCode::IOError, "json content mismatch");
  }
  USBSerial.printf("readJson: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("json read failed", st.message, jsonPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  String csvRead;
  st = board.storage().readTextFile("sample.csv", &csvRead, 4096);
  if (st.ok()) {
    bool hasHeader = csvRead.indexOf("key,value") >= 0;
    bool hasBase = csvRead.indexOf("frames,800") >= 0;
    bool hasAppend = csvRead.indexOf("channels,2") >= 0;
    if (!hasHeader || !hasBase || !hasAppend) {
      st = Status::Error(StatusCode::IOError, "csv content mismatch");
    }
  }
  USBSerial.printf("readCsv: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("csv read failed", st.message, csvPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  uint8_t binRead[16] = {0};
  size_t binReadN = 0;
  st = board.storage().readBinaryFile("sample.bin", binRead, sizeof(binRead), &binReadN);
  if (st.ok()) {
    bool okBytes = (binReadN == sizeof(kBinSampleA) + sizeof(kBinSampleB));
    for (size_t i = 0; okBytes && i < sizeof(kBinSampleA); i++) {
      if (binRead[i] != kBinSampleA[i]) okBytes = false;
    }
    for (size_t i = 0; okBytes && i < sizeof(kBinSampleB); i++) {
      if (binRead[sizeof(kBinSampleA) + i] != kBinSampleB[i]) okBytes = false;
    }
    if (!okBytes) {
      st = Status::Error(StatusCode::IOError, "binary content mismatch");
    }
  }
  USBSerial.printf("readBin: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("bin read failed", st.message, binPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  bool exists = false;
  uint32_t size = 0;
  st = board.storage().fileExists("sample.bmp", &exists, gImageDir);
  if (st.ok() && !exists) st = Status::Error(StatusCode::IOError, "bmp missing");
  if (st.ok()) st = board.storage().fileSize("sample.bmp", &size, gImageDir);
  if (st.ok() && size < 128) st = Status::Error(StatusCode::IOError, "bmp too small");
  USBSerial.printf("checkBmp: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("bmp check failed", st.message, imgPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().fileExists("sample.wav", &exists, gAudioDir);
  if (st.ok() && !exists) st = Status::Error(StatusCode::IOError, "wav missing");
  if (st.ok()) st = board.storage().fileSize("sample.wav", &size, gAudioDir);
  if (st.ok() && size <= 44) st = Status::Error(StatusCode::IOError, "wav too small");
  USBSerial.printf("checkWav: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("wav check failed", st.message, wavPath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().writeTextFile("delete_me.tmp", "delete");
  if (st.ok()) st = board.storage().removeFile("delete_me.tmp");
  if (st.ok()) st = board.storage().fileExists("delete_me.tmp", &exists);
  if (st.ok() && exists) st = Status::Error(StatusCode::IOError, "remove failed");
  USBSerial.printf("removeFile: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) {
    drawStatus("remove failed", st.message, deletePath, 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  drawStatus("storage all-in-one ok", imgPath, jsonPath, 0x006600);
  board.led().setRgb(0, {0, 170, 0});
}

static void onButtonA() {
  USBSerial.println("A -> set custom dirs");
  applyDirectories("S:/media/images", "S:/media/audio", "S:/media/data");
}

static void onButtonB() {
  USBSerial.println("B -> reset default dirs");
  applyDirectories("S:/images", "S:/audio", "S:/data");
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
  boot.initSd = false;
  boot.initAi = false;

  board.begin(boot);
  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);

  USBSerial.println("storage_smoke boot");
  buildSampleBmpPattern();
  buildSampleWavPattern();
  applyDirectories("S:/images", "S:/audio", "S:/data");
}

void loop() {
  delay(50);
}
