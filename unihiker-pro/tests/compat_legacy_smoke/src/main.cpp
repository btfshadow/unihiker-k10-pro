#include <Arduino.h>
#include <unihiker_pro_legacy.h>

unihiker_pro::UniHikerProLegacyShim k10;

void onButtonA() {
  USBSerial.println("legacy A pressed");
  k10.rgb->write(0, 255, 0, 0);
  k10.canvas->canvasText("A pressed", 10, 80, 0x880000,
                         k10.canvas->eCNAndENFont24, 20, true);
  k10.canvas->updateCanvas();
}

void onButtonB() {
  uint16_t lux = k10.readALS();
  USBSerial.printf("legacy B pressed, lux=%u\n", (unsigned)lux);
  k10.rgb->write(1, 0, 0, 255);
  k10.canvas->canvasText(String("Lux ") + String(lux), 10, 120, 0x003366,
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

  k10.canvas->canvasText("compat legacy smoke", 1, 0x000000);
  k10.canvas->canvasText("A: callback", 10, 180, 0x444444,
                         k10.canvas->eCNAndENFont16, 20, true);
  k10.canvas->canvasText("B: lux", 10, 210, 0x444444,
                         k10.canvas->eCNAndENFont16, 20, true);
  k10.canvas->updateCanvas();

  k10.buttonA->setPressedCallback(onButtonA);
  k10.buttonB->setPressedCallback(onButtonB);

  USBSerial.println("compat_legacy_smoke ready");
}

void loop() {
  delay(40);
}
