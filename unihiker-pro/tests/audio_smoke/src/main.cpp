#include <Arduino.h>
#include <driver/i2s.h>
#include <unihiker_pro.h>

using namespace unihiker_pro;

extern SemaphoreHandle_t xSPIlMutex;

UniHikerPro board;
static TaskHandle_t gRecordTask = nullptr;
static volatile bool gRecordRunning = false;
static volatile bool gRecordStopRequested = false;
static volatile bool gRecordFinished = false;
static volatile bool gRecordSuccess = false;
static volatile uint32_t gRecordBytes = 0;
static uint32_t gRecordStartMs = 0;

static String gRecordApiPath = "S:/audio/audio_test.wav";

static void fillWavHeader(uint8_t *header, uint32_t dataSize,
                          uint32_t sampleRate = 16000,
                          uint16_t channels = 2,
                          uint16_t bitsPerSample = 16) {
  uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
  uint16_t blockAlign = channels * (bitsPerSample / 8);
  uint32_t riffSize = dataSize + 36;

  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  header[4] = (uint8_t)(riffSize & 0xFF);
  header[5] = (uint8_t)((riffSize >> 8) & 0xFF);
  header[6] = (uint8_t)((riffSize >> 16) & 0xFF);
  header[7] = (uint8_t)((riffSize >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 16;
  header[17] = 0;
  header[18] = 0;
  header[19] = 0;
  header[20] = 1;
  header[21] = 0;
  header[22] = (uint8_t)(channels & 0xFF);
  header[23] = (uint8_t)((channels >> 8) & 0xFF);
  header[24] = (uint8_t)(sampleRate & 0xFF);
  header[25] = (uint8_t)((sampleRate >> 8) & 0xFF);
  header[26] = (uint8_t)((sampleRate >> 16) & 0xFF);
  header[27] = (uint8_t)((sampleRate >> 24) & 0xFF);
  header[28] = (uint8_t)(byteRate & 0xFF);
  header[29] = (uint8_t)((byteRate >> 8) & 0xFF);
  header[30] = (uint8_t)((byteRate >> 16) & 0xFF);
  header[31] = (uint8_t)((byteRate >> 24) & 0xFF);
  header[32] = (uint8_t)(blockAlign & 0xFF);
  header[33] = (uint8_t)((blockAlign >> 8) & 0xFF);
  header[34] = (uint8_t)(bitsPerSample & 0xFF);
  header[35] = (uint8_t)((bitsPerSample >> 8) & 0xFF);
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (uint8_t)(dataSize & 0xFF);
  header[41] = (uint8_t)((dataSize >> 8) & 0xFF);
  header[42] = (uint8_t)((dataSize >> 16) & 0xFF);
  header[43] = (uint8_t)((dataSize >> 24) & 0xFF);
}

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

  lv_fs_file_t file;
  lv_fs_res_t ret;
  if (xSPIlMutex) {
    xSemaphoreTake(xSPIlMutex, portMAX_DELAY);
  }
  ret = lv_fs_open(&file, gRecordApiPath.c_str(), LV_FS_MODE_WR);
  if (xSPIlMutex) {
    xSemaphoreGive(xSPIlMutex);
  }
  if (ret != LV_FS_RES_OK) {
    USBSerial.println("record task: lv_fs_open write failed");
    gRecordRunning = false;
    gRecordFinished = true;
    gRecordTask = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  uint8_t wavHeader[44];
  fillWavHeader(wavHeader, 0);
  uint32_t written = 0;
  if (xSPIlMutex) {
    xSemaphoreTake(xSPIlMutex, portMAX_DELAY);
  }
  ret = lv_fs_write(&file, wavHeader, sizeof(wavHeader), &written);
  if (xSPIlMutex) {
    xSemaphoreGive(xSPIlMutex);
  }
  if (ret != LV_FS_RES_OK || written != sizeof(wavHeader)) {
    USBSerial.println("record task: write wav header failed");
    if (xSPIlMutex) {
      xSemaphoreTake(xSPIlMutex, portMAX_DELAY);
    }
    lv_fs_close(&file);
    if (xSPIlMutex) {
      xSemaphoreGive(xSPIlMutex);
    }
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
    written = 0;
    if (xSPIlMutex) {
      xSemaphoreTake(xSPIlMutex, portMAX_DELAY);
    }
    ret = lv_fs_write(&file, buffer, (uint32_t)bytesRead, &written);
    if (xSPIlMutex) {
      xSemaphoreGive(xSPIlMutex);
    }
    if (ret != LV_FS_RES_OK || written != bytesRead) {
      USBSerial.println("record task: write chunk failed");
      writeError = true;
      break;
    }
    gRecordBytes += (uint32_t)bytesRead;
  }

  if (xI2SMutex) {
    xSemaphoreGive(xI2SMutex);
  }

  board.pins().write(BoardPin::AmpGain, false);

  if (!writeError) {
    fillWavHeader(wavHeader, gRecordBytes);
    if (xSPIlMutex) {
      xSemaphoreTake(xSPIlMutex, portMAX_DELAY);
    }
    ret = lv_fs_seek(&file, 0, LV_FS_SEEK_SET);
    if (ret == LV_FS_RES_OK) {
      written = 0;
      ret = lv_fs_write(&file, wavHeader, sizeof(wavHeader), &written);
      if (ret != LV_FS_RES_OK || written != sizeof(wavHeader)) {
        writeError = true;
      }
    } else {
      writeError = true;
    }
    if (xSPIlMutex) {
      xSemaphoreGive(xSPIlMutex);
    }
  }

  if (xSPIlMutex) {
    xSemaphoreTake(xSPIlMutex, portMAX_DELAY);
  }
  lv_fs_close(&file);
  if (xSPIlMutex) {
    xSemaphoreGive(xSPIlMutex);
  }

  uint32_t verifySize = 0;
  if (!writeError) {
    lv_fs_file_t verify;
    if (xSPIlMutex) {
      xSemaphoreTake(xSPIlMutex, portMAX_DELAY);
    }
    ret = lv_fs_open(&verify, gRecordApiPath.c_str(), LV_FS_MODE_RD);
    if (ret == LV_FS_RES_OK) {
      (void)lv_fs_seek(&verify, 0, LV_FS_SEEK_END);
      (void)lv_fs_tell(&verify, &verifySize);
      lv_fs_close(&verify);
    }
    if (xSPIlMutex) {
      xSemaphoreGive(xSPIlMutex);
    }
  }

  gRecordSuccess = !writeError && verifySize > 44;

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
