#pragma once

#include <stdint.h>

namespace unihiker_pro {

enum class StatusCode {
  Ok = 0,
  NotInitialized,
  InvalidArgument,
  NotSupported,
  IOError,
  Busy,
};

struct Status {
  StatusCode code;
  const char *message;

  bool ok() const { return code == StatusCode::Ok; }

  static Status OkStatus() { return {StatusCode::Ok, "ok"}; }
  static Status Error(StatusCode c, const char *m) { return {c, m}; }
};

struct BootOptions {
  bool initScreen = true;
  int screenRotation = 2;
  bool createCanvas = true;
  bool initCameraBackground = false;
  bool initSd = false;
  bool initAi = false;
};

enum class AiMode {
  None = 0,
  Face,
  FaceRecognize,
  FaceEnroll,
  FaceDeleteAll,
  Cat,
  Move,
  Code,
  Ocr,
};

enum class ButtonId {
  A = 0,
  B,
  AB,
};

enum class BoardPin {
  LcdBacklight = 0,
  CameraReset,
  ButtonB,
  P12,
  P13,
  P14,
  P15,
  P2,
  P8,
  P9,
  P10,
  P6,
  ButtonA,
  P4,
  P3,
  AmpGain,
};

struct RgbColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

using ButtonCallback = void (*)();

}  // namespace unihiker_pro