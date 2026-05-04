#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;
static uint32_t gLastDrawMs = 0;

static void drawDiagnostics(const SensorDiagnostics &diag) {
  auto &d = board.display();
  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.textRow("sensors smoke", 1, 0x000000);
  d.setFontSize(Canvas::eCNAndENFont16);

  d.textAt(String("I2C: ") + (diag.i2cBusReady ? "OK" : "ERR"), 10, 52, 0x000000, 24, true);
  d.textAt(String("AHT20: ") + (diag.aht20Present ? "OK" : "MISS"), 10, 76, 0x000000, 24, true);
  d.textAt(String("ALS: ") + (diag.alsPresent ? "OK" : "MISS"), 10, 100, 0x000000, 24, true);
  d.textAt(String("ACCEL: ") + (diag.accelPresent ? "OK" : "MISS"), 10, 124, 0x000000, 24, true);
  d.textAt(String("MIC: ") + (diag.micAvailable ? "OK" : "MISS"), 10, 148, 0x000000, 24, true);

  d.setFontSize(Canvas::eCNAndENFont24);
  d.textAt("A: refresh all", 10, 270, 0x444444, 20, true);
  d.textAt("B: re-diagnose", 10, 295, 0x444444, 20, true);
  d.update();
}

static void drawLiveReadings() {
  auto &s = board.sensors();
  auto &d = board.display();

  float t = s.temperatureC();
  float h = s.humidityRh();
  uint16_t lux = s.ambientLux();
  int ax = s.accelX();
  int ay = s.accelY();
  int az = s.accelZ();
  uint64_t mic = s.micLevel();

  d.setFontSize(Canvas::eCNAndENFont16);
  d.textAt(String("T: ") + String(t, 1) + " C", 10, 176, 0x005500, 24, true);
  d.textAt(String("H: ") + String(h, 1) + " %", 10, 200, 0x005500, 24, true);
  d.textAt(String("LUX: ") + String(lux), 10, 224, 0x005500, 24, true);
  d.textAt(String("ACC: ") + String(ax) + "," + String(ay) + "," + String(az),
           10, 248, 0x005500, 30, true);
  d.update();

  USBSerial.printf("T=%.1fC H=%.1f%% LUX=%u ACC=(%d,%d,%d) MIC=%llu\n",
                   t, h, (unsigned)lux, ax, ay, az,
                   (unsigned long long)mic);
}

static void onButtonA() {
  USBSerial.println("A -> refreshAll");
  Status st = board.sensors().refreshAll();
  USBSerial.printf("refreshAll: %s\n", st.ok() ? "ok" : st.message);
  drawLiveReadings();
}

static void onButtonB() {
  USBSerial.println("B -> diagnose");
  SensorDiagnostics diag;
  Status st = board.sensors().diagnose(diag);
  USBSerial.printf("diagnose: %s\n", st.ok() ? "ok" : st.message);
  drawDiagnostics(diag);
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

  SensorCacheConfig cfg;
  cfg.environmentMs = 900;
  cfg.ambientMs = 500;
  cfg.accelMs = 150;
  cfg.micMs = 120;
  board.sensors().setCacheConfig(cfg);

  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);

  SensorDiagnostics diag;
  Status st = board.sensors().diagnose(diag);
  USBSerial.printf("sensors_smoke boot, diagnose=%s\n", st.ok() ? "ok" : st.message);
  drawDiagnostics(diag);
  drawLiveReadings();
}

void loop() {
  if (millis() - gLastDrawMs >= 1200) {
    gLastDrawMs = millis();
    drawLiveReadings();
  }
  delay(30);
}
