// camera_smoke — preview de câmera + captura hi-res com barra de progresso
// Button A: liga/desliga preview
// Button B: captura QVGA estável no SD com barra de progresso
#include <Arduino.h>
#include <esp_camera.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;
static bool previewOn = false;
static int photoCount = 0;

// Barra de progresso
constexpr int16_t BAR_X = 10, BAR_Y = 270, BAR_W = 220, BAR_H = 16;

static void showProgress(uint8_t pct) {
  // Borda
  board.display().drawRect(BAR_X - 1, BAR_Y - 1, BAR_W + 2, BAR_H + 2,
                           0xFFFFFF, 0x444444, false);
  // Fundo
  board.display().drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, 0x444444, 0x444444, true);
  // Preenchimento
  if (pct > 0) {
    int16_t fillW = (int16_t)((uint32_t)pct * BAR_W / 100);
    board.display().drawRect(BAR_X, BAR_Y, fillW, BAR_H, 0x00CC00, 0x00CC00, true);
  }
  // Texto
  board.display().clearRegion(BAR_X, BAR_Y - 22, BAR_W, 20);
  board.display().setFontSize(Canvas::eCNAndENFont16);
  board.display().textAt(String(pct) + "%", BAR_X + BAR_W / 2 - 10, BAR_Y - 22,
                         0xFFFFFF, 6, true);
  board.display().setFontSize(Canvas::eCNAndENFont24);
  board.display().update();
}

void onButtonA() {
  previewOn = !previewOn;
  board.camera().showPreview(previewOn);
  if (!previewOn) {
    board.display().setBackground(0x222222);
    board.display().clearCanvas();
    board.display().textAt("Preview OFF", 10, 10, 0xFFFFFF, 20, true);
  } else {
    board.display().clearCanvas();
  }
  board.display().textAt("A:preview  B:capture", 10, 295, 0xAAAAAA, 22, true);
  board.display().update();
  board.led().setRgb(0, previewOn ? RgbColor{0, 200, 0} : RgbColor{50, 50, 50});
  USBSerial.printf("preview %s\n", previewOn ? "ON" : "OFF");
}

void onButtonB() {
  photoCount++;
  String path = String("S:/photo") + photoCount + ".bmp";

  board.display().textAt("Capturing...", 10, 10, 0xFFFF00, 20, true);
  showProgress(0);

  Status s = board.camera().captureHiRes(path, FRAMESIZE_QVGA, showProgress);

  if (s.ok()) {
    USBSerial.printf("saved: %s\n", path.c_str());
    board.display().clearCanvas();
    board.display().textAt("Saved: photo" + String(photoCount) + ".bmp",
                           10, 10, 0x00FF00, 20, true);
    board.led().setRgb(1, RgbColor{0, 0, 200});
  } else {
    USBSerial.printf("error: %s\n", s.message);
    board.display().clearCanvas();
    board.display().textAt("ERR: capture failed", 10, 10, 0xFF0000, 20, true);
    board.led().setRgb(1, RgbColor{200, 0, 0});
  }
  board.display().textAt("A:preview  B:capture", 10, 295, 0xAAAAAA, 22, true);
  board.display().update();
}

void setup() {
  USBSerial.begin(115200);
  unsigned long t0 = millis();
  while (!USBSerial && millis() - t0 < 8000) delay(10);

  BootOptions boot;
  boot.initScreen           = true;
  boot.createCanvas         = true;
  boot.initCameraBackground = true;
  boot.initSd               = true;
  boot.initAi               = false;

  board.begin(boot);
  USBSerial.println("camera_smoke boot");

  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  previewOn = true;
  board.display().clearCanvas();
  board.display().textAt("A:preview  B:capture", 10, 295, 0xAAAAAA, 22, true);
  board.display().update();

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);
}

void loop() {
  delay(50);
}
