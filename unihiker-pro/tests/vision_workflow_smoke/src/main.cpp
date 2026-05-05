#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;

static const VisionWorkflowMode kWorkflowModes[] = {
  VisionWorkflowMode::LiveAim,
  VisionWorkflowMode::CaptureReview,
  VisionWorkflowMode::InputReader,
};
static const size_t kWorkflowModeCount = sizeof(kWorkflowModes) / sizeof(kWorkflowModes[0]);

static const AiMode kAiModes[] = {
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
static const size_t kAiModeCount = sizeof(kAiModes) / sizeof(kAiModes[0]);
static const uint32_t kButtonLongPressMs = 900;
static const bool kLiveFeedbackForTest = true;
static const uint32_t kLiveFeedbackUpdateMs = 240;

static size_t gWorkflowIndex = 0;
static size_t gAiModeIndex = 0;
static bool gLiveRunning = false;
static String gLastShownSummary;

static const char *workflowName(VisionWorkflowMode mode);
static const char *aiModeName(AiMode mode);

static String selectionLine1() {
  return String("wf=") + workflowName(kWorkflowModes[gWorkflowIndex]) +
         " [" + String((unsigned long)(gWorkflowIndex + 1)) +
         "/" + String((unsigned long)kWorkflowModeCount) + "]";
}

static String selectionLine2() {
  return String("ai=") + aiModeName(kAiModes[gAiModeIndex]) +
         " [" + String((unsigned long)(gAiModeIndex + 1)) +
         "/" + String((unsigned long)kAiModeCount) + "]";
}

static const char *workflowName(VisionWorkflowMode mode) {
  switch (mode) {
    case VisionWorkflowMode::LiveAim: return "live";
    case VisionWorkflowMode::CaptureReview: return "capture";
    case VisionWorkflowMode::InputReader: return "file";
    case VisionWorkflowMode::Ocr: return "ocr";
    default: return "unknown";
  }
}

static const char *aiModeName(AiMode mode) {
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
  d.textRow("vision workflow", 1, 0x000000);
  d.setFontSize(Canvas::eCNAndENFont16);
  d.textAt(title, 10, 56, color, 28, true);
  d.textAt(line1, 10, 82, color, 30, true);
  d.textAt(line2, 10, 108, color, 30, true);
  d.textAt("A<1s: next wf", 10, 246, 0x444444, 22, true);
  d.textAt("A>=1s: prev wf", 10, 270, 0x444444, 22, true);
  d.textAt("B<1s: run/stop | B>=1s: next IA", 10, 294, 0x444444, 20, true);
  d.update();
}

static void drawSelection(const String &title = "selection",
                          uint32_t color = 0x000000) {
  drawState(title, selectionLine1(), selectionLine2(), color);
}

static void showLastResult() {
  const VisionWorkflowResult &r = board.vision().lastWorkflowResult();
  USBSerial.printf("workflow result: ok=%d wf=%d mode=%d bytes=%lu src=%s summary=%s\n",
                   r.ok ? 1 : 0,
                   (int)r.workflow,
                   (int)r.mode,
                   (unsigned long)r.analyzedBytes,
                   r.source.c_str(),
                   r.summary.c_str());

  String line1 = String("wf=") + workflowName(r.workflow) +
                 " ai=" + aiModeName(r.mode) +
                 " ok=" + (r.ok ? "1" : "0");
  String line2 = r.summary;
  drawState("result", line1, line2, r.ok ? 0x006600 : 0xCC0000);
}

static void ensureInputSample() {
  (void)board.storage().setDataDirectory("S:/data");
  (void)board.storage().ensureDirectories();
  (void)board.storage().writeTextFile("workflow_input.txt",
                                      "hello from vision workflow reader\nmode=input\n");
}

static void runActiveWorkflow() {
  VisionWorkflowMode wf = kWorkflowModes[gWorkflowIndex];

  if (gLiveRunning && wf != VisionWorkflowMode::LiveAim) {
    drawState("live ativo",
              "B curto: parar live",
              "A bloqueado durante live",
              0x000000);
    return;
  }

  Status st = board.vision().setWorkflowMode(wf);
  if (!st.ok()) {
    drawState("set workflow failed", st.message, workflowName(wf), 0xCC0000);
    return;
  }

  switch (wf) {
    case VisionWorkflowMode::LiveAim:
      if (!gLiveRunning) {
        (void)board.vision().setMode(kAiModes[gAiModeIndex]);
        (void)board.vision().setLiveFeedbackPeriodMs(kLiveFeedbackUpdateMs);
        (void)board.vision().setLiveFeedbackEnabled(kLiveFeedbackForTest);
        st = board.vision().startLiveAim(kLiveFeedbackForTest);
        gLiveRunning = st.ok();
      } else {
        st = board.vision().stopLiveAim();
        if (st.ok()) gLiveRunning = false;
      }
      break;

    case VisionWorkflowMode::CaptureReview:
      st = board.vision().captureAndReview("workflow_capture.bmp", FRAMESIZE_SXGA);
      break;

    case VisionWorkflowMode::InputReader:
      ensureInputSample();
      st = board.vision().analyzeInputFile("workflow_input.txt");
      break;

    case VisionWorkflowMode::Ocr: {
      (void)board.vision().setMode(AiMode::Ocr);
      String out;
      st = board.vision().runOcrOnInput("S:/data/workflow_input.txt", &out);
      break;
    }

    default:
      st = Status::Error(StatusCode::InvalidArgument, "invalid workflow mode");
      break;
  }

  USBSerial.printf("run workflow %s -> code=%d msg=%s\n",
                   workflowName(wf), (int)st.code, st.message);

  if (wf == VisionWorkflowMode::LiveAim && gLiveRunning && st.ok()) {
    const VisionWorkflowResult &r = board.vision().lastWorkflowResult();
    USBSerial.printf("live feedback: %s\n", r.summary.c_str());
    return;
  }

  showLastResult();
}

static void onButtonAShort() {
  if (gLiveRunning) {
    drawState("live ativo",
              "B curto: parar live",
              "B longo: trocar IA",
              0x000000);
    return;
  }

  gWorkflowIndex = (gWorkflowIndex + 1) % kWorkflowModeCount;
  drawSelection("workflow +1", 0x000000);
}

static void onButtonALong() {
  if (gLiveRunning) {
    drawState("live ativo",
              "B curto: parar live",
              "B longo: trocar IA",
              0x000000);
    return;
  }

  gWorkflowIndex = (gWorkflowIndex + kWorkflowModeCount - 1) % kWorkflowModeCount;
  drawSelection("workflow -1", 0x000000);
}

static void onButtonBShort() {
  runActiveWorkflow();
}

static void onButtonBLong() {
  gAiModeIndex = (gAiModeIndex + 1) % kAiModeCount;
  Status st = board.vision().setMode(kAiModes[gAiModeIndex]);
  USBSerial.printf("set ai mode (B long) %s -> code=%d msg=%s\n",
                   aiModeName(kAiModes[gAiModeIndex]), (int)st.code, st.message);

  if (gLiveRunning && st.ok()) {
    // Live feedback task updates overlay continuously; no manual refresh here.
    return;
  }

  drawSelection("ai mode +1", st.ok() ? 0x006600 : 0xCC0000);
}

static void printModeCatalog() {
  USBSerial.println("AI modes catalog:");
  for (size_t i = 0; i < kAiModeCount; i++) {
    USBSerial.printf("  [%u/%u] %s\n",
                     (unsigned int)(i + 1),
                     (unsigned int)kAiModeCount,
                     aiModeName(kAiModes[i]));
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
  boot.initSd = true;
  boot.initAi = false;

  Status st = board.begin(boot);
  if (!st.ok()) {
    drawState("board init failed", st.message, "", 0xCC0000);
    return;
  }

  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  st = board.vision().init();
  USBSerial.printf("vision init: code=%d msg=%s\n", (int)st.code, st.message);
  if (!st.ok()) {
    drawState("vision init failed", st.message, "", 0xCC0000);
    return;
  }

  (void)board.vision().setMode(kAiModes[gAiModeIndex]);
  (void)board.vision().setWorkflowMode(kWorkflowModes[gWorkflowIndex]);

  printModeCatalog();

  board.input().onReleaseByDuration(ButtonId::A,
                                    onButtonAShort,
                                    onButtonALong,
                                    kButtonLongPressMs);
  board.input().onReleaseByDuration(ButtonId::B,
                                    onButtonBShort,
                                    onButtonBLong,
                                    kButtonLongPressMs);

  drawSelection("ready", 0x000000);
}

void loop() {
  static uint32_t lastMs = 0;
  if (millis() - lastMs >= 1500) {
    lastMs = millis();

    const VisionWorkflowResult &r = board.vision().lastWorkflowResult();
    if (r.summary.length() > 0) {
      if (gLiveRunning) {
        USBSerial.printf("live summary: %s\n", r.summary.c_str());
        gLastShownSummary = r.summary;
      } else {
        if (r.summary != gLastShownSummary) {
          showLastResult();
          gLastShownSummary = r.summary;
        }
      }
    }
  }
  delay(40);
}
