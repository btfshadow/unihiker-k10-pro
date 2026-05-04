#include <Arduino.h>
#include <Preferences.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;
Preferences prefs;

static const char *kNs = "cam_lifecycle";
static const char *kKeyCamOn = "cam_on";

static bool g_camOn = true;

static bool loadCamOnPref() {
  if (!prefs.begin(kNs, true)) return true;
  bool on = prefs.getBool(kKeyCamOn, true);
  prefs.end();
  return on;
}

static bool saveCamOnPref(bool on) {
  if (!prefs.begin(kNs, false)) return false;
  size_t n = prefs.putBool(kKeyCamOn, on);
  prefs.end();
  return n > 0;
}

static void drawUi() {
  board.display().setBackground(0x0F1218);
  board.display().clearCanvas();
  board.display().textRow("camera lifecycle test", 1, 0xFFFFFF);

  board.display().textAt(String("State: ") + (g_camOn ? "ON" : "OFF"),
                         10, 54, g_camOn ? 0x00FF66 : 0xFFB347, 24, true);
  board.display().textAt("A: toggle+reboot", 10, 112, 0x66CCFF, 22, true);
  board.display().textAt("B: kill camera", 10, 146, 0x66CCFF, 22, true);
  board.display().textAt("(hard stop + reboot)", 10, 180, 0x9AB0D3, 20, true);
  board.display().update();
}

void onButtonA() {
  bool nextOn = !g_camOn;
  bool ok = saveCamOnPref(nextOn);
  USBSerial.printf("toggle request: %s -> %s (saved=%d)\\n",
                   g_camOn ? "ON" : "OFF",
                   nextOn ? "ON" : "OFF",
                   (int)ok);

  board.display().clearCanvas();
  board.display().textAt("Toggle Camera", 10, 40, 0x00FF66, 22, true);
  board.display().textAt(String("Next: ") + (nextOn ? "ON" : "OFF"), 10, 82, 0xFFFFFF, 24, true);
  board.display().textAt("Rebooting...", 10, 126, 0x66CCFF, 22, true);
  board.display().update();
  delay(350);
  ESP.restart();
}

void onButtonB() {
  // Persist OFF and use library hard-stop API.
  (void)saveCamOnPref(false);
  USBSerial.println("kill camera requested (OFF persisted)");

  board.display().clearCanvas();
  board.display().textAt("Killing Camera", 10, 40, 0xFFB347, 22, true);
  board.display().textAt("Hard stop + reboot", 10, 82, 0xFFFFFF, 22, true);
  board.display().update();

  (void)board.camera().killAndReboot(250);
}

void setup() {
  USBSerial.begin(115200);
  unsigned long t0 = millis();
  while (!USBSerial && millis() - t0 < 8000) {
    delay(10);
  }

  g_camOn = loadCamOnPref();

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

  if (g_camOn) {
    Status st = board.camera().start();
    USBSerial.printf("camera.start(): ok=%d code=%d\n", (int)st.ok(), (int)st.code);
  } else {
    Status st = board.camera().stop();
    USBSerial.printf("camera.stop(): ok=%d code=%d\n", (int)st.ok(), (int)st.code);
  }

  drawUi();

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);
}

void loop() {
  delay(50);
}
