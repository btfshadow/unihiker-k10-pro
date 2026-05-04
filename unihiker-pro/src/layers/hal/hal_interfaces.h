#pragma once

#include "../core/foundation_types.h"

#include <stdint.h>

class UNIHIKER_K10;
class AHT20;
class AIRecognition;
class ASR;
class Music;

namespace unihiker_pro {

class IBoardHal {
 public:
  virtual ~IBoardHal() {}
  virtual Status begin(const BootOptions &options) = 0;
  virtual UNIHIKER_K10 &board() = 0;
  virtual AHT20 &aht20() = 0;
  virtual Status writePin(BoardPin pin, bool level) = 0;
  virtual bool readPin(BoardPin pin) = 0;
  virtual Status attachButtonPress(ButtonId button, ButtonCallback callback) = 0;
  virtual Status attachButtonRelease(ButtonId button, ButtonCallback callback) = 0;
  virtual bool isReady() const = 0;
};

class IVisionHal {
 public:
  virtual ~IVisionHal() {}
  virtual Status init() = 0;
  virtual Status switchMode(AiMode mode) = 0;
  virtual AIRecognition &ai() = 0;
};

class ISpeechHal {
 public:
  virtual ~ISpeechHal() {}
  virtual ASR &asr() = 0;
};

class IAudioHal {
 public:
  virtual ~IAudioHal() {}
  virtual Music &music() = 0;
};

}  // namespace unihiker_pro
