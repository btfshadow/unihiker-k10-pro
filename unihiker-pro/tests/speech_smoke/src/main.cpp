#include <Arduino.h>
#include <Preferences.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;

static const uint32_t kCommandRegisterDelayMs = 6500;
static const uint16_t kWakeWindowMs = 12000;
static const uint32_t kVirtualWakeWindowMs = 12000;
static const size_t kMaxCommandEntries = 24;
static const size_t kPersistSlotsPerGroup = 6;
static const char *kPrefsNs = "speech_smoke";
static const uint8_t kGroupHello = 1;
static const uint8_t kGroupStatus = 2;

struct CommandEntry {
  uint8_t id;
  uint8_t group;
  String phrase;
  bool persisted;
  uint32_t hits;
};

static uint32_t gDetectedHello = 0;
static uint32_t gDetectedStatus = 0;
static bool gCommandsRegistered = false;
static uint32_t gSpeechInitAtMs = 0;
static uint32_t gLastWakeLogMs = 0;
static uint32_t gLastRegisterRetryMs = 0;
static bool gWakeSeen = false;
static bool gVirtualWakeEnabled = false;
static bool gSessionActive = false;
static uint32_t gSessionUntilMs = 0;
static bool gCalibrationMode = false;
static int gCalibrationIndex = 0;
static uint8_t gNextCommandId = 1;
static size_t gEntryCount = 0;
static String gSerialLine;
static CommandEntry gEntries[kMaxCommandEntries];
static Preferences gPrefs;

static void drawState(const String &title,
                      const String &line1,
                      const String &line2,
                      uint32_t color = 0x000000) {
  auto &d = board.display();
  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.textRow("speech smoke", 1, 0x000000);
  d.setFontSize(Canvas::eCNAndENFont16);
  d.textAt(title, 10, 56, color, 30, true);
  d.textAt(line1, 10, 84, color, 34, true);
  d.textAt(line2, 10, 112, color, 34, true);
  d.textAt("A: testar/prev", 10, 246, 0x444444, 24, true);
  d.textAt("B: contadores/next", 10, 272, 0x444444, 24, true);
  d.update();
}

static String normalizePhrase(const String &input) {
  String out = input;
  out.trim();
  out.toLowerCase();
  while (out.indexOf("  ") >= 0) {
    out.replace("  ", " ");
  }
  return out;
}

static String persistKey(uint8_t group, size_t index) {
  char key[16];
  snprintf(key, sizeof(key), "%c_%u", group == kGroupHello ? 'h' : 's', (unsigned int)index);
  return String(key);
}

static bool hasPhrase(const String &phrase) {
  String key = normalizePhrase(phrase);
  for (size_t i = 0; i < gEntryCount; ++i) {
    if (normalizePhrase(gEntries[i].phrase) == key) {
      return true;
    }
  }
  return false;
}

static bool addCommandEntry(uint8_t group, const String &phrase, bool persisted) {
  String normalized = normalizePhrase(phrase);
  if (normalized.length() == 0) {
    return false;
  }
  if (hasPhrase(normalized)) {
    return false;
  }
  if (gEntryCount >= kMaxCommandEntries) {
    return false;
  }

  CommandEntry &entry = gEntries[gEntryCount++];
  entry.id = gNextCommandId++;
  entry.group = group;
  entry.phrase = normalized;
  entry.persisted = persisted;
  entry.hits = 0;
  return true;
}

static void loadPersistedCommands() {
  if (!gPrefs.begin(kPrefsNs, true)) {
    return;
  }

  for (size_t i = 0; i < kPersistSlotsPerGroup; ++i) {
    String h = gPrefs.getString(persistKey(kGroupHello, i).c_str(), "");
    String s = gPrefs.getString(persistKey(kGroupStatus, i).c_str(), "");
    if (h.length() > 0) {
      addCommandEntry(kGroupHello, h, true);
    }
    if (s.length() > 0) {
      addCommandEntry(kGroupStatus, s, true);
    }
  }

  gPrefs.end();
}

static void savePersistedCommands() {
  if (!gPrefs.begin(kPrefsNs, false)) {
    USBSerial.println("speech: falha abrindo preferencias");
    return;
  }

  for (size_t i = 0; i < kPersistSlotsPerGroup; ++i) {
    gPrefs.putString(persistKey(kGroupHello, i).c_str(), "");
    gPrefs.putString(persistKey(kGroupStatus, i).c_str(), "");
  }

  size_t h = 0;
  size_t s = 0;
  for (size_t i = 0; i < gEntryCount; ++i) {
    const CommandEntry &e = gEntries[i];
    if (!e.persisted) continue;

    if (e.group == kGroupHello && h < kPersistSlotsPerGroup) {
      gPrefs.putString(persistKey(kGroupHello, h).c_str(), e.phrase);
      h++;
      continue;
    }

    if (e.group == kGroupStatus && s < kPersistSlotsPerGroup) {
      gPrefs.putString(persistKey(kGroupStatus, s).c_str(), e.phrase);
      s++;
    }
  }

  gPrefs.end();
}

static void queueDefaultCommandProfile() {
  gEntryCount = 0;
  gNextCommandId = 1;

  // PT-first profile for native PTBR model.
  addCommandEntry(kGroupHello, "ola luci", false);
  addCommandEntry(kGroupHello, "ola lusi", false);
  addCommandEntry(kGroupHello, "oi luci", false);
  addCommandEntry(kGroupHello, "oi lusi", false);

  addCommandEntry(kGroupStatus, "status luci", false);
  addCommandEntry(kGroupStatus, "estado luci", false);
  addCommandEntry(kGroupStatus, "status lusi", false);
  addCommandEntry(kGroupStatus, "estado lusi", false);
  addCommandEntry(kGroupStatus, "como esta luci", false);
  addCommandEntry(kGroupStatus, "situacao luci", false);

  loadPersistedCommands();

  board.speech().resetCommandRegistry();
  for (size_t i = 0; i < gEntryCount; ++i) {
    board.speech().queueCommand(gEntries[i].id, gEntries[i].phrase);
  }

  USBSerial.printf("speech: perfil em fila (%u)\n", (unsigned int)gEntryCount);
}

static void drawCalibrationHint() {
  if (gEntryCount == 0) {
    drawState("calibracao", "sem comandos", "", 0x444444);
    return;
  }

  if (gCalibrationIndex < 0) gCalibrationIndex = 0;
  if (gCalibrationIndex >= (int)gEntryCount) gCalibrationIndex = (int)gEntryCount - 1;

  const CommandEntry &e = gEntries[gCalibrationIndex];
  String l1 = String("alvo: ") + e.phrase;
  String l2 = String("hits=") + String((unsigned long)e.hits) + " wake+fale";
  drawState("calibracao", l1, l2, 0x003366);
}

static void setCalibrationMode(bool enabled) {
  gCalibrationMode = enabled;
  if (gCalibrationMode) {
    drawCalibrationHint();
    USBSerial.println("speech: calibracao ON (A prev, B next)");
  } else {
    drawState("pronto", "wake: ola luci", "diga comando", 0x000000);
    USBSerial.println("speech: calibracao OFF");
  }
}

static void stepCalibration(int delta) {
  if (gEntryCount == 0) return;
  int n = (int)gEntryCount;
  gCalibrationIndex = (gCalibrationIndex + delta) % n;
  if (gCalibrationIndex < 0) gCalibrationIndex += n;
  drawCalibrationHint();
}

static void openCommandSession(const char *sourceLabel) {
  gSessionActive = true;
  gSessionUntilMs = millis() + kVirtualWakeWindowMs;
  USBSerial.printf("speech: sessao aberta por %s (%lu ms)\n",
                   sourceLabel,
                   (unsigned long)kVirtualWakeWindowMs);
}

static void refreshSessionState() {
  if (!gSessionActive) return;
  if ((int32_t)(gSessionUntilMs - millis()) > 0) return;
  gSessionActive = false;
  USBSerial.println("speech: sessao expirada");
}

static void onButtonA() {
  if (gCalibrationMode) {
    stepCalibration(-1);
    return;
  }

  if (!board.speech().ttsReady()) {
    drawState("tts indisponivel", "fila TTS nao pronta", "somente ASR ativo", 0x884400);
    USBSerial.println("speech: tts indisponivel");
    return;
  }

  board.speech().speak("speech smoke ativo");
  drawState("tts enviado", "speech smoke ativo", "aguardando comandos", 0x006600);
  USBSerial.println("speech: disparo manual de fala");
}

static void onButtonB() {
  if (gCalibrationMode) {
    stepCalibration(1);
    return;
  }

  String l1 = String("ola=") + String((unsigned long)gDetectedHello);
  String l2 = String("status=") + String((unsigned long)gDetectedStatus);
  drawState("contadores", l1, l2, 0x000000);
}

static void printCommandList() {
  USBSerial.println("speech: command list");
  for (size_t i = 0; i < gEntryCount; ++i) {
    const CommandEntry &e = gEntries[i];
    USBSerial.printf("  id=%u group=%u hits=%lu persisted=%s phrase=%s\n",
                     (unsigned int)e.id,
                     (unsigned int)e.group,
                     (unsigned long)e.hits,
                     e.persisted ? "yes" : "no",
                     e.phrase.c_str());
  }
}

static void processSerialCommandLine(const String &line) {
  if (line.length() == 0) return;

  if (line == "help") {
    USBSerial.println("speech cmd: help | stats | list | preset pt | wakept on|off | calib on|off|next|prev|reset | add1 <f> | add2 <f> | save | clearpersist");
    return;
  }

  if (line == "preset pt") {
    queueDefaultCommandProfile();
    gCommandsRegistered = false;
    gSpeechInitAtMs = millis() - kCommandRegisterDelayMs;
    USBSerial.println("speech: preset PT aplicado");
    return;
  }

  if (line == "stats") {
    USBSerial.printf("speech stats: hello=%lu status=%lu queued=%u tts=%s wakept=%s session=%s\n",
                     (unsigned long)gDetectedHello,
                     (unsigned long)gDetectedStatus,
                     (unsigned int)board.speech().queuedCommandCount(),
                     board.speech().ttsReady() ? "ready" : "no",
                     gVirtualWakeEnabled ? "on" : "off",
                     gSessionActive ? "open" : "closed");
    return;
  }

  if (line == "wakept on") {
    gVirtualWakeEnabled = true;
    USBSerial.println("speech: wake virtual PT ligado");
    return;
  }

  if (line == "wakept off") {
    gVirtualWakeEnabled = false;
    USBSerial.println("speech: wake virtual PT desligado");
    return;
  }

  if (line == "list") {
    printCommandList();
    return;
  }

  if (line == "calib on") {
    setCalibrationMode(true);
    return;
  }

  if (line == "calib off") {
    setCalibrationMode(false);
    return;
  }

  if (line == "calib next") {
    setCalibrationMode(true);
    stepCalibration(1);
    return;
  }

  if (line == "calib prev") {
    setCalibrationMode(true);
    stepCalibration(-1);
    return;
  }

  if (line == "calib reset") {
    for (size_t i = 0; i < gEntryCount; ++i) gEntries[i].hits = 0;
    USBSerial.println("speech: hits resetados");
    if (gCalibrationMode) drawCalibrationHint();
    return;
  }

  if (line.startsWith("add1 ") || line.startsWith("add2 ")) {
    uint8_t group = line.startsWith("add1 ") ? kGroupHello : kGroupStatus;
    String phrase = line.substring(5);
    phrase.trim();

    if (!addCommandEntry(group, phrase, true)) {
      USBSerial.println("speech: add falhou (duplicado/cheio/invalido)");
      return;
    }

    CommandEntry &e = gEntries[gEntryCount - 1];
    board.speech().queueCommand(e.id, e.phrase);
    if (gCommandsRegistered) {
      board.speech().addCommand(e.id, e.phrase);
    }
    savePersistedCommands();
    USBSerial.printf("speech: add ok id=%u group=%u phrase=%s\n",
                     (unsigned int)e.id,
                     (unsigned int)e.group,
                     e.phrase.c_str());
    return;
  }

  if (line == "save") {
    savePersistedCommands();
    USBSerial.println("speech: comandos persistidos");
    return;
  }

  if (line == "clearpersist") {
    if (gPrefs.begin(kPrefsNs, false)) {
      for (size_t i = 0; i < kPersistSlotsPerGroup; ++i) {
        gPrefs.putString(persistKey(kGroupHello, i).c_str(), "");
        gPrefs.putString(persistKey(kGroupStatus, i).c_str(), "");
      }
      gPrefs.end();
    }

    queueDefaultCommandProfile();
    gCommandsRegistered = false;
    gSpeechInitAtMs = millis() - kCommandRegisterDelayMs;
    USBSerial.println("speech: persistencia limpa");
    return;
  }

  USBSerial.printf("speech: cmd desconhecido -> %s\n", line.c_str());
}

static void handleSerialCommands() {
  while (USBSerial.available() > 0) {
    char c = (char)USBSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String line = gSerialLine;
      gSerialLine = "";
      line.trim();
      processSerialCommandLine(line);
      continue;
    }

    if (gSerialLine.length() < 180) {
      gSerialLine += c;
    }
  }
}

static void registerCommandsWhenReady() {
  if (gCommandsRegistered) {
    return;
  }

  if (millis() - gSpeechInitAtMs < kCommandRegisterDelayMs) {
    return;
  }

  Status st = board.speech().applyQueuedCommands();
  if (!st.ok()) {
    if (millis() - gLastRegisterRetryMs > 1500) {
      gLastRegisterRetryMs = millis();
      USBSerial.printf("speech: falha registrando comandos (%s), retry...\n", st.message);
    }
    return;
  }

  gCommandsRegistered = true;
  USBSerial.println("speech: comandos PT registrados apos aquecimento");
  USBSerial.println("speech: serial cmd -> help");
  drawState("pronto", "wake: ola luci", "diga comando", 0x000000);
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

  Status speechInit = board.speech().beginWithProfile(SpeechProfile::PortugueseBrazil,
                                                      1,
                                                      kWakeWindowMs,
                                                      false);
  if (!speechInit.ok()) {
    drawState("erro speech", "modelo PTBR ausente", "sem fallback EN", 0x880000);
    USBSerial.printf("speech: init fail %s\n", speechInit.message);
    USBSerial.println("speech: gere/instale artifacts PTBR nativos e recompile");
    return;
  }

  String diag = board.speech().initSummary();
  USBSerial.printf("speech: %s\n", diag.c_str());
  Status ttsStatus = board.speech().initTts(2);
  USBSerial.printf("speech: tts init -> %s\n", ttsStatus.ok() ? "ok" : ttsStatus.message);

  queueDefaultCommandProfile();
  gSpeechInitAtMs = millis();

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);

  drawState("speech init", "aquecendo backend", "wake: ola luci", 0x000000);
  USBSerial.println("speech smoke ready");
}

void loop() {
  handleSerialCommands();
  registerCommandsWhenReady();
  refreshSessionState();

  if (board.speech().wakeDetected()) {
    if (!gWakeSeen) {
      gWakeSeen = true;
      openCommandSession("wake hardware");
      USBSerial.println("speech: wake detectado, fale comando agora");
    }
  } else {
    gWakeSeen = false;
  }

  if (!gCommandsRegistered) {
    delay(40);
    return;
  }

  if (!gSessionActive && millis() - gLastWakeLogMs > 3500) {
    gLastWakeLogMs = millis();
    USBSerial.println("speech: aguardando wake (ola luci)");
  }

  bool detectedAny = false;
  for (size_t i = 0; i < gEntryCount; ++i) {
    CommandEntry &e = gEntries[i];
    if (!board.speech().detectCommand(e.id)) {
      continue;
    }

    detectedAny = true;
    e.hits++;

    bool isHello = e.group == kGroupHello;
    bool isStatus = e.group == kGroupStatus;

    if (isHello && gVirtualWakeEnabled) {
      openCommandSession("wake PT");
    }

    if (isStatus && !gSessionActive) {
      USBSerial.printf("speech: status ignorado sem wake/sessao (frase=%s)\n", e.phrase.c_str());
      continue;
    }

    if (isHello) {
      gDetectedHello++;
    } else {
      gDetectedStatus++;
    }

    String l1 = String(e.group == kGroupHello ? "ola detectado #" : "status detectado #") +
                String((unsigned long)(e.group == kGroupHello ? gDetectedHello : gDetectedStatus));
    String l2 = String("frase: ") + e.phrase;
    if (gCalibrationMode) {
      bool target = ((int)i == gCalibrationIndex);
      l2 = String(target ? "alvo ok: " : "outro: ") + e.phrase;
      drawState("calibracao", l1, l2, target ? 0x006600 : 0x224466);
    } else {
      drawState("asr", l1, l2, 0x006600);
    }

    USBSerial.printf("speech: cmd group=%u id=%u phrase=%s hits=%lu\n",
                     (unsigned int)e.group,
                     (unsigned int)e.id,
                     e.phrase.c_str(),
                     (unsigned long)e.hits);

    if (e.group == kGroupHello) {
      board.speech().speak("ola recebido");
    } else {
      board.speech().speak("status ok");
    }
    break;
  }

  if (detectedAny) {
    delay(220);
  }

  delay(20);
}
