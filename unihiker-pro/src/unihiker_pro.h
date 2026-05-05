#pragma once

#include "layers/hal/legacy_hal.h"
#include "layers/services/services.h"

namespace unihiker_pro {

class UniHikerPro {
 public:
  UniHikerPro();
  UniHikerPro(IBoardHal &boardHal, IVisionHal &visionHal, ISpeechHal &speechHal,
              IAudioHal &audioHal);

  Status begin(const BootOptions &options = BootOptions());

  DisplayService &display();
  InputService &input();
  LedService &led();
  PinService &pins();
  SensorService &sensors();
  StorageService &storage();
  ConnectivityService &connectivity();
  CameraService &camera();
  AudioService &audio();
  VisionService &vision();
  SpeechService &speech();

 private:
  LegacyBoardHal boardHal_;
  LegacyVisionHal visionHal_;
  LegacySpeechHal speechHal_;
  LegacyAudioHal audioHal_;

  IBoardHal *boardHalRef_;
  IVisionHal *visionHalRef_;
  ISpeechHal *speechHalRef_;
  IAudioHal *audioHalRef_;

  DisplayService displayService_;
  InputService inputService_;
  LedService ledService_;
  PinService pinService_;
  SensorService sensorService_;
  StorageService storageService_;
  ConnectivityService connectivityService_;
  CameraService cameraService_;
  AudioService audioService_;
  VisionService visionService_;
  SpeechService speechService_;
};

}  // namespace unihiker_pro
