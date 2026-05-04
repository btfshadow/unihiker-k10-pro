#include "legacy_hal.h"

namespace unihiker_pro {

ePin_t LegacyBoardHal::toLegacyPin(BoardPin pin) {
  switch (pin) {
    case BoardPin::LcdBacklight:
      return eLCD_BLK;
    case BoardPin::CameraReset:
      return eCamera_rst;
    case BoardPin::ButtonB:
      return eP11_KeyB;
    case BoardPin::P12:
      return eP12;
    case BoardPin::P13:
      return eP13;
    case BoardPin::P14:
      return eP14;
    case BoardPin::P15:
      return eP15;
    case BoardPin::P2:
      return eP2;
    case BoardPin::P8:
      return eP8;
    case BoardPin::P9:
      return eP9;
    case BoardPin::P10:
      return eP10;
    case BoardPin::P6:
      return eP6;
    case BoardPin::ButtonA:
      return eP5_KeyA;
    case BoardPin::P4:
      return eP4;
    case BoardPin::P3:
      return eP3;
    case BoardPin::AmpGain:
    default:
      return eAmp_Gain;
  }
}

LegacyBoardHal::LegacyBoardHal() : board_(), aht20_(), ready_(false) {}

Status LegacyBoardHal::begin(const BootOptions &options) {
  board_.begin();

  if (options.initScreen) {
    board_.initScreen(options.screenRotation);
  }
  if (options.createCanvas) {
    board_.creatCanvas();
  }
  if (options.initCameraBackground) {
    board_.initBgCamerImage();
    board_.setBgCamerImage(true);
  }
  if (options.initSd) {
    board_.initSDFile();
  }

  ready_ = true;
  return Status::OkStatus();
}

UNIHIKER_K10 &LegacyBoardHal::board() { return board_; }

AHT20 &LegacyBoardHal::aht20() { return aht20_; }

Status LegacyBoardHal::writePin(BoardPin pin, bool level) {
  if (!ready_) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  digital_write(toLegacyPin(pin), level ? 1 : 0);
  return Status::OkStatus();
}

bool LegacyBoardHal::readPin(BoardPin pin) {
  if (!ready_) {
    return false;
  }
  return digital_read(toLegacyPin(pin)) != 0;
}

Button *LegacyBoardHal::buttonFor(ButtonId button) {
  switch (button) {
    case ButtonId::A:
      return board_.buttonA;
    case ButtonId::B:
      return board_.buttonB;
    case ButtonId::AB:
      return board_.buttonAB;
    default:
      return nullptr;
  }
}

Status LegacyBoardHal::attachButtonPress(ButtonId button, ButtonCallback callback) {
  if (!ready_) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  Button *target = buttonFor(button);
  if (target == nullptr || callback == nullptr) {
    return Status::Error(StatusCode::InvalidArgument, "invalid button or callback");
  }
  target->setPressedCallback(callback);
  return Status::OkStatus();
}

Status LegacyBoardHal::attachButtonRelease(ButtonId button, ButtonCallback callback) {
  if (!ready_) {
    return Status::Error(StatusCode::NotInitialized, "board not initialized");
  }
  Button *target = buttonFor(button);
  if (target == nullptr || callback == nullptr) {
    return Status::Error(StatusCode::InvalidArgument, "invalid button or callback");
  }
  target->setUnPressedCallback(callback);
  return Status::OkStatus();
}

bool LegacyBoardHal::isReady() const { return ready_; }

LegacyVisionHal::LegacyVisionHal() : ai_(), initialized_(false) {}

Status LegacyVisionHal::init() {
  ai_.initAi();
  initialized_ = true;
  return Status::OkStatus();
}

Status LegacyVisionHal::switchMode(AiMode mode) {
  if (!initialized_) {
    return Status::Error(StatusCode::NotInitialized, "vision not initialized");
  }

  switch (mode) {
    case AiMode::Face:
      ai_.switchAiMode(AIRecognition::Face);
      break;
    case AiMode::Cat:
      ai_.switchAiMode(AIRecognition::Cat);
      break;
    case AiMode::Move:
      ai_.switchAiMode(AIRecognition::Move);
      break;
    case AiMode::Code:
      ai_.switchAiMode(AIRecognition::Code);
      break;
    case AiMode::None:
    default:
      ai_.switchAiMode(AIRecognition::NoMode);
      break;
  }

  return Status::OkStatus();
}

AIRecognition &LegacyVisionHal::ai() { return ai_; }

ASR &LegacySpeechHal::asr() { return asr_; }

Music &LegacyAudioHal::music() { return music_; }

}  // namespace unihiker_pro
