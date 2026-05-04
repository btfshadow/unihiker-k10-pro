#pragma once

#include "unihiker_pro.h"

namespace unihiker_pro {

class UniHikerProLegacyShim {
 public:
  UniHikerProLegacyShim() : sdk_() {}

  void begin() {
    BootOptions boot;
    boot.initScreen = false;
    boot.createCanvas = false;
    sdk_.begin(boot);
  }

  void initScreen(int dir = 2, int frame = 0) {
    (void)frame;
    BootOptions boot;
    boot.initScreen = true;
    boot.screenRotation = dir;
    boot.createCanvas = true;
    boot.initCameraBackground = false;
    boot.initSd = false;
    boot.initAi = false;
    sdk_.begin(boot);
  }

  void setScreenBackground(uint32_t color) { sdk_.display().setBackground(color); }

  void initBgCamerImage() {
    BootOptions boot;
    boot.initScreen = true;
    boot.createCanvas = true;
    boot.initCameraBackground = true;
    sdk_.begin(boot);
  }

  bool buttonA() { return sdk_.input().buttonAPressed(); }
  bool buttonB() { return sdk_.input().buttonBPressed(); }

  void setRgb(int8_t index, uint8_t r, uint8_t g, uint8_t b) {
    sdk_.led().setRgb(index, {r, g, b});
  }

  uint16_t readALS() { return sdk_.sensors().ambientLux(); }

  UniHikerPro &sdk() { return sdk_; }

 private:
  UniHikerPro sdk_;
};

}  // namespace unihiker_pro
