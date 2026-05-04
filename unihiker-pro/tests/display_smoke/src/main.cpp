// display_smoke — minimal graphics suite for Plan 03 phase 2 closure.
#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;

struct SuiteResult {
  uint8_t passed = 0;
  uint8_t failed = 0;
};

static uint8_t gCaseIndex = 0;
static SuiteResult gLastRun;

static const uint8_t kBmp8x8[] = {
  0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x7E, 0x3C, 0x18,
};

static const char *statusCodeName(StatusCode code) {
  switch (code) {
    case StatusCode::Ok:
      return "Ok";
    case StatusCode::NotInitialized:
      return "NotInitialized";
    case StatusCode::InvalidArgument:
      return "InvalidArgument";
    case StatusCode::NotSupported:
      return "NotSupported";
    case StatusCode::IOError:
      return "IOError";
    case StatusCode::Busy:
      return "Busy";
    default:
      return "Unknown";
  }
}

static bool expectOk(const Status &st, const char *step) {
  if (st.ok()) return true;
  USBSerial.printf("FAIL: %s -> %s (%s)\n", step, statusCodeName(st.code),
                   st.message);
  return false;
}

static bool expectCode(const Status &st, StatusCode code, const char *step) {
  if (st.code == code) return true;
  USBSerial.printf("FAIL: %s -> expected %s, got %s (%s)\n", step,
                   statusCodeName(code), statusCodeName(st.code), st.message);
  return false;
}

static bool caseTextAndFonts() {
  auto &d = board.display();
  bool ok = true;
  ok &= expectOk(d.setBackground(0xFFFFFF), "setBackground");
  ok &= expectOk(d.clearCanvas(), "clearCanvas");
  ok &= expectOk(d.textRow("case 1: text", 1, 0x000000), "textRow");
  ok &= expectOk(d.setFontSize(Canvas::eCNAndENFont24), "font24");
  ok &= expectOk(d.textAt("Hello", 12, 60, 0x0033CC, 10, true), "textAt1");
  ok &= expectOk(d.setFontSize(Canvas::eCNAndENFont16), "font16");
  ok &= expectOk(d.textAt("font16", 12, 95, 0xCC3300, 10, true), "textAt2");
  ok &= expectOk(d.update(), "update");
  return ok;
}

static bool caseLinesAndShapes() {
  auto &d = board.display();
  bool ok = true;
  ok &= expectOk(d.setBackground(0xF2F2F2), "setBackground");
  ok &= expectOk(d.clearCanvas(), "clearCanvas");
  ok &= expectOk(d.textRow("case 2: shapes", 1, 0x000000), "textRow");
  ok &= expectOk(d.setLineWidth(3), "lineWidth3");
  ok &= expectOk(d.drawLine(10, 48, 230, 48, 0xFF0000), "line1");
  ok &= expectOk(d.drawLine(10, 78, 230, 78, 0x00AA00), "line2");
  ok &= expectOk(d.setLineWidth(1), "lineWidth1");
  ok &= expectOk(d.drawRect(20, 100, 80, 60, 0x0000FF, 0xFFFFFF, false),
                 "rect");
  ok &= expectOk(d.drawCircle(170, 130, 30, 0xAA6600, 0xFFEECC, true),
                 "circle");
  ok &= expectOk(d.update(), "update");
  return ok;
}

static bool casePointsAndClearOps() {
  auto &d = board.display();
  bool ok = true;
  ok &= expectOk(d.setBackground(0xFFFFFF), "setBackground");
  ok &= expectOk(d.clearCanvas(), "clearCanvas");
  ok &= expectOk(d.textRow("case 3: clear", 1, 0x000000), "textRow");
  for (int x = 8; x < 232; x += 4) {
    ok &= expectOk(d.drawPoint(x, 80 + (x % 30), 0xFF0000), "pointA");
    ok &= expectOk(d.drawPoint(x, 140 - (x % 30), 0x0000FF), "pointB");
  }
  ok &= expectOk(d.drawBitmap(100, 210, 8, 8, kBmp8x8), "bitmap");
  ok &= expectOk(d.update(), "update1");
  delay(300);
  ok &= expectOk(d.clearRegion(24, 70, 192, 90), "clearRegion");
  ok &= expectOk(d.clearRow(1), "clearRow");
  ok &= expectOk(d.textRow("case 3: region+row", 1, 0x333333), "textRow2");
  ok &= expectOk(d.update(), "update2");
  return ok;
}

static bool caseSessionLifecycle() {
  auto &d = board.display();
  bool ok = true;
  ok &= expectOk(d.destroyCanvas(), "destroyCanvas");
  ok &= expectCode(d.clearCanvas(), StatusCode::NotInitialized,
                   "clearCanvas after destroy");
  ok &= expectOk(d.createCanvas(), "createCanvas");
  ok &= expectOk(d.setBackground(0xFFFFFF), "setBackground");
  ok &= expectOk(d.clearCanvas(), "clearCanvas");
  ok &= expectOk(d.textRow("case 4: session", 1, 0x006600), "textRow");
  ok &= expectOk(d.textAt("destroy/create ok", 10, 88, 0x000000, 20, true),
                 "textAt");
  ok &= expectOk(d.update(), "update");
  return ok;
}

using CaseFn = bool (*)();

struct CaseItem {
  const char *name;
  CaseFn fn;
};

static const CaseItem kCases[] = {
  {"Text and fonts", caseTextAndFonts},
  {"Lines and shapes", caseLinesAndShapes},
  {"Points and clear", casePointsAndClearOps},
  {"Session lifecycle", caseSessionLifecycle},
};

static const uint8_t kCaseCount = sizeof(kCases) / sizeof(kCases[0]);

static bool runCase(uint8_t index) {
  if (index >= kCaseCount) return false;
  USBSerial.printf("[CASE %u/%u] %s\n", (unsigned)(index + 1),
                   (unsigned)kCaseCount, kCases[index].name);
  bool ok = kCases[index].fn();
  USBSerial.printf("[CASE %u] %s\n", (unsigned)(index + 1),
                   ok ? "PASS" : "FAIL");
  return ok;
}

static void showSuiteSummary(const SuiteResult &result) {
  auto &d = board.display();
  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.textRow("display suite", 1, 0x000000);
  d.textAt(String("PASS: ") + String(result.passed), 10, 70, 0x008800, 20, true);
  d.textAt(String("FAIL: ") + String(result.failed), 10, 105, 0xCC0000, 20, true);
  d.textAt("A: next case", 10, 180, 0x333333, 20, true);
  d.textAt("B: rerun suite", 10, 210, 0x333333, 20, true);
  d.update();
}

static SuiteResult runSuiteOnce() {
  SuiteResult result;
  for (uint8_t i = 0; i < kCaseCount; ++i) {
    bool ok = runCase(i);
    if (ok) {
      result.passed++;
    } else {
      result.failed++;
    }
    delay(450);
  }
  return result;
}

static void onButtonA() {
  USBSerial.println("btn A -> run next case");
  bool ok = runCase(gCaseIndex);
  gCaseIndex = (uint8_t)((gCaseIndex + 1) % kCaseCount);
  if (ok) {
    gLastRun.passed++;
  } else {
    gLastRun.failed++;
  }
  showSuiteSummary(gLastRun);
}

static void onButtonB() {
  USBSerial.println("btn B -> rerun full suite");
  gLastRun = runSuiteOnce();
  gCaseIndex = 0;
  showSuiteSummary(gLastRun);
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
  USBSerial.println("display_smoke suite boot");

  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);

  gLastRun = runSuiteOnce();
  showSuiteSummary(gLastRun);
}

void loop() { delay(50); }
