// camera_live_flow_test
//
// Este teste valida o fluxo simplificado com cameraLive padrão da library:
//   1) boot em menu
//   2) B curto no menu entra no live
//   3) no live, a própria library cuida de resolução/textos/captura/saída
//   4) B longo sai do live e retorna ao menu

#include <Arduino.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

UniHikerPro board;

static void enterLive();
static void bindMenuButtons();
static CameraLiveOptions makeLiveOptions();

static void drawMenu() {
  board.display().setBackground(0x101820);
  board.display().clearCanvas();
  board.display().textRow("MENU", 1, 0xFFFFFF);
  board.display().textAt("B<2s: entrar camera live", 8, 72, 0x44AAFF, 22);
  board.display().textAt("No live: A troca res", 8, 108, 0xCFE8FF, 20);
  board.display().textAt("B<2s captura | A>2s menu", 8, 140, 0xCFE8FF, 20);
  board.display().update();
  USBSerial.println("[menu] exibido");
}

static void onReturnContext() {
  // Chamado pela library após A longo (cameraStop padrão já executado).
  USBSerial.println("[live] retorno para menu");
  drawMenu();
  bindMenuButtons();
}

static void enterLive() {
  USBSerial.println("[menu] entrando no cameraLive padrão");
  CameraLiveOptions opts = makeLiveOptions();
  Status st = board.camera().cameraLive(opts);
  USBSerial.printf("[menu] cameraLive status=%d\n", (int)st.code);
}

static void bindMenuButtons() {
  board.input().onReleaseByDuration(ButtonId::B, enterLive, nullptr);
}

static CameraLiveOptions makeLiveOptions() {
  CameraLiveOptions opts;
  opts.onReturnContext = onReturnContext;
  return opts;
}

void setup() {
  USBSerial.begin(115200);
  unsigned long t0 = millis();
  while (!USBSerial && millis() - t0 < 6000) delay(10);
  USBSerial.println("=== camera_live_flow_test boot ===");

  BootOptions boot;
  boot.initScreen           = true;
  boot.createCanvas         = true;
  boot.initCameraBackground = false;  // cameraLive cuida disso
  boot.initSd               = false;  // evita loop infinito do framework em initSDFile
  boot.initAi               = false;
  board.begin(boot);

  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  bool enteredLive = false;
  CameraLiveOptions opts = makeLiveOptions();
  Status bootSt = board.camera().cameraLiveBoot(opts, &enteredLive);
  USBSerial.printf("[boot] cameraLiveBoot status=%d entered=%d\n", (int)bootSt.code,
                   (int)enteredLive);
  if (enteredLive) {
    return;
  }

  drawMenu();
  bindMenuButtons();
}

void loop() {
  delay(30);
}
