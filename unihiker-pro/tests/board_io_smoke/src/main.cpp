#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;
static bool backlightOn = true;
static bool ampGainOn = false;

void onButtonA() {
  backlightOn = !backlightOn;
  USBSerial.printf("button A, backlight=%d\n", backlightOn ? 1 : 0);
  board.led().setRgb(0, backlightOn ? RgbColor{0, 255, 0} : RgbColor{255, 0, 0});
  board.led().setBacklight(backlightOn);
  board.display().textAt(backlightOn ? "BL ON" : "BL OFF", 10, 40, 0x000000, 20, true);
  board.display().update();
}

void onButtonB() {
  ampGainOn = !ampGainOn;
  board.led().setRgb(1, ampGainOn ? RgbColor{0, 0, 255} : RgbColor{32, 32, 32});
  board.led().setAmplifierGain(ampGainOn);
  bool p3 = board.pins().read(BoardPin::P3);
  USBSerial.printf("button B, amp=%d, p3=%d\n", ampGainOn ? 1 : 0, p3 ? 1 : 0);
  board.display().textAt(String("AMP ") + (ampGainOn ? "ON" : "OFF") + String(" P3=") + String(p3),
                         10, 70, 0x000000, 24, true);
  board.display().update();
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
  USBSerial.println("board_io_smoke boot");
  board.display().setBackground(0xFFFFFF);
  board.display().textRow("board io smoke", 1, 0x000000);
  board.display().textAt("A: backlight", 10, 120, 0x000000, 24, true);
  board.display().textAt("B: amp gain", 10, 150, 0x000000, 24, true);
  board.display().update();

  board.led().setBrightness(3);
  board.led().off();
  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);
  board.pins().write(BoardPin::LcdBacklight, true);
}

void loop() {
  delay(50);
}
