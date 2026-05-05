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
  d.textAt("B: qr wifi listen", 10, 276, 0x444444, 26, true);
  d.update();
}

static void printHelp() {
  USBSerial.println("wifi cmd:");
  USBSerial.println("  help");
  USBSerial.println("  stats");
  USBSerial.println("  context");
  USBSerial.println("  scan");
  USBSerial.println("  analyze");
  USBSerial.println("  connect known");
  USBSerial.println("  connect auto");
  USBSerial.println("  connect <ssid>|<pass>");
  USBSerial.println("  add <ssid>|<pass>");
  USBSerial.println("  forget <ssid>");
  USBSerial.println("  clear");
  USBSerial.println("  disconnect");
  USBSerial.println("  qr listen");
  USBSerial.println("  mdns start <host>");
  USBSerial.println("  mdns stop");
  USBSerial.println("  mdns stats");
  USBSerial.println("  mdns query");
  USBSerial.println("  http start [port]");
  USBSerial.println("  http stop");
  USBSerial.println("  http stats");
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

static void runQrListen() {
  drawStatus("qr wifi", "abrindo camera IA", "aponte QR compativel", 0x003366);

  WifiConnectOptions options;
  options.timeoutMs = 10000;
  options.allowScanFallback = false;
  options.useStoredRadioHints = true;
  options.persistOnSuccess = true;

  String payload;
  Status st = board.connectivity().waitAndConnectFromVisionQr(board.vision(),
                                                              options,
                                                              30000,
                                                              180,
                                                              &payload);
  USBSerial.printf("wifi qr listen: %s\n", st.ok() ? "ok" : st.message);
  if (payload.length() > 0) {
    USBSerial.printf("wifi qr payload: %s\n", payload.c_str());
  }

  if (st.ok()) {
    drawFromStats();
    return;
  }

  String hint = payload.length() > 0 ? payload.substring(0, 34) : "sem qr wifi valido";
  drawStatus("qr wifi", st.message, hint, 0x884400);
}

static void onButtonB() {
  runQrListen();
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

  if (line == "context") {
    WifiContextSnapshot ctx;
    Status st = board.connectivity().wifiContext(ctx, true);
    USBSerial.printf("wifi context: %s\n", st.ok() ? "ok" : st.message);
    if (st.ok()) {
      USBSerial.printf("  connected=%s status=%u ssid=%s bssid=%s\n",
                       ctx.connected ? "yes" : "no",
                       (unsigned)ctx.statusCode,
                       ctx.ssid.c_str(),
                       ctx.bssid.c_str());
      USBSerial.printf("  ip=%s gw=%s mask=%s dns1=%s dns2=%s\n",
                       ctx.localIp.c_str(),
                       ctx.gatewayIp.c_str(),
                       ctx.subnetMask.c_str(),
                       ctx.dns1.c_str(),
                       ctx.dns2.c_str());
      USBSerial.printf("  mac=%s rssi=%ld quality=%u channel=%u\n",
                       ctx.stationMac.c_str(),
                       (long)ctx.rssi,
                       (unsigned)ctx.qualityPercent,
                       (unsigned)ctx.channel);
      USBSerial.printf("  known=%u reconnect=%lu success=%lu connectedSince=%lu updatedAt=%lu\n",
                       (unsigned)ctx.knownProfiles,
                       (unsigned long)ctx.reconnectCount,
                       (unsigned long)ctx.successfulConnectCount,
                       (unsigned long)ctx.connectedSinceMs,
                       (unsigned long)ctx.updatedAtMs);
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

  if (line == "qr listen") {
    runQrListen();
    return;
  }

  if (line.startsWith("mdns start ")) {
    String host = line.substring(11);
    host.trim();
    Status st = board.connectivity().startMdns(host,
                                               "unihiker-pro",
                                               "unihiker",
                                               "tcp",
                                               80);
    USBSerial.printf("mdns start: %s\n", st.ok() ? "ok" : st.message);
    return;
  }

  if (line == "mdns stop") {
    Status st = board.connectivity().stopMdns();
    USBSerial.printf("mdns stop: %s\n", st.ok() ? "ok" : st.message);
    return;
  }

  if (line == "mdns stats") {
    MdnsLinkStats md;
    Status st = board.connectivity().mdnsLinkStats(md);
    USBSerial.printf("mdns stats: %s\n", st.ok() ? "ok" : st.message);
    if (st.ok()) {
      USBSerial.printf("  running=%s host=%s instance=%s service=%s proto=%s port=%u\n",
                       md.running ? "yes" : "no",
                       md.host.c_str(),
                       md.instance.c_str(),
                       md.service.c_str(),
                       md.proto.c_str(),
                       (unsigned)md.port);
    }
    return;
  }

  if (line == "mdns query") {
    String report;
    Status st = board.connectivity().mdnsDiagnostics(&report, true);
    USBSerial.printf("mdns query: %s\n", st.ok() ? "ok" : st.message);
    if (st.ok()) {
      USBSerial.println(report);
    }
    return;
  }

  if (line.startsWith("http start")) {
    uint16_t port = 80;
    if (line.length() > 10) {
      String p = line.substring(10);
      p.trim();
      long parsed = p.toInt();
      if (parsed > 0 && parsed <= 65535) {
        port = (uint16_t)parsed;
      }
    }
    Status st = board.connectivity().startHttpServer(port, true);
    USBSerial.printf("http start: %s\n", st.ok() ? "ok" : st.message);
    if (st.ok()) {
      USBSerial.printf("http endpoints: /health /wifi/stats /mdns/stats /wifi/analyze\n");
    }
    return;
  }

  if (line == "http stop") {
    Status st = board.connectivity().stopHttpServer();
    USBSerial.printf("http stop: %s\n", st.ok() ? "ok" : st.message);
    return;
  }

  if (line == "http stats") {
    HttpServerStats hs;
    Status st = board.connectivity().httpServerStats(hs);
    USBSerial.printf("http stats: %s\n", st.ok() ? "ok" : st.message);
    if (st.ok()) {
      USBSerial.printf("  running=%s port=%u startedAt=%lu requests=%lu analyze=%s\n",
                       hs.running ? "yes" : "no",
                       (unsigned)hs.port,
                       (unsigned long)hs.startedAtMs,
                       (unsigned long)hs.requestCount,
                       hs.exposeAnalysis ? "on" : "off");
    }
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
  (void)board.connectivity().httpHandleClient();
  handleSerial();

  if (millis() - gLastUiMs >= 2500) {
    gLastUiMs = millis();
    drawFromStats();
  }

  delay(20);
}
