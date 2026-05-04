#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;

static String gImageDir = "S:/images";
static String gAudioDir = "S:/audio";

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
  d.textAt("A: custom dirs", 10, 260, 0x444444, 22, true);
  d.textAt("B: reset default", 10, 285, 0x444444, 22, true);
  d.update();
}

static void applyDirectories(const String &imageDir, const String &audioDir) {
  gImageDir = imageDir;
  gAudioDir = audioDir;

  (void)board.storage().setImageDirectory(gImageDir);
  (void)board.storage().setAudioDirectory(gAudioDir);

  Status st = board.storage().initSd();
  if (!st.ok()) {
    USBSerial.printf("initSd error: %s\n", st.message);
    drawStatus("SD init failed", st.message, "check card", 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  st = board.storage().ensureDirectories();
  String imgPath = board.storage().imagePath("sample.bmp");
  String wavPath = board.storage().audioPath("sample.wav");

  USBSerial.printf("dirs image=%s audio=%s\n", gImageDir.c_str(), gAudioDir.c_str());
  USBSerial.printf("paths bmp=%s wav=%s\n", imgPath.c_str(), wavPath.c_str());
  USBSerial.printf("ensureDirectories: %s\n", st.ok() ? "ok" : st.message);

  if (!st.ok()) {
    drawStatus("dir create failed", st.message, "", 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  drawStatus("dirs ready", imgPath, wavPath, 0x006600);
  board.led().setRgb(0, {0, 170, 0});
}

static void onButtonA() {
  USBSerial.println("A -> set custom dirs");
  applyDirectories("S:/media/images", "S:/media/audio");
}

static void onButtonB() {
  USBSerial.println("B -> reset default dirs");
  applyDirectories("S:/images", "S:/audio");
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
  applyDirectories("S:/images", "S:/audio");
}

void loop() {
  delay(50);
}
