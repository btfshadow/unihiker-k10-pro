#include <Arduino.h>
#include <unihiker_pro_legacy.h>

unihiker_pro::UniHikerProLegacyShim k10;

void onButtonA() {
  k10.rgb->write(0, 0x00FF00);
  k10.canvas->canvasText("legacy A", 1, 0x000000);
  k10.canvas->canvasText("LED verde", 10, 76, 0x006600,
                         k10.canvas->eCNAndENFont24, 20, true);
  k10.canvas->updateCanvas();
}

void onButtonB() {
  uint16_t lux = k10.readALS();
  k10.rgb->write(1, 0x0000FF);
  k10.canvas->canvasText(String("Lux: ") + String(lux), 10, 116, 0x003366,
                         k10.canvas->eCNAndENFont24, 24, true);
  k10.canvas->updateCanvas();
}

void setup() {
  USBSerial.begin(115200);
  unsigned long t0 = millis();
  while (!USBSerial && millis() - t0 < 8000) {
    delay(10);
  }

  k10.begin();
  k10.initScreen(2, 0);
  k10.creatCanvas();
  k10.setScreenBackground(0xFFFFFF);
  k10.rgb->brightness(3);
  k10.rgb->write(0, 0x000000);
  k10.rgb->write(1, 0x000000);
  k10.rgb->write(2, 0x000000);

  k10.canvas->canvasText("legacy compat", 1, 0x000000);
  k10.canvas->canvasText("A: callback", 10, 176, 0x444444,
                         k10.canvas->eCNAndENFont16, 24, true);
  k10.canvas->canvasText("B: lux", 10, 206, 0x444444,
                         k10.canvas->eCNAndENFont16, 24, true);
  k10.canvas->updateCanvas();

  k10.buttonA->setPressedCallback(onButtonA);
  k10.buttonB->setPressedCallback(onButtonB);
}

void loop() {
  delay(30);
}
