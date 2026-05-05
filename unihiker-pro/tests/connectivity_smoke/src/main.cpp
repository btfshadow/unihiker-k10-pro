#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;

static String gSerialLine;
static uint32_t gLastUiMs = 0;

static void drawStatus(const String &title,
                       const String &line1,
                       const String &line2,
                       uint32_t color = 0x000000) {
  auto &d = board.display();
  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.textRow("wifi smoke", 1, 0x000000);
  d.setFontSize(Canvas::eCNAndENFont16);
  d.textAt(title, 10, 56, color, 36, true);
  d.textAt(line1, 10, 86, color, 36, true);
  d.textAt(line2, 10, 116, color, 36, true);
  d.textAt("A: connect auto", 10, 250, 0x444444, 26, true);
  d.textAt("B: analyze env", 10, 276, 0x444444, 26, true);
  d.update();
}

static void printHelp() {
  USBSerial.println("wifi cmd:");
  USBSerial.println("  help");
  USBSerial.println("  stats");
  USBSerial.println("  scan");
  USBSerial.println("  analyze");
  USBSerial.println("  connect known");
  USBSerial.println("  connect auto");
  USBSerial.println("  connect <ssid>|<pass>");
  USBSerial.println("  add <ssid>|<pass>");
  USBSerial.println("  forget <ssid>");
  USBSerial.println("  clear");
  USBSerial.println("  disconnect");
  USBSerial.println("  qr WIFI:T:WPA;S:MinhaRede;P:Senha123;;");
}

static bool splitPair(const String &text, String *left, String *right) {
  if (!left || !right) return false;
  int sep = text.indexOf('|');
  if (sep <= 0) return false;
  *left = text.substring(0, sep);
  *right = text.substring(sep + 1);
  left->trim();
  right->trim();
  return left->length() > 0;
}

static void drawFromStats() {
  WifiLinkStats stats;
  Status st = board.connectivity().linkStats(stats);
  if (!st.ok()) {
    drawStatus("erro wifi", st.message, "linkStats", 0x880000);
    return;
  }

  String l1;
  String l2;
  if (stats.connected) {
    l1 = String("SSID: ") + stats.ssid;
    l2 = String("RSSI ") + String((long)stats.rssi) +
         String(" Q") + String((unsigned long)stats.qualityPercent) + "%";
  } else {
    l1 = "desconectado";
    l2 = String("profiles=") + String((unsigned long)stats.knownProfiles);
  }
  drawStatus("wifi", l1, l2, stats.connected ? 0x006600 : 0x663300);
}

static void runAutoConnect(bool allowScanFallback) {
  WifiConnectOptions options;
  options.timeoutMs = 6000;
  options.allowScanFallback = allowScanFallback;
  options.useStoredRadioHints = true;
  options.persistOnSuccess = true;

  Status st = board.connectivity().connectKnown(options);
  USBSerial.printf("wifi auto connect (%s): %s\n",
                   allowScanFallback ? "scan" : "fast-only",
                   st.ok() ? "ok" : st.message);
  drawFromStats();
}

static void onButtonA() {
  runAutoConnect(true);
}

static void onButtonB() {
  String report;
  Status st = board.connectivity().analyzeEnvironment(&report, 8, true);
  if (!st.ok()) {
    USBSerial.printf("wifi analyze fail: %s\n", st.message);
    drawStatus("analyze erro", st.message, "", 0x880000);
    return;
  }

  USBSerial.println(report);
  drawStatus("analyze ok", "relatorio no serial", "comando: scan", 0x003366);
}

static void processCommand(const String &line) {
  if (line.length() == 0) return;

  if (line == "help") {
    printHelp();
    return;
  }

  if (line == "stats") {
    WifiLinkStats stats;
    Status st = board.connectivity().linkStats(stats);
    USBSerial.printf("wifi stats: %s\n", st.ok() ? "ok" : st.message);
    if (st.ok()) {
      USBSerial.printf("  connected=%s ssid=%s rssi=%ld quality=%u channel=%u\n",
                       stats.connected ? "yes" : "no",
                       stats.ssid.c_str(),
                       (long)stats.rssi,
                       (unsigned)stats.qualityPercent,
                       (unsigned)stats.channel);
      USBSerial.printf("  ip=%s gw=%s dns1=%s profiles=%u reconnect=%lu\n",
                       stats.localIp.c_str(),
                       stats.gatewayIp.c_str(),
                       stats.dns1.c_str(),
                       (unsigned)stats.knownProfiles,
                       (unsigned long)stats.reconnectCount);
    }
    drawFromStats();
    return;
  }

  if (line == "scan") {
    WifiScanResult scan[16];
    size_t total = 0;
    Status st = board.connectivity().scan(scan, 16, &total, true, false, 120);
    if (!st.ok()) {
      USBSerial.printf("wifi scan fail: %s\n", st.message);
      drawStatus("scan erro", st.message, "", 0x880000);
      return;
    }

    USBSerial.printf("wifi scan total=%u (showing up to 16)\n", (unsigned)total);
    size_t show = total > 16 ? 16 : total;
    for (size_t i = 0; i < show; ++i) {
      USBSerial.printf("  %u) ssid=%s rssi=%ld ch=%u bssid=%s enc=%u\n",
                       (unsigned)(i + 1),
                       scan[i].ssid.c_str(),
                       (long)scan[i].rssi,
                       (unsigned)scan[i].channel,
                       scan[i].bssid.c_str(),
                       (unsigned)scan[i].encryption);
    }

    drawStatus("scan ok",
               String("total=") + String((unsigned long)total),
               "detalhes no serial",
               0x003366);
    return;
  }

  if (line == "analyze") {
    onButtonB();
    return;
  }

  if (line == "connect known") {
    runAutoConnect(false);
    return;
  }

  if (line == "connect auto") {
    runAutoConnect(true);
    return;
  }

  if (line == "disconnect") {
    Status st = board.connectivity().disconnect(false);
    USBSerial.printf("wifi disconnect: %s\n", st.ok() ? "ok" : st.message);
    drawFromStats();
    return;
  }

  if (line == "clear") {
    Status st = board.connectivity().clearKnownNetworks();
    USBSerial.printf("wifi clear: %s\n", st.ok() ? "ok" : st.message);
    drawFromStats();
    return;
  }

  if (line.startsWith("forget ")) {
    String ssid = line.substring(7);
    ssid.trim();
    Status st = board.connectivity().removeKnownNetwork(ssid);
    USBSerial.printf("wifi forget: %s\n", st.ok() ? "ok" : st.message);
    drawFromStats();
    return;
  }

  if (line.startsWith("add ")) {
    String ssid;
    String pass;
    if (!splitPair(line.substring(4), &ssid, &pass)) {
      USBSerial.println("uso: add <ssid>|<pass>");
      return;
    }
    Status st = board.connectivity().addKnownNetwork(ssid, pass);
    USBSerial.printf("wifi add: %s\n", st.ok() ? "ok" : st.message);
    drawFromStats();
    return;
  }

  if (line.startsWith("connect ")) {
    String args = line.substring(8);
    String ssid;
    String pass;
    if (!splitPair(args, &ssid, &pass)) {
      USBSerial.println("uso: connect <ssid>|<pass>");
      return;
    }

    WifiConnectOptions options;
    options.timeoutMs = 10000;
    options.allowScanFallback = false;
    options.useStoredRadioHints = true;
    options.persistOnSuccess = true;

    Status st = board.connectivity().connect(ssid, pass, options);
    USBSerial.printf("wifi connect: %s\n", st.ok() ? "ok" : st.message);
    drawFromStats();
    return;
  }

  if (line.startsWith("qr ")) {
    String payload = line.substring(3);
    payload.trim();

    WifiConnectOptions options;
    options.timeoutMs = 10000;
    options.allowScanFallback = false;
    options.useStoredRadioHints = true;
    options.persistOnSuccess = true;

    Status st = board.connectivity().connectFromQrPayload(payload, options);
    USBSerial.printf("wifi qr connect: %s\n", st.ok() ? "ok" : st.message);
    drawFromStats();
    return;
  }

  USBSerial.printf("comando desconhecido: %s\n", line.c_str());
}

static void handleSerial() {
  while (USBSerial.available() > 0) {
    char c = (char)USBSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String line = gSerialLine;
      gSerialLine = "";
      line.trim();
      processCommand(line);
      continue;
    }

    if (gSerialLine.length() < 300) {
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
  if (!st.ok()) {
    return;
  }

  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  st = board.connectivity().begin(true);
  USBSerial.printf("connectivity begin: %s\n", st.ok() ? "ok" : st.message);

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);

  printHelp();
  drawStatus("wifi pronto", "cmd: help", "A auto / B analyze", 0x000000);

  runAutoConnect(false);
}

void loop() {
  handleSerial();

  if (millis() - gLastUiMs >= 2500) {
    gLastUiMs = millis();
    drawFromStats();
  }

  delay(20);
}
