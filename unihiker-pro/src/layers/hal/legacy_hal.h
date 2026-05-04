#pragma once

#include "hal_interfaces.h"

#include <AIRecognition.h>
#include <asr.h>
#include <initBoard.h>
#include <unihiker_k10.h>

namespace unihiker_pro {

class LegacyBoardHal : public IBoardHal {
 public:
  LegacyBoardHal();

  Status begin(const BootOptions &options) override;
  UNIHIKER_K10 &board() override;
  AHT20 &aht20() override;
  Status writePin(BoardPin pin, bool level) override;
  bool readPin(BoardPin pin) override;
  Status attachButtonPress(ButtonId button, ButtonCallback callback) override;
  Status attachButtonRelease(ButtonId button, ButtonCallback callback) override;
  bool isReady() const override;

 private:
  static ePin_t toLegacyPin(BoardPin pin);
  Button *buttonFor(ButtonId button);

  UNIHIKER_K10 board_;
  AHT20 aht20_;
  bool ready_;
};

class LegacyVisionHal : public IVisionHal {
 public:
  LegacyVisionHal();

  Status init() override;
  Status switchMode(AiMode mode) override;
  AIRecognition &ai() override;

 private:
  AIRecognition ai_;
  bool initialized_;
};

class LegacySpeechHal : public ISpeechHal {
 public:
  ASR &asr() override;

 private:
  ASR asr_;
};

class LegacyAudioHal : public IAudioHal {
 public:
  Music &music() override;

 private:
  Music music_;
};

}  // namespace unihiker_pro
