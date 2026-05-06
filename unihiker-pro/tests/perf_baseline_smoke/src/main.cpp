#include <Arduino.h>
#include <unihiker_pro.h>
#include "layers/core/utf8_utils.h"

using namespace unihiker_pro;

UniHikerPro board;
static String gSerialLine;

static uint32_t safeDiv(uint32_t a, uint32_t b) {
  return b == 0 ? 0 : (a / b);
}

static void printPerfKv(const String &payload) {
  USBSerial.print("PERF|");
  USBSerial.println(payload);
}

static uint32_t parseArgAt(const String &line, uint8_t index, uint32_t fallbackValue) {
  String trimmed = line;
  trimmed.trim();

  uint8_t token = 0;
  int start = 0;
  while (start < (int)trimmed.length()) {
    while (start < (int)trimmed.length() && trimmed[start] == ' ') start++;
    if (start >= (int)trimmed.length()) break;

    int end = start;
    while (end < (int)trimmed.length() && trimmed[end] != ' ') end++;
    if (token == index) {
      long n = trimmed.substring(start, end).toInt();
      return n > 0 ? (uint32_t)n : fallbackValue;
    }

    token++;
    start = end + 1;
  }

  return fallbackValue;
}

static void printSnapshot() {
  USBSerial.printf("perf snapshot:\n");
  USBSerial.printf("  uptime_ms=%lu cpu_mhz=%u\n",
                   (unsigned long)millis(),
                   (unsigned)getCpuFrequencyMhz());
  USBSerial.printf("  heap_free=%u heap_min=%u heap_max_alloc=%u\n",
                   (unsigned)ESP.getFreeHeap(),
                   (unsigned)ESP.getMinFreeHeap(),
                   (unsigned)ESP.getMaxAllocHeap());
  USBSerial.printf("  psram_free=%u psram_size=%u\n",
                   (unsigned)ESP.getFreePsram(),
                   (unsigned)ESP.getPsramSize());
  USBSerial.printf("  flash_size=%u sketch_size=%u\n",
                   (unsigned)ESP.getFlashChipSize(),
                   (unsigned)ESP.getSketchSize());

  String kv = "kind=snapshot";
  kv += "|uptime_ms=" + String((unsigned long)millis());
  kv += "|cpu_mhz=" + String((unsigned long)getCpuFrequencyMhz());
  kv += "|heap_free=" + String((unsigned long)ESP.getFreeHeap());
  kv += "|heap_min=" + String((unsigned long)ESP.getMinFreeHeap());
  kv += "|heap_max_alloc=" + String((unsigned long)ESP.getMaxAllocHeap());
  kv += "|psram_free=" + String((unsigned long)ESP.getFreePsram());
  kv += "|psram_size=" + String((unsigned long)ESP.getPsramSize());
  kv += "|flash_size=" + String((unsigned long)ESP.getFlashChipSize());
  kv += "|sketch_size=" + String((unsigned long)ESP.getSketchSize());
  printPerfKv(kv);
}

static uint32_t parseLoops(const String &line, uint32_t fallbackValue = 200) {
  int sep = line.lastIndexOf(' ');
  if (sep < 0) return fallbackValue;
  long n = line.substring(sep + 1).toInt();
  if (n <= 0) return fallbackValue;
  return (uint32_t)n;
}

static void benchUtf8(uint32_t loops) {
  String sample = "acao secao coracao maca acucar cancao -- sample utf8 path";
  uint32_t t0 = micros();
  size_t outLen = 0;
  for (uint32_t i = 0; i < loops; ++i) {
    String s = utf8::sanitize(sample);
    s = utf8::latinDisplayFallback(s);
    outLen += s.length();
  }
  uint32_t dt = micros() - t0;

  USBSerial.printf("bench utf8: loops=%lu total_us=%lu avg_us=%lu out_len_sum=%u\n",
                   (unsigned long)loops,
                   (unsigned long)dt,
                   (unsigned long)safeDiv(dt, loops),
                   (unsigned)outLen);

  String kv = "kind=bench_utf8";
  kv += "|loops=" + String((unsigned long)loops);
  kv += "|total_us=" + String((unsigned long)dt);
  kv += "|avg_us=" + String((unsigned long)safeDiv(dt, loops));
  kv += "|out_len_sum=" + String((unsigned long)outLen);
  printPerfKv(kv);
}

static void benchDisplay(uint32_t loops) {
  auto &d = board.display();
  uint32_t t0 = millis();

  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.setFontSize(Canvas::eCNAndENFont16);

  for (uint32_t i = 0; i < loops; ++i) {
    int16_t y = (int16_t)(40 + (i % 8) * 26);
    String txt = String("perf line ") + String((unsigned long)i);
    d.textAt(txt, 10, y, 0x003366, 32, true);
  }

  d.update();
  uint32_t dt = millis() - t0;

  USBSerial.printf("bench display: loops=%lu total_ms=%lu avg_us_per_op=%lu\n",
                   (unsigned long)loops,
                   (unsigned long)dt,
                   (unsigned long)safeDiv(dt * 1000UL, loops));

  String kv = "kind=bench_display";
  kv += "|loops=" + String((unsigned long)loops);
  kv += "|total_ms=" + String((unsigned long)dt);
  kv += "|avg_us_per_op=" + String((unsigned long)safeDiv(dt * 1000UL, loops));
  printPerfKv(kv);
}

static void runReport(uint32_t loops, uint32_t rounds) {
  printPerfKv("kind=report|phase=start|loops=" + String((unsigned long)loops) +
              "|rounds=" + String((unsigned long)rounds));

  for (uint32_t i = 0; i < rounds; ++i) {
    printPerfKv("kind=report|phase=round|index=" + String((unsigned long)(i + 1)));
    printSnapshot();
    benchUtf8(loops * 3);
    benchDisplay(loops);
    printSnapshot();
  }

  printPerfKv("kind=report|phase=end|rounds=" + String((unsigned long)rounds));
}

static void drawReady() {
  auto &d = board.display();
  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.textRow("perf baseline smoke", 1, 0x000000);
  d.setFontSize(Canvas::eCNAndENFont16);
  d.textAt("A: snapshot", 10, 64, 0x003366, 30, true);
  d.textAt("B: bench display 120", 10, 96, 0x003366, 30, true);
  d.textAt("serial: help", 10, 128, 0x444444, 30, true);
  d.update();
}

static void printHelp() {
  USBSerial.println("perf cmd:");
  USBSerial.println("  help");
  USBSerial.println("  snapshot");
  USBSerial.println("  bench utf8 [loops]");
  USBSerial.println("  bench display [loops]");
  USBSerial.println("  bench all [loops]");
  USBSerial.println("  report [loops] [rounds]");
}

static void onButtonA() {
  printSnapshot();
}

static void onButtonB() {
  benchDisplay(120);
}

static void processCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "help") {
    printHelp();
    return;
  }

  if (line == "snapshot") {
    printSnapshot();
    return;
  }

  if (line.startsWith("bench utf8")) {
    uint32_t loops = parseLoops(line, 600);
    benchUtf8(loops);
    return;
  }

  if (line.startsWith("bench display")) {
    uint32_t loops = parseLoops(line, 120);
    benchDisplay(loops);
    return;
  }

  if (line.startsWith("bench all")) {
    uint32_t loops = parseLoops(line, 200);
    printSnapshot();
    benchUtf8(loops * 3);
    benchDisplay(loops);
    printSnapshot();
    return;
  }

  if (line.startsWith("report")) {
    uint32_t loops = parseArgAt(line, 1, 200);
    uint32_t rounds = parseArgAt(line, 2, 3);
    if (rounds > 20) rounds = 20;
    runReport(loops, rounds);
    return;
  }

  USBSerial.printf("unknown: %s\n", line.c_str());
}

static void handleSerial() {
  while (USBSerial.available() > 0) {
    char c = (char)USBSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String line = gSerialLine;
      gSerialLine = "";
      processCommand(line);
      continue;
    }

    if (gSerialLine.length() < 180) {
      gSerialLine += c;
    }
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
  USBSerial.printf("perf_baseline_smoke begin: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) return;

  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);

  printHelp();
  printSnapshot();
  drawReady();
}

void loop() {
  handleSerial();
  delay(20);
}
