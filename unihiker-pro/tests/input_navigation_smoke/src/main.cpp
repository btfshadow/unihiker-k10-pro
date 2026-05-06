#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;
static String gLine;
static uint32_t gPage = 0;
static String gContext = "main";

static void drawUtf8Sample() {
  auto &d = board.display();
  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.textRow("teste UTF-8", 1, 0x000000);
  d.textAt("ação, seção, coração", 10, 72, 0x002244, 24, true);
  d.textAt("maçã, açúcar, canção", 10, 108, 0x002244, 24, true);
  d.textAt("informação: botão A/B", 10, 144, 0x002244, 30, true);
  d.textAt("Ç ç Ã ã É é Ê ê Ô ô", 10, 180, 0x224400, 30, true);
  d.textAt("comando: nav utf8 demo", 10, 234, 0x555555, 30, true);
  d.update();

  USBSerial.println("UTF8 demo:");
  USBSerial.println("  ação, seção, coração");
  USBSerial.println("  maçã, açúcar, canção");
  USBSerial.println("  informação, navegação, botão");
  USBSerial.println("  Ç ç Ã ã É é Ê ê Ô ô");
}

static void drawPage() {
  auto &d = board.display();
  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.textRow("menu nav smoke", 1, 0x000000);
  d.textAt(String("ctx: ") + gContext, 10, 42, 0x224466, 22, true);
  d.textAt(String("page: ") + String((unsigned long)gPage), 10, 70, 0x002244, 20, true);
  d.textAt("A curto / A longo", 10, 160, 0x444444, 24, true);
  d.textAt("B curto / B longo", 10, 190, 0x444444, 24, true);
  d.textAt("AB longo (>=2s)", 10, 220, 0x444444, 24, true);
  d.textAt("Transição bloqueia botoes", 10, 250, 0x666666, 30, true);
  (void)board.navigation().renderHints();
  d.update();
}

static void printHelp() {
  USBSerial.println("menu nav cmd:");
  USBSerial.println("  help");
  USBSerial.println("  stats");
  USBSerial.println("  reset");
  USBSerial.println("  page <n>");
  USBSerial.println("  nav state");
  USBSerial.println("  nav goto <id>");
  USBSerial.println("  nav ui <on|off>");
  USBSerial.println("  nav utf8 <on|off>");
  USBSerial.println("  nav json demo");
  USBSerial.println("  nav utf8 demo");
  USBSerial.println("  font load <S:/fonts/name.bin|name.ttf>");
  USBSerial.println("  font clear");
}

static void printStats() {
  InputDiagnostics diag;
  Status st = board.input().diagnostics(diag);
  USBSerial.printf("input stats: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) return;

  USBSerial.printf("  received=%lu emitted=%lu suppressed=%lu duplicates=%lu updatedAt=%lu\n",
                   (unsigned long)diag.received,
                   (unsigned long)diag.emitted,
                   (unsigned long)diag.suppressed,
                   (unsigned long)diag.duplicatesDetected,
                   (unsigned long)diag.updatedAtMs);
  USBSerial.printf("  press   A=%lu B=%lu AB=%lu\n",
                   (unsigned long)diag.pressReceived.a,
                   (unsigned long)diag.pressReceived.b,
                   (unsigned long)diag.pressReceived.ab);
  USBSerial.printf("  release A=%lu B=%lu AB=%lu\n",
                   (unsigned long)diag.releaseReceived.a,
                   (unsigned long)diag.releaseReceived.b,
                   (unsigned long)diag.releaseReceived.ab);
  USBSerial.printf("  short   A=%lu B=%lu AB=%lu\n",
                   (unsigned long)diag.shortEmitted.a,
                   (unsigned long)diag.shortEmitted.b,
                   (unsigned long)diag.shortEmitted.ab);
  USBSerial.printf("  long    A=%lu B=%lu AB=%lu\n",
                   (unsigned long)diag.longEmitted.a,
                   (unsigned long)diag.longEmitted.b,
                   (unsigned long)diag.longEmitted.ab);
}

static void printNavState() {
  NavigationRuntimeState state;
  Status st = board.navigation().runtimeState(state);
  USBSerial.printf("nav state: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) return;

  USBSerial.printf("  active=%s ctx=%s switchedAt=%lu lockUntil=%lu ui=%s utf8=%s\n",
                   state.active ? "yes" : "no",
                   state.activeContextId.c_str(),
                   (unsigned long)state.switchedAtMs,
                   (unsigned long)state.transitionLockUntilMs,
                   state.uiEnabled ? "on" : "off",
                   state.utf8Enabled ? "on" : "off");
}

static void onAShort() {
  if (gPage > 0) gPage--;
  USBSerial.printf("event: A rápido (page=%lu)\n", (unsigned long)gPage);
  drawPage();
}

static void onALong() {
  gPage = 0;
  USBSerial.println("event: A lento (voltar)");
  drawPage();
}

static void onBShort() {
  gPage++;
  USBSerial.printf("event: B rápido (page=%lu)\n", (unsigned long)gPage);
  drawPage();
}

static void onBLong() {
  gContext = "media";
  (void)board.navigation().activateContext("media", 320);
  USBSerial.println("event: B lento (ir para contexto media)");
  drawPage();
}

static void onABLong() {
  gContext = "main";
  gPage = 0;
  (void)board.navigation().activateContext("main", 320);
  USBSerial.println("event: AB lento (contexto raiz)");
  drawPage();
}

static void setupNavigation() {
  NavigationContext mainCtx;
  mainCtx.id = "main";
  mainCtx.title = "Menu Principal - Ação";
  mainCtx.transitionIgnoreMs = 320;
  mainCtx.ui.showHints = true;
  mainCtx.actions[0].label = "A rápido: anterior";
  mainCtx.actions[0].callback = onAShort;
  mainCtx.actions[1].label = "A lento: voltar à seção";
  mainCtx.actions[1].callback = onALong;
  mainCtx.actions[2].label = "B rápido: próximo";
  mainCtx.actions[2].callback = onBShort;
  mainCtx.actions[3].label = "B lento: ir mídia";
  mainCtx.actions[3].callback = onBLong;
  mainCtx.actions[4].label = "AB lento: raiz";
  mainCtx.actions[4].callback = onABLong;

  NavigationContext mediaCtx = mainCtx;
  mediaCtx.id = "media";
  mediaCtx.title = "Menu Mídia";
  mediaCtx.actions[0].label = "A rápido: quadro -";
  mediaCtx.actions[1].label = "A lento: voltar";
  mediaCtx.actions[2].label = "B rápido: quadro +";
  mediaCtx.actions[3].label = "B lento: destaque";
  mediaCtx.actions[4].label = "AB lento: raiz";

  (void)board.navigation().upsertContext(mainCtx);
  (void)board.navigation().upsertContext(mediaCtx);
  (void)board.navigation().begin(2000);
  (void)board.navigation().activateContext("main", 300);
}

static void processCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "help") {
    printHelp();
    return;
  }
  if (line == "stats") {
    printStats();
    return;
  }
  if (line == "reset") {
    Status st = board.input().resetDiagnostics();
    USBSerial.printf("input reset: %s\n", st.ok() ? "ok" : st.message);
    return;
  }
  if (line.startsWith("page ")) {
    long value = line.substring(5).toInt();
    if (value < 0) value = 0;
    gPage = (uint32_t)value;
    drawPage();
    USBSerial.printf("page set: %lu\n", (unsigned long)gPage);
    return;
  }

  if (line == "nav state") {
    printNavState();
    return;
  }

  if (line.startsWith("nav goto ")) {
    String id = line.substring(9);
    id.trim();
    Status st = board.navigation().activateContext(id, 320);
    USBSerial.printf("nav goto: %s\n", st.ok() ? "ok" : st.message);
    if (st.ok()) {
      gContext = id;
      drawPage();
    }
    return;
  }

  if (line.startsWith("nav ui ")) {
    String v = line.substring(7);
    v.trim();
    bool on = (v == "on");
    Status st = board.navigation().setGlobalUiEnabled(on);
    USBSerial.printf("nav ui: %s\n", st.ok() ? "ok" : st.message);
    drawPage();
    return;
  }

  if (line.startsWith("nav utf8 ")) {
    String v = line.substring(9);
    v.trim();
    bool on = (v == "on");
    Status st = board.navigation().setGlobalUtf8Enabled(on);
    USBSerial.printf("nav utf8: %s\n", st.ok() ? "ok" : st.message);
    drawPage();
    return;
  }

  if (line == "nav json demo") {
    String json =
        "{\"id\":\"main\",\"title\":\"Principal UTF-8 com ação\",\"showHints\":true," \
        "\"uiEnabled\":true,\"hintColor\":24000,\"hintY\":276," \
        "\"a_fast\":\"A rápido: voltar à seção\",\"a_long\":\"A lento: menu\"," \
        "\"b_fast\":\"B rápido: avançar\",\"b_long\":\"B lento: contexto\"," \
        "\"ab_long\":\"AB lento: raiz\"}";
    Status st = board.navigation().upsertContextFromJson(json);
    USBSerial.printf("nav json demo: %s\n", st.ok() ? "ok" : st.message);
    if (st.ok()) {
      (void)board.navigation().activateContext("main", 280);
      gContext = "main";
      drawPage();
    }
    return;
  }

  if (line == "nav utf8 demo") {
    drawUtf8Sample();
    return;
  }

  if (line.startsWith("font load ")) {
    String path = line.substring(10);
    path.trim();
    Status st = board.display().loadFontFile(path);
    USBSerial.printf("font load: %s (%s)\n", st.ok() ? "ok" : st.message, path.c_str());
    drawPage();
    return;
  }

  if (line == "font clear") {
    Status st = board.display().clearFontFile();
    USBSerial.printf("font clear: %s\n", st.ok() ? "ok" : st.message);
    drawPage();
    return;
  }

  USBSerial.printf("unknown: %s\n", line.c_str());
}

static void handleSerial() {
  while (USBSerial.available() > 0) {
    char c = (char)USBSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String line = gLine;
      gLine = "";
      processCommand(line);
      continue;
    }
    if (gLine.length() < 256) {
      gLine += c;
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
  USBSerial.printf("input_navigation_smoke begin: %s\n", st.ok() ? "ok" : st.message);
  if (!st.ok()) return;

  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  (void)board.input().resetDiagnostics();
  setupNavigation();
  drawPage();
  printHelp();
}

void loop() {
  handleSerial();
  delay(20);
}
