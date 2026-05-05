#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;

static const AiMode kModes[] = {
  AiMode::Face,
  AiMode::FaceRecognize,
  AiMode::FaceEnroll,
  AiMode::FaceDeleteAll,
  AiMode::Cat,
  AiMode::Move,
  AiMode::Code,
  AiMode::Ocr,
  AiMode::None,
};
static const size_t kModeCount = sizeof(kModes) / sizeof(kModes[0]);

static size_t gModeIndex = 0;
static uint32_t gSwitchOk = 0;
static uint32_t gSwitchFail = 0;

static const char *modeName(AiMode mode) {
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

static void drawState(const String &title,
                      const String &line1,
                      const String &line2,
                      uint32_t color = 0x000000) {
  auto &d = board.display();
  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.textRow("vision smoke", 1, 0x000000);
  d.setFontSize(Canvas::eCNAndENFont16);
  d.textAt(title, 10, 56, color, 28, true);
  d.textAt(line1, 10, 82, color, 30, true);
  d.textAt(line2, 10, 108, color, 30, true);
  d.textAt("A: next mode", 10, 246, 0x444444, 22, true);
  d.textAt("B: stress switch", 10, 270, 0x444444, 22, true);
  d.textAt("AB: clear faces", 10, 294, 0x444444, 22, true);
  d.update();
}

static void refreshDetectionUi() {
  bool det = board.vision().detected();
  String payload;
  String recInfo;

  if (board.vision().recognized()) {
    int id = board.vision().recognitionId();
    recInfo = String(" recID=") + String(id);
  }

  if (board.vision().mode() == AiMode::Code && det) {
    payload = board.vision().qrPayload();
    if (payload.length() == 0) payload = "(empty)";
  }

  drawState(String("mode: ") + modeName(board.vision().mode()),
            String("det: ") + (det ? "yes" : "no") +
              " sw=" + String((unsigned long)board.vision().modeSwitchCount()) + recInfo,
            (payload.length() > 0) ? payload :
              (String("ok=") + String((unsigned long)gSwitchOk) +
               " fail=" + String((unsigned long)gSwitchFail)),
            det ? 0x006600 : 0x000000);
}

static void switchTo(AiMode mode, bool countResult = true) {
  Status st = board.vision().setMode(mode);
  USBSerial.printf("switch -> %s: code=%d msg=%s\n",
                   modeName(mode), (int)st.code, st.message);

  if (!st.ok()) {
    if (countResult) gSwitchFail++;
    drawState("switch failed", st.message, modeName(mode), 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  if (countResult) gSwitchOk++;
  board.led().setRgb(0, {0, 160, 0});
  refreshDetectionUi();
}

static void onButtonA() {
  gModeIndex = (gModeIndex + 1) % kModeCount;
  switchTo(kModes[gModeIndex], true);
}

static void onButtonB() {
  USBSerial.println("stress: begin 20 mode switches");

  uint32_t failBefore = gSwitchFail;
  for (int i = 0; i < 20; i++) {
    AiMode m = kModes[i % (int)kModeCount];
    switchTo(m, true);
    delay(120);
  }

  if (gSwitchFail == failBefore) {
    drawState("stress ok", "20 switches", "no error", 0x006600);
    board.led().setRgb(0, {0, 170, 0});
  } else {
    drawState("stress failed", "see serial", "switch error", 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
  }
}

static void onButtonAB() {
  USBSerial.println("AB -> face clear mode");
  switchTo(AiMode::FaceDeleteAll, true);
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

  Status preInit = board.vision().setMode(AiMode::Face);
  USBSerial.printf("pre-init setMode expected fail: code=%d msg=%s\n",
                   (int)preInit.code, preInit.message);

  Status initSt = board.vision().init();
  USBSerial.printf("vision init: code=%d msg=%s\n", (int)initSt.code, initSt.message);
  if (initSt.ok()) {
    Status motionSt = board.vision().setMotionThreshold(120);
    USBSerial.printf("vision motion threshold: code=%d msg=%s\n",
                     (int)motionSt.code, motionSt.message);
  }

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);
  board.input().onPress(ButtonId::AB, onButtonAB);

  if (!initSt.ok()) {
    drawState("vision init failed", initSt.message, "", 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  gModeIndex = 0;
  switchTo(kModes[gModeIndex], true);
}

void loop() {
  static uint32_t lastRefreshMs = 0;
  if (millis() - lastRefreshMs >= 1500) {
    lastRefreshMs = millis();
    refreshDetectionUi();
  }
  delay(40);
}
