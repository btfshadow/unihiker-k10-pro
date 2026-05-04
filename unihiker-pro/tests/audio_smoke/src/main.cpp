#include <Arduino.h>
#include <driver/i2s.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

UniHikerPro board;
static TaskHandle_t gRecordTask = nullptr;
static volatile bool gRecordRunning = false;
static volatile bool gRecordStopRequested = false;
static volatile bool gRecordFinished = false;
static volatile bool gRecordSuccess = false;
static volatile uint32_t gRecordBytes = 0;
static uint32_t gRecordStartMs = 0;

static String gRecordApiPath = "S:/audio/audio_test.wav";

static void drawState(const char *line1, const char *line2, uint32_t color = 0x000000) {
  auto &d = board.display();
  d.setBackground(0xFFFFFF);
  d.clearCanvas();
  d.textRow("audio smoke", 1, 0x000000);
  d.setFontSize(Canvas::eCNAndENFont16);
  d.textAt(line1, 10, 70, color, 26, true);
  d.textAt(line2, 10, 95, color, 26, true);
  d.textAt("A: replay last", 10, 260, 0x444444, 22, true);
  d.textAt("B: start/stop rec", 10, 285, 0x444444, 22, true);
  d.update();
}

static void recordTask(void *arg) {
  (void)arg;
  gRecordSuccess = false;
  gRecordBytes = 0;

  Status wavStart = board.storage().beginWavRecord(gRecordApiPath);
  if (!wavStart.ok()) {
    USBSerial.printf("record task: beginWavRecord failed (%s)\n", wavStart.message);
    gRecordRunning = false;
    gRecordFinished = true;
    gRecordTask = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  board.pins().write(BoardPin::AmpGain, true);

  if (xI2SMutex) {
    xSemaphoreTake(xI2SMutex, portMAX_DELAY);
  }

  uint8_t buffer[2048];
  bool writeError = false;
  while (!gRecordStopRequested) {
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytesRead, pdMS_TO_TICKS(120));
    if (bytesRead == 0) {
      continue;
    }
    Status appendSt = board.storage().appendWavRecord(buffer, bytesRead);
    if (!appendSt.ok()) {
      USBSerial.printf("record task: appendWavRecord failed (%s)\n", appendSt.message);
      writeError = true;
      break;
    }
    gRecordBytes += (uint32_t)bytesRead;
  }

  if (xI2SMutex) {
    xSemaphoreGive(xI2SMutex);
  }

  board.pins().write(BoardPin::AmpGain, false);

  Status wavEnd = board.storage().endWavRecord(!writeError);
  if (!wavEnd.ok()) {
    USBSerial.printf("record task: endWavRecord failed (%s)\n", wavEnd.message);
    writeError = true;
  }

  gRecordSuccess = !writeError;

  gRecordRunning = false;
  gRecordFinished = true;
  gRecordTask = nullptr;
  vTaskDelete(nullptr);
}

static void startRecording() {
  if (gRecordRunning || gRecordTask != nullptr) {
    return;
  }

  // Stop any active audio playback before opening microphone stream.
  (void)board.audio().stopFile();
  (void)board.audio().stopBuiltIn();

  gRecordStopRequested = false;
  gRecordFinished = false;
  gRecordSuccess = false;
  gRecordBytes = 0;
  gRecordStartMs = millis();
  gRecordRunning = true;

  BaseType_t ok = xTaskCreatePinnedToCore(recordTask, "recTask", 6 * 1024,
                                          nullptr, 5, &gRecordTask,
                                          ARDUINO_RUNNING_CORE);
  if (ok != pdPASS) {
    gRecordRunning = false;
    gRecordTask = nullptr;
    drawState("record task failed", "xTaskCreate error", 0xCC0000);
    board.led().setRgb(0, {180, 0, 0});
    return;
  }

  drawState("REC ON", "B novamente para", 0xAA0000);
  board.led().setRgb(0, {180, 0, 0});
  USBSerial.println("B -> recording started");
}

static void stopRecording() {
  if (!gRecordRunning) return;
  gRecordStopRequested = true;
  drawState("stopping...", "aguarde finalizar", 0xCC6600);
  board.led().setRgb(0, {180, 80, 0});
  USBSerial.println("B -> stop requested");
}

static void onButtonA() {
  if (gRecordRunning) {
    drawState("replay blocked", "recording in progress", 0xCC0000);
    return;
  }

  Status st = board.audio().playFile(gRecordApiPath);
  USBSerial.printf("A -> replay: code=%d, msg=%s\n", (int)st.code, st.message);
  if (!st.ok()) {
    drawState("replay failed", st.message, 0xCC0000);
    board.led().setRgb(1, {180, 0, 0});
  } else {
    drawState("replaying", "audio_test.wav", 0x006600);
    board.led().setRgb(1, {0, 160, 0});
  }
}

static void onButtonB() {
  if (!gRecordRunning) {
    startRecording();
  } else {
    stopRecording();
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
  // Keep same SD boot strategy used by camera_smoke.
  boot.initSd = true;
  boot.initAi = false;

  board.begin(boot);

  // Defaults: BMP -> S:/images, WAV -> S:/audio.
  // User may override with setAudioDirectory("S:/my_audio_folder").
  (void)board.storage().setAudioDirectory("S:/audio");
  Status storageSt = board.storage().ensureDirectories();
  if (!storageSt.ok()) {
    USBSerial.printf("storage dirs init failed: %s\n", storageSt.message);
  }
  gRecordApiPath = board.storage().audioPath("audio_test.wav");
  board.led().setBrightness(3);
  board.led().off();
  board.pins().write(BoardPin::LcdBacklight, true);

  // Warm up I2S once via AudioService initialization path.
  (void)board.audio().playBuiltIn(BA_DING, Once);

  board.input().onPress(ButtonId::A, onButtonA);
  board.input().onPress(ButtonId::B, onButtonB);

  USBSerial.println("audio_smoke boot");
  drawState("ready", "B start/stop + auto replay", 0x000000);
}

void loop() {
  if (gRecordRunning) {
    uint32_t elapsedMs = millis() - gRecordStartMs;
    if ((elapsedMs % 1000) < 40) {
      USBSerial.printf("rec running: %lus, bytes=%lu\n",
                       (unsigned long)(elapsedMs / 1000),
                       (unsigned long)gRecordBytes);
    }
  }

  if (gRecordFinished) {
    gRecordFinished = false;
    USBSerial.printf("record finished: success=%d bytes=%lu\n",
                     gRecordSuccess ? 1 : 0,
                     (unsigned long)gRecordBytes);

    if (!gRecordSuccess) {
      drawState("record failed", "file not created", 0xCC0000);
      board.led().setRgb(0, {180, 0, 0});
    } else {
      drawState("record ok", "replaying...", 0x006600);
      board.led().setRgb(0, {0, 160, 0});
      Status st = board.audio().playFile(gRecordApiPath);
      USBSerial.printf("auto replay: code=%d, msg=%s\n", (int)st.code, st.message);
      if (!st.ok()) {
        drawState("recorded, replay fail", st.message, 0xCC0000);
      }
    }
  }

  delay(50);
}
