#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;

void onButtonAPress() {
  board.led().setRgb(0, {255, 0, 0});
  board.display().textAt("A pressed", 10, 36, 0x000000, 20, true);
  board.display().update();
}

void onButtonBPress() {
  uint16_t lux = board.sensors().ambientLux();
  board.display().textAt(String("Lux: ") + String(lux), 10, 66, 0x000000, 20, true);
  board.display().update();
}

void setup() {
  BootOptions boot;
  boot.initScreen = true;
  boot.createCanvas = true;
  boot.initCameraBackground = false;
  boot.initSd = false;
  boot.initAi = false;

  board.begin(boot);
  board.display().setBackground(0xFFFFFF);
  board.display().textRow("unihiker-pro ready", 1, 0x000000);
  board.display().update();

  board.led().setBrightness(3);
  board.input().onPress(ButtonId::A, onButtonAPress);
  board.input().onPress(ButtonId::B, onButtonBPress);
  board.pins().write(BoardPin::LcdBacklight, true);
}

void loop() {
  delay(50);
}
