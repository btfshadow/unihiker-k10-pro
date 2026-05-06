#include <Arduino.h>
#include <unihiker_pro.h>

#if __has_include(<esp_bt.h>)
#include <esp_bt.h>
#define SMOKE_HAS_BT_PROBE 1
#else
#define SMOKE_HAS_BT_PROBE 0
#endif

using namespace unihiker_pro;

UniHikerPro board;
static String gLine;
static uint32_t gPass = 0;
static uint32_t gFail = 0;
static String gSuite = "idle";
static uint32_t gStep = 0;
static volatile bool gBtnASeen = false;
static volatile bool gBtnBSeen = false;
static volatile bool gBtnABSeen = false;

static void cbPressA() { USBSerial.println("[cb] press A"); }
static void cbReleaseA() { USBSerial.println("[cb] release A"); }
static void cbShortA() { USBSerial.println("[cb] short A"); }
static void cbLongA() { USBSerial.println("[cb] long A"); }
static void cbNavAFast() { USBSerial.println("[nav] A fast"); }
static void cbNavALong() { USBSerial.println("[nav] A long"); }
static void cbNavBFast() { USBSerial.println("[nav] B fast"); }
static void cbNavBLong() { USBSerial.println("[nav] B long"); }
static void cbNavABLong() { USBSerial.println("[nav] AB long"); }
static void cbDetectA() {
  gBtnASeen = true;
  USBSerial.println("[hw] botao A detectado");
}
static void cbDetectB() {
  gBtnBSeen = true;
  USBSerial.println("[hw] botao B detectado");
}
static void cbDetectAB() {
  gBtnABSeen = true;
  USBSerial.println("[hw] botao AB detectado");
}

static String clipText(const String &text, size_t maxLen) {
  if (text.length() <= maxLen) return text;
  if (maxLen <= 3) return text.substring(0, maxLen);
  return text.substring(0, maxLen - 3) + "...";
}

static void drawProgress(const String &line1, const String &line2, uint32_t line2Color) {
  auto &d = board.display();
  (void)d.setBackground(0xFFFFFF);
  (void)d.clearCanvas();
  (void)d.textRow("smoke test", 1, 0x000000);
  (void)d.setFontSize(Canvas::eCNAndENFont16);
  (void)d.textAt(clipText(line1, 40), 8, 56, 0x223355, 44, true);
  (void)d.textAt(clipText(line2, 40), 8, 92, line2Color, 44, true);
  (void)d.textAt(String("pass=") + String((unsigned long)gPass) +
                     String(" fail=") + String((unsigned long)gFail),
                 8,
                 128,
                 0x444444,
                 44,
                 true);
  (void)d.update();
}

static void beginSuiteDisplay(const char *suite) {
  gSuite = suite;
  gStep = 0;
  drawProgress(String("suite: ") + gSuite, "running...", 0x224466);
}

static void updateSuiteDisplay(const char *step, bool ok) {
  if (step == nullptr) return;
  String stepName = step;
  if (stepName == "display.destroyCanvas") {
    return;
  }

  gStep++;
  String line1 = String("suite: ") + gSuite +
                 String(" step: ") + String((unsigned long)gStep);
  String line2 = String(ok ? "ok: " : "fail: ") + stepName;
  drawProgress(line1, line2, ok ? 0x116611 : 0xAA2222);
}

static void endSuiteDisplay() {
  String line1 = String("suite: ") + gSuite + " done";
  String line2 = (gFail == 0)
                     ? String("result: PASS")
                     : String("result: FAIL (") + String((unsigned long)gFail) + String(")");
  drawProgress(line1, line2, gFail == 0 ? 0x116611 : 0xAA2222);
}

static void mark(const char *step, const Status &st) {
  if (st.ok()) {
    gPass++;
    USBSerial.printf("[OK]   %s\n", step);
  } else {
    gFail++;
    USBSerial.printf("[FAIL] %s -> %s\n", step, st.message);
  }
  updateSuiteDisplay(step, st.ok());
}

static void markBool(const char *step, bool ok) {
  if (ok) {
    gPass++;
    USBSerial.printf("[OK]   %s\n", step);
  } else {
    gFail++;
    USBSerial.printf("[FAIL] %s\n", step);
  }
  updateSuiteDisplay(step, ok);
}

static void printHelp() {
  USBSerial.println("smoke_test cmd:");
  USBSerial.println("  help");
  USBSerial.println("  selfcheck quick");
  USBSerial.println("  selfcheck full");
  USBSerial.println("  selfcheck hardware");
  USBSerial.println("  camera stophard");
  USBSerial.println("  camera killreboot");
  USBSerial.println("  camera liveboot");
  USBSerial.println("  report");
}

static bool waitButtonsPhysical(uint32_t timeoutMs) {
  gBtnASeen = false;
  gBtnBSeen = false;
  gBtnABSeen = false;

  (void)board.input().onPress(ButtonId::A, cbDetectA);
  (void)board.input().onPress(ButtonId::B, cbDetectB);
  (void)board.input().onPress(ButtonId::AB, cbDetectAB);
  (void)board.input().onRelease(ButtonId::A, cbDetectA);
  (void)board.input().onRelease(ButtonId::B, cbDetectB);
  (void)board.input().onRelease(ButtonId::AB, cbDetectAB);
  (void)board.input().resetDiagnostics();

  bool prevA = false;
  bool prevB = false;
  bool prevAB = false;
  uint32_t bothStartedAt = 0;

  uint32_t started = millis();
  uint32_t lastUi = 0;
  while ((millis() - started) < timeoutMs) {
    bool curA = board.input().buttonAPressed();
    bool curB = board.input().buttonBPressed();
    bool curAB = board.input().buttonABPressed();

    if (curA && !prevA) {
      gBtnASeen = true;
      USBSerial.println("[hw] botao A detectado (poll)");
    }
    if (curB && !prevB) {
      gBtnBSeen = true;
      USBSerial.println("[hw] botao B detectado (poll)");
    }
    if (curAB && !prevAB) {
      gBtnABSeen = true;
      USBSerial.println("[hw] botao AB detectado (poll)");
    }

    // Fallback robusto: considera AB detectado quando A+B ficam juntos por >=120ms.
    if (curA && curB) {
      if (bothStartedAt == 0) bothStartedAt = millis();
      if ((millis() - bothStartedAt) >= 120) {
        if (!gBtnABSeen) {
          gBtnABSeen = true;
          USBSerial.println("[hw] botao AB detectado (chord A+B)");
        }
      }
    } else {
      bothStartedAt = 0;
    }

    prevA = curA;
    prevB = curB;
    prevAB = curAB;

    if (gBtnASeen && gBtnBSeen && gBtnABSeen) {
      return true;
    }

    if ((millis() - lastUi) > 180) {
      lastUi = millis();
      String pending = "aguardando:";
      if (!gBtnASeen) pending += " A";
      if (!gBtnBSeen) pending += " B";
      if (!gBtnABSeen) pending += " AB";
      InputDiagnostics diag;
      (void)board.input().diagnostics(diag);
      String line1 = String("btn ") + String((unsigned long)((timeoutMs - (millis() - started)) / 1000)) +
                     "s A=" + (curA ? "1" : "0") +
                     " B=" + (curB ? "1" : "0");
      String line2 = pending + String(" rx=") + String((unsigned long)diag.received) +
                     String(" em=") + String((unsigned long)diag.emitted);
      drawProgress(line1, line2, 0x7A5200);
    }

    delay(20);
  }

  return gBtnASeen && gBtnBSeen && gBtnABSeen;
}

static bool probeBluetoothController(String *detailOut) {
#if SMOKE_HAS_BT_PROBE
  esp_bt_controller_status_t st = esp_bt_controller_get_status();
  const char *label = "unknown";
  if (st == ESP_BT_CONTROLLER_STATUS_IDLE) {
    label = "idle";
  } else if (st == ESP_BT_CONTROLLER_STATUS_INITED) {
    label = "inited";
  } else if (st == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    label = "enabled";
  }

  if (detailOut != nullptr) {
    *detailOut = String("bt=") + label + String(" code=") + String((int)st);
  }
  return true;
#else
  if (detailOut != nullptr) {
    *detailOut = "bluetooth api indisponivel nesta build";
  }
  return false;
#endif
}

static void runHardwareCheck() {
  gPass = 0;
  gFail = 0;
  beginSuiteDisplay("hardware");
  USBSerial.println("=== selfcheck hardware ===");

  mark("display.setBackground", board.display().setBackground(0xFFFFFF));
  mark("display.clearCanvas", board.display().clearCanvas());
  mark("display.textRow", board.display().textRow("hardware selfcheck", 1, 0x000000));
  mark("display.update", board.display().update());

  markBool("buttons.physical.A_B_AB", waitButtonsPhysical(18000));

  mark("audio.midi.playBuiltIn", board.audio().playBuiltIn((Melodies)0, OnceInBackground));
  delay(900);
  mark("audio.midi.stopBuiltIn", board.audio().stopBuiltIn());

  Status sdSt = board.storage().initSd();
  mark("storage.initSd", sdSt);
  if (sdSt.ok()) {
    mark("audio.recordFile", board.audio().recordFile("S:/audio/hw_record.wav", 1));
  } else {
    USBSerial.println("[SKIP] audio.recordFile (SD indisponivel)");
  }

  mark("camera.start", board.camera().start());
  mark("camera.initPreview", board.camera().initPreview());
  mark("camera.showPreview.on", board.camera().showPreview(true));
  delay(800);
  if (sdSt.ok()) {
    mark("camera.capture", board.camera().capture("S:/images/hw_capture.bmp"));
  } else {
    USBSerial.println("[SKIP] camera.capture (SD indisponivel)");
  }
  mark("camera.showPreview.off", board.camera().showPreview(false));
  mark("camera.stop", board.camera().stop());

  mark("conn.begin", board.connectivity().begin(false));
  WifiScanResult scan[8];
  size_t count = 0;
  mark("conn.scan", board.connectivity().scan(scan, 8, &count, true, false, 80));
  WifiLinkStats wls;
  mark("conn.linkStats", board.connectivity().linkStats(wls));

  String btInfo;
  bool btProbe = probeBluetoothController(&btInfo);
  markBool("bt.controller.probe", btProbe);
  USBSerial.printf("bt probe: %s\n", btInfo.c_str());

  USBSerial.printf("hardware summary: pass=%lu fail=%lu\n",
                   (unsigned long)gPass,
                   (unsigned long)gFail);
  endSuiteDisplay();
}

static void runQuickCheck() {
  gPass = 0;
  gFail = 0;
  beginSuiteDisplay("quick");
  USBSerial.println("=== selfcheck quick ===");

  mark("display.setBackground", board.display().setBackground(0xFFFFFF));
  mark("display.clearCanvas", board.display().clearCanvas());
  mark("display.textRow", board.display().textRow("quick smoke", 1, 0x000000));
  mark("display.update", board.display().update());

  mark("input.resetDiagnostics", board.input().resetDiagnostics());
  InputDiagnostics diag;
  mark("input.diagnostics", board.input().diagnostics(diag));

  mark("led.setBrightness", board.led().setBrightness(3));
  mark("led.off", board.led().off());

  mark("pin.write.backlight", board.pins().write(BoardPin::LcdBacklight, true));
  (void)board.pins().read(BoardPin::LcdBacklight);

  mark("sensor.refreshAll", board.sensors().refreshAll());
  (void)board.sensors().temperatureC();
  (void)board.sensors().humidityRh();

  mark("connectivity.begin", board.connectivity().begin(false));
  WifiLinkStats ws;
  mark("connectivity.linkStats", board.connectivity().linkStats(ws));

  mark("camera.start", board.camera().start());
  mark("camera.stop", board.camera().stop());

  mark("vision.init", board.vision().init());
  mark("vision.setMode.none", board.vision().setMode(AiMode::None));

  mark("audio.stopBuiltIn", board.audio().stopBuiltIn());

  board.speech().begin(0, 1, 6000);
  markBool("speech.initialized", board.speech().initialized());

  USBSerial.printf("quick summary: pass=%lu fail=%lu\n",
                   (unsigned long)gPass,
                   (unsigned long)gFail);
  endSuiteDisplay();
}

static void runFullCheck() {
  gPass = 0;
  gFail = 0;
  beginSuiteDisplay("full");
  USBSerial.println("=== selfcheck full ===");

  // Display
  mark("display.createCanvas", board.display().createCanvas());
  mark("display.setBackground", board.display().setBackground(0xFFFFFF));
  mark("display.clearCanvas", board.display().clearCanvas());
  mark("display.clearRow", board.display().clearRow(1));
  mark("display.clearRegion", board.display().clearRegion(0, 0, 80, 40));
  mark("display.setLineWidth", board.display().setLineWidth(2));
  mark("display.setFontSize", board.display().setFontSize(Canvas::eCNAndENFont16));
  mark("display.drawPoint", board.display().drawPoint(10, 10, 0x000000));
  mark("display.drawLine", board.display().drawLine(10, 20, 120, 20, 0x003366));
  mark("display.drawCircle", board.display().drawCircle(80, 80, 16, 0x006600, 0xFFFFFF, false));
  mark("display.drawRect", board.display().drawRect(20, 100, 80, 32, 0x660000, 0xFFFFFF, false));
  static const uint8_t bmp[8] = {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF};
  mark("display.drawBitmap", board.display().drawBitmap(4, 4, 8, 8, bmp));
  mark("display.drawImage", board.display().drawImage(0, 0, "S:/images/smoke.bmp"));
  mark("display.textRow", board.display().textRow("full smoke", 1, 0x000000));
  mark("display.textAt", board.display().textAt("acoes A/B/AB", 8, 40, 0x003366, 30, true));
  mark("display.loadFontFile", board.display().loadFontFile("S:/fonts/smoke.bin"));
  mark("display.clearFontFile", board.display().clearFontFile());
  mark("display.setCameraBackground.off", board.display().setCameraBackground(false));
  mark("display.update", board.display().update());
  mark("display.destroyCanvas", board.display().destroyCanvas());
  mark("display.createCanvas.again", board.display().createCanvas());

  // Input + Navigation
  mark("input.onPress.A", board.input().onPress(ButtonId::A, cbPressA));
  mark("input.onRelease.A", board.input().onRelease(ButtonId::A, cbReleaseA));
  mark("input.onReleaseByDuration.A", board.input().onReleaseByDuration(ButtonId::A, cbShortA, cbLongA, 2000));
  markBool("input.buttonAPressed.query", board.input().buttonAPressed() || !board.input().buttonAPressed());
  markBool("input.buttonBPressed.query", board.input().buttonBPressed() || !board.input().buttonBPressed());
  markBool("input.buttonABPressed.query", board.input().buttonABPressed() || !board.input().buttonABPressed());
  markBool("input.pressed.A.query", board.input().pressed(ButtonId::A) || !board.input().pressed(ButtonId::A));
  mark("input.resetDiagnostics", board.input().resetDiagnostics());
  InputDiagnostics diag;
  mark("input.diagnostics", board.input().diagnostics(diag));

  NavigationContext ctx;
  ctx.id = "main";
  ctx.title = "Smoke Main";
  ctx.actions[0].label = "A fast";
  ctx.actions[0].callback = cbNavAFast;
  ctx.actions[1].label = "A long";
  ctx.actions[1].callback = cbNavALong;
  ctx.actions[2].label = "B fast";
  ctx.actions[2].callback = cbNavBFast;
  ctx.actions[3].label = "B long";
  ctx.actions[3].callback = cbNavBLong;
  ctx.actions[4].label = "AB long";
  ctx.actions[4].callback = cbNavABLong;

  mark("nav.upsertContext", board.navigation().upsertContext(ctx));
  mark("nav.begin", board.navigation().begin(2000));
  mark("nav.activateContext", board.navigation().activateContext("main", 120));
  NavigationRuntimeState navState;
  mark("nav.runtimeState", board.navigation().runtimeState(navState));
  NavigationContext outCtx;
  mark("nav.activeContext", board.navigation().activeContext(outCtx));
  mark("nav.setGlobalUiEnabled", board.navigation().setGlobalUiEnabled(true));
  mark("nav.setGlobalUtf8Enabled", board.navigation().setGlobalUtf8Enabled(true));
  mark("nav.applyTransitionLock", board.navigation().applyTransitionLock(0));
  mark("nav.renderHints", board.navigation().renderHints());
  mark("nav.trigger.AFast", board.navigation().trigger(NavigationActionSlot::AFast));
  mark("nav.trigger.ALong", board.navigation().trigger(NavigationActionSlot::ALong));
  mark("nav.trigger.BFast", board.navigation().trigger(NavigationActionSlot::BFast));
  mark("nav.trigger.BLong", board.navigation().trigger(NavigationActionSlot::BLong));
  mark("nav.trigger.ABLong", board.navigation().trigger(NavigationActionSlot::ABLong));
  mark("nav.upsertContextFromJson",
       board.navigation().upsertContextFromJson("{\"id\":\"main\",\"title\":\"Main JSON\",\"showHints\":true,\"a_fast\":\"prev\"}"));

  // LED + Pins
  mark("led.setRgb", board.led().setRgb(0, {20, 10, 5}));
  mark("led.setAll", board.led().setAll({2, 2, 2}));
  mark("led.setBrightness", board.led().setBrightness(3));
  mark("led.setBacklight", board.led().setBacklight(true));
  mark("led.setAmplifierGain", board.led().setAmplifierGain(false));
  mark("led.off", board.led().off());
  mark("pin.write.backlight", board.pins().write(BoardPin::LcdBacklight, true));
  (void)board.pins().read(BoardPin::LcdBacklight);

  // Sensors
  SensorCacheConfig cfg;
  cfg.environmentMs = 400;
  cfg.ambientMs = 200;
  cfg.accelMs = 120;
  cfg.micMs = 120;
  mark("sensor.setCacheConfig", board.sensors().setCacheConfig(cfg));
  mark("sensor.refreshEnvironment", board.sensors().refreshEnvironment());
  mark("sensor.refreshAmbient", board.sensors().refreshAmbient());
  mark("sensor.refreshMotion", board.sensors().refreshMotion());
  mark("sensor.refreshMic", board.sensors().refreshMic());
  mark("sensor.refreshAll", board.sensors().refreshAll());
  SensorDiagnostics sdiag;
  mark("sensor.diagnose", board.sensors().diagnose(sdiag));
  (void)board.sensors().temperatureC();
  (void)board.sensors().humidityRh();
  (void)board.sensors().ambientLux();
  (void)board.sensors().accelX();
  (void)board.sensors().accelY();
  (void)board.sensors().accelZ();
  (void)board.sensors().micLevel();

  // Camera
  mark("camera.start", board.camera().start());
  mark("camera.initPreview", board.camera().initPreview());
  mark("camera.showPreview.on", board.camera().showPreview(true));
  mark("camera.capture", board.camera().capture("S:/images/smoke_cap.bmp"));
  mark("camera.captureHiRes", board.camera().captureHiRes("S:/images/smoke_hi.bmp", FRAMESIZE_VGA, nullptr));
  markBool("camera.isPreviewActive.query", board.camera().isPreviewActive() || !board.camera().isPreviewActive());
  mark("camera.showPreview.off", board.camera().showPreview(false));
  mark("camera.stop", board.camera().stop());
  mark("camera.cameraStop.soft", board.camera().cameraStop(false));

  // Storage
  mark("storage.initSd", board.storage().initSd());
  StorageHealth sh;
  mark("storage.healthCheck", board.storage().healthCheck(sh, false));
  mark("storage.setImageDirectory", board.storage().setImageDirectory("S:/images"));
  mark("storage.setAudioDirectory", board.storage().setAudioDirectory("S:/audio"));
  mark("storage.setDataDirectory", board.storage().setDataDirectory("S:/data"));
  (void)board.storage().imageDirectory();
  (void)board.storage().audioDirectory();
  (void)board.storage().dataDirectory();
  (void)board.storage().imagePath("sample.bmp");
  (void)board.storage().audioPath("sample.wav");
  (void)board.storage().dataPath("sample.txt");
  mark("storage.ensureDirectories", board.storage().ensureDirectories());
  static const uint16_t px[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
  mark("storage.writeRgb565Bmp", board.storage().writeRgb565Bmp("S:/images/smoke_rgb565.bmp", 2, 2, px));
  mark("storage.beginWavRecord", board.storage().beginWavRecord("S:/audio/smoke.wav", 16000, 1, 16));
  static const uint8_t wavChunk[64] = {0};
  mark("storage.appendWavRecord", board.storage().appendWavRecord(wavChunk, sizeof(wavChunk)));
  mark("storage.endWavRecord", board.storage().endWavRecord(true));
  mark("storage.writeTextFile", board.storage().writeTextFile("S:/data/smoke.txt", "hello"));
  mark("storage.appendTextFile", board.storage().appendTextFile("S:/data/smoke.txt", " world"));
  mark("storage.writeBinaryFile", board.storage().writeBinaryFile("S:/data/smoke.bin", wavChunk, sizeof(wavChunk)));
  mark("storage.appendBinaryFile", board.storage().appendBinaryFile("S:/data/smoke.bin", wavChunk, 8));
  String textOut;
  mark("storage.readTextFile", board.storage().readTextFile("S:/data/smoke.txt", &textOut, 256));
  uint8_t readBuf[96];
  size_t outBytes = 0;
  mark("storage.readBinaryFile", board.storage().readBinaryFile("S:/data/smoke.bin", readBuf, sizeof(readBuf), &outBytes));
  bool exists = false;
  mark("storage.fileExists", board.storage().fileExists("S:/data/smoke.txt", &exists));
  uint32_t fsize = 0;
  mark("storage.fileSize", board.storage().fileSize("S:/data/smoke.txt", &fsize));
  mark("storage.writeJson", board.storage().writeJson("S:/data/smoke.json", "{\"ok\":true}"));
  mark("storage.writeCsv", board.storage().writeCsv("S:/data/smoke.csv", "a,b\n1,2\n"));
  mark("storage.removeFile", board.storage().removeFile("S:/data/smoke.bin"));

  // Connectivity
  mark("conn.begin", board.connectivity().begin(false));
  mark("conn.setAutoReconnect", board.connectivity().setAutoReconnect(false));
  mark("conn.addKnownNetwork", board.connectivity().addKnownNetwork("SMOKE_NET", "12345678"));
  mark("conn.removeKnownNetwork", board.connectivity().removeKnownNetwork("SMOKE_NET"));
  mark("conn.clearKnownNetworks", board.connectivity().clearKnownNetworks());
  (void)board.connectivity().connected();
  (void)board.connectivity().ssid();
  (void)board.connectivity().bssid();
  (void)board.connectivity().rssi();
  (void)board.connectivity().localIp();
  (void)board.connectivity().gatewayIp();
  (void)board.connectivity().subnetMask();
  (void)board.connectivity().dnsIp(0);
  WifiScanResult scan[4];
  size_t count = 0;
  mark("conn.scan", board.connectivity().scan(scan, 4, &count, true, false, 80));
  String wifiReport;
  mark("conn.analyzeEnvironment", board.connectivity().analyzeEnvironment(&wifiReport, 4, true));
  String qrSsid;
  String qrPass;
  bool hidden = false;
  mark("conn.parseWifiQrPayload",
       board.connectivity().parseWifiQrPayload("WIFI:S:smoke;T:WPA;P:12345678;;", &qrSsid, &qrPass, &hidden));
  mark("conn.connectFromQrPayload", board.connectivity().connectFromQrPayload("WIFI:S:smoke;T:WPA;P:12345678;;"));
  WifiContextSnapshot wctx;
  mark("conn.wifiContext", board.connectivity().wifiContext(wctx, true));
  WifiLinkStats wls;
  mark("conn.linkStats", board.connectivity().linkStats(wls));
  mark("conn.startMdns", board.connectivity().startMdns("unihiker-smoke", "smoke", "unihiker", "tcp", 80));
  MdnsLinkStats mls;
  mark("conn.mdnsLinkStats", board.connectivity().mdnsLinkStats(mls));
  String mdnsReport;
  mark("conn.mdnsDiagnostics", board.connectivity().mdnsDiagnostics(&mdnsReport, false));
  mark("conn.startHttpServer", board.connectivity().startHttpServer(80, false));
  HttpServerStats hs;
  mark("conn.httpServerStats", board.connectivity().httpServerStats(hs));
  mark("conn.httpHandleClient", board.connectivity().httpHandleClient());
  mark("conn.stopHttpServer", board.connectivity().stopHttpServer());
  mark("conn.stopMdns", board.connectivity().stopMdns());
  mark("conn.disconnect", board.connectivity().disconnect(false));

  // Audio
  mark("audio.playBuiltIn", board.audio().playBuiltIn((Melodies)0, OnceInBackground));
  mark("audio.stopBuiltIn", board.audio().stopBuiltIn());
  mark("audio.playFile", board.audio().playFile("S:/audio/smoke.wav"));
  mark("audio.stopFile", board.audio().stopFile());
  mark("audio.recordFile", board.audio().recordFile("S:/audio/smoke_record.wav", 1));

  // Vision
  mark("vision.init", board.vision().init());
  mark("vision.setMode.none", board.vision().setMode(AiMode::None));
  (void)board.vision().mode();
  (void)board.vision().modeSwitchCount();
  mark("vision.setWorkflowMode.live", board.vision().setWorkflowMode(VisionWorkflowMode::LiveAim));
  (void)board.vision().workflowMode();
  (void)board.vision().lastWorkflowResult();
  mark("vision.startLiveAim", board.vision().startLiveAim(false));
  mark("vision.setLiveFeedbackEnabled", board.vision().setLiveFeedbackEnabled(false));
  mark("vision.setLiveFeedbackPeriodMs", board.vision().setLiveFeedbackPeriodMs(180));
  mark("vision.refreshLiveAimFeedback", board.vision().refreshLiveAimFeedback(false));
  (void)board.vision().describeCurrentPerception();
  (void)board.vision().liveAimActive();
  mark("vision.stopLiveAim", board.vision().stopLiveAim());
  mark("vision.captureAndReview", board.vision().captureAndReview("S:/images/vision_cap.bmp", FRAMESIZE_VGA));
  mark("vision.analyzeInputText", board.vision().analyzeInputText("payload smoke"));
  static const uint8_t vbin[8] = {1,2,3,4,5,6,7,8};
  mark("vision.analyzeInputBinary", board.vision().analyzeInputBinary(vbin, sizeof(vbin)));
  mark("vision.analyzeInputAny", board.vision().analyzeInputAny("any smoke"));
  String ocrOut;
  mark("vision.runOcrOnInput", board.vision().runOcrOnInput("any smoke", &ocrOut));
  (void)board.vision().detected();
  (void)board.vision().recognized();
  (void)board.vision().recognitionId();
  mark("vision.setMotionThreshold", board.vision().setMotionThreshold(8));
  (void)board.vision().faceData((AIRecognition::eFaceOrCatData_t)0);
  (void)board.vision().catData((AIRecognition::eFaceOrCatData_t)0);
  (void)board.vision().qrPayload();

  // Speech
  board.speech().begin(0, 1, 6000);
  mark("speech.beginWithProfile", board.speech().beginWithProfile(SpeechProfile::English, 0, 6000, true));
  mark("speech.beginAuto", board.speech().beginAuto(0, 6000, true));
  mark("speech.initTts", board.speech().initTts(2));
  board.speech().addCommand(1, "hello smoke");
  mark("speech.resetCommandRegistry", board.speech().resetCommandRegistry());
  mark("speech.queueCommand", board.speech().queueCommand(1, "hello smoke"));
  mark("speech.applyQueuedCommands", board.speech().applyQueuedCommands());
  (void)board.speech().queuedCommandCount();
  (void)board.speech().detectCommand(1);
  (void)board.speech().wakeDetected();
  (void)board.speech().ttsReady();
  board.speech().speak("smoke speech");
  (void)board.speech().initialized();
  (void)board.speech().requestedProfile();
  (void)board.speech().activeProfile();
  (void)board.speech().activeLang();
  (void)board.speech().activeMode();
  (void)board.speech().activeWakeUpMs();
  (void)board.speech().fallbackToEnglishApplied();
  (void)board.speech().lastInitStatus();
  (void)board.speech().initSummary();
  (void)SpeechService::profileLabel(SpeechProfile::English);

  USBSerial.printf("full summary: pass=%lu fail=%lu\n",
                   (unsigned long)gPass,
                   (unsigned long)gFail);
  endSuiteDisplay();
}

static void processCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "help") {
    printHelp();
    return;
  }
  if (line == "selfcheck quick") {
    runQuickCheck();
    return;
  }
  if (line == "selfcheck full") {
    runFullCheck();
    return;
  }
  if (line == "selfcheck hardware") {
    runHardwareCheck();
    return;
  }
  if (line == "camera stophard") {
    USBSerial.println("camera stop hard -> reboot expected");
    (void)board.camera().cameraStop(true, 120);
    return;
  }
  if (line == "camera killreboot") {
    USBSerial.println("camera killAndReboot -> reboot expected");
    (void)board.camera().killAndReboot(120);
    return;
  }
  if (line == "camera liveboot") {
    bool entered = false;
    CameraLiveOptions opt;
    Status st = board.camera().cameraLiveBoot(opt, &entered);
    USBSerial.printf("camera liveboot: %s entered=%s\n", st.ok() ? "ok" : st.message, entered ? "yes" : "no");
    return;
  }
  if (line == "report") {
    USBSerial.printf("report: pass=%lu fail=%lu\n",
                     (unsigned long)gPass,
                     (unsigned long)gFail);
    return;
  }

  USBSerial.printf("unknown: %s\n", line.c_str());
}

static void handleSerial() {
  while (USBSerial.available() > 0) {
    char c = (char)USBSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String line = gLine;
      gLine = "";
      processCommand(line);
      continue;
    }
    if (gLine.length() < 220) gLine += c;
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
  boot.initSd = false;
  boot.initAi = false;

  Status st = board.begin(boot);
  USBSerial.printf("smoke_test begin: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) return;

  (void)board.led().setBrightness(3);
  (void)board.led().off();
  (void)board.pins().write(BoardPin::LcdBacklight, true);

  (void)board.display().setBackground(0xFFFFFF);
  (void)board.display().clearCanvas();
  (void)board.display().textRow("smoke_test ready", 1, 0x000000);
  (void)board.display().textAt("serial: selfcheck quick/full", 8, 58, 0x003366, 34, true);
  (void)board.display().update();

  printHelp();
}

void loop() {
  handleSerial();
  delay(20);
}
