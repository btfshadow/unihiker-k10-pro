// display_smoke — exercises all DisplayService primitives
#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;
static uint8_t step = 0;

static void runStep() {
  auto &d = board.display();

  switch (step % 6) {
    case 0:
      d.setBackground(0xFFFFFF);
      d.clearCanvas();
      d.textRow("step 0: text", 1, 0x000000);
      d.setFontSize(Canvas::eCNAndENFont24);
      d.textAt("Hello Unihiker!", 10, 60, 0x0000FF, 24, true);
      d.textAt("Font 24 EN+CN", 10, 90, 0xFF0000, 24, true);
      d.update();
      USBSerial.println("step 0: text");
      break;

    case 1:
      d.setBackground(0xF0F0F0);
      d.clearCanvas();
      d.textRow("step 1: lines", 1, 0x000000);
      d.setLineWidth(3);
      d.drawLine(10, 50, 230, 50, 0xFF0000);
      d.drawLine(10, 80, 230, 80, 0x00AA00);
      d.drawLine(10, 110, 230, 110, 0x0000FF);
      d.setLineWidth(1);
      d.update();
      USBSerial.println("step 1: lines");
      break;

    case 2:
      d.setBackground(0xFFFFFF);
      d.clearCanvas();
      d.textRow("step 2: shapes", 1, 0x000000);
      d.drawRect(20, 50, 80, 60, 0xFF0000, 0xFFFFFF, false);
      d.drawRect(120, 50, 80, 60, 0x0000FF, 0xCCCCFF, true);
      d.drawCircle(60, 170, 30, 0x009900, 0xFFFFFF, false);
      d.drawCircle(170, 170, 30, 0xAA6600, 0xFFEECC, true);
      d.update();
      USBSerial.println("step 2: shapes");
      break;

    case 3:
      d.setBackground(0xFFFFFF);
      d.clearCanvas();
      d.textRow("step 3: points", 1, 0x000000);
      for (int i = 0; i < 240; i += 4) {
        d.drawPoint(i, 80 + (i % 40), 0xFF0000);
        d.drawPoint(i, 140 - (i % 40), 0x0000FF);
      }
      d.update();
      USBSerial.println("step 3: points");
      break;

    case 4:
      d.setBackground(0xFFFFFF);
      d.clearCanvas();
      d.textRow("step 4: clear region", 1, 0x000000);
      d.drawRect(10, 50, 220, 180, 0x000000, 0xAAAAAA, true);
      d.update();
      delay(800);
      d.clearRegion(30, 70, 180, 140);
      d.textAt("region cleared", 35, 110, 0xFF0000, 20, false);
      d.update();
      USBSerial.println("step 4: clearRegion");
      break;

    case 5:
      d.setBackground(0xFFFFFF);
      d.clearCanvas();
      d.textRow("step 5: font size", 1, 0x000000);
      d.setFontSize(Canvas::eCNAndENFont16);
      d.textAt("Font size 16", 10, 60, 0x000000, 30, true);
      d.setFontSize(Canvas::eCNAndENFont24);
      d.textAt("Font size 24", 10, 100, 0x000000, 24, true);
      d.update();
      USBSerial.println("step 5: fonts");
      break;
  }
  step++;
}

void setup() {
  USBSerial.begin(115200);
  unsigned long t0 = millis();
  while (!USBSerial && millis() - t0 < 8000) delay(10);

  BootOptions boot;
  boot.initScreen  = true;
  boot.createCanvas = true;
  boot.initSd      = false;
  boot.initAi      = false;

  board.begin(boot);
  USBSerial.println("display_smoke boot");

  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  board.input().onPress(ButtonId::A, []() {
    USBSerial.println("btn A -> next step");
    runStep();
  });
  board.input().onPress(ButtonId::B, []() {
    USBSerial.println("btn B -> clear");
    board.display().setBackground(0xFFFFFF);
    board.display().clearCanvas();
    board.display().textRow("canvas cleared", 1, 0x777777);
    board.display().update();
    step = 0;
  });

  runStep();  // show step 0 on boot
}

void loop() {
  delay(50);
}
