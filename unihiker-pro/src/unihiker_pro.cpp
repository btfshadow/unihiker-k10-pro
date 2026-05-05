#include "unihiker_pro.h"

namespace unihiker_pro {

UniHikerPro::UniHikerPro()
    : boardHal_(),
      visionHal_(),
      speechHal_(),
      audioHal_(),
      boardHalRef_(&boardHal_),
      visionHalRef_(&visionHal_),
      speechHalRef_(&speechHal_),
      audioHalRef_(&audioHal_),
      displayService_(*boardHalRef_),
      inputService_(*boardHalRef_),
      ledService_(*boardHalRef_),
      pinService_(*boardHalRef_),
      sensorService_(*boardHalRef_),
      storageService_(*boardHalRef_),
      connectivityService_(),
      cameraService_(*boardHalRef_, &inputService_),
      audioService_(*boardHalRef_, *audioHalRef_),
      visionService_(*boardHalRef_, *visionHalRef_,
             &cameraService_, &storageService_, &displayService_),
      speechService_(*speechHalRef_) {}

UniHikerPro::UniHikerPro(IBoardHal &boardHal, IVisionHal &visionHal,
                         ISpeechHal &speechHal, IAudioHal &audioHal)
    : boardHal_(),
      visionHal_(),
      speechHal_(),
      audioHal_(),
      boardHalRef_(&boardHal),
      visionHalRef_(&visionHal),
      speechHalRef_(&speechHal),
      audioHalRef_(&audioHal),
      displayService_(*boardHalRef_),
      inputService_(*boardHalRef_),
      ledService_(*boardHalRef_),
      pinService_(*boardHalRef_),
      sensorService_(*boardHalRef_),
      storageService_(*boardHalRef_),
      connectivityService_(),
      cameraService_(*boardHalRef_, &inputService_),
      audioService_(*boardHalRef_, *audioHalRef_),
      visionService_(*boardHalRef_, *visionHalRef_,
             &cameraService_, &storageService_, &displayService_),
      speechService_(*speechHalRef_) {}

Status UniHikerPro::begin(const BootOptions &options) {
  auto status = boardHalRef_->begin(options);
  if (!status.ok()) {
    return status;
  }

  if (options.initAi) {
    status = visionService_.init();
    if (!status.ok()) {
      return status;
    }
  }

  return Status::OkStatus();
}

DisplayService &UniHikerPro::display() { return displayService_; }

InputService &UniHikerPro::input() { return inputService_; }

LedService &UniHikerPro::led() { return ledService_; }

PinService &UniHikerPro::pins() { return pinService_; }

SensorService &UniHikerPro::sensors() { return sensorService_; }

StorageService &UniHikerPro::storage() { return storageService_; }

ConnectivityService &UniHikerPro::connectivity() { return connectivityService_; }

CameraService &UniHikerPro::camera() { return cameraService_; }

AudioService &UniHikerPro::audio() { return audioService_; }

VisionService &UniHikerPro::vision() { return visionService_; }

SpeechService &UniHikerPro::speech() { return speechService_; }

}  // namespace unihiker_pro
