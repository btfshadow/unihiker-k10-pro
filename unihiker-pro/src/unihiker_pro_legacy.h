#pragma once

#include "unihiker_pro.h"

namespace unihiker_pro {

using CBFunc = ButtonCallback;

class LegacyCanvasProxy {
 public:
  typedef enum {
    eCNAndENFont24,
    eJapanFont24x24,
    eKoreanFont24x24,
    eGreeceFont12x24,
    eCNAndENFont16,
  } eFontSize_t;

  explicit LegacyCanvasProxy(UniHikerPro &sdk) : sdk_(sdk) {}

  void updateCanvas() { (void)sdk_.display().update(); }
  void canvasClear() { (void)sdk_.display().clearCanvas(); }
  void canvasClear(uint8_t row) { (void)sdk_.display().clearRow(row); }
  void clearLocalCanvas(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    (void)sdk_.display().clearRegion(x, y, w, h);
  }

  void canvasText(const char *text, uint8_t row, uint32_t color) {
    (void)sdk_.display().textRow(text ? String(text) : String(""), row, color);
  }
  void canvasText(String text, uint8_t row, uint32_t color) {
    (void)sdk_.display().textRow(text, row, color);
  }
  void canvasText(float text, uint8_t row, uint32_t color) {
    (void)sdk_.display().textRow(String(text), row, color);
  }

  void canvasText(const char *text,
                  int16_t x,
                  int16_t y,
                  uint32_t color,
                  eFontSize_t font,
                  int count,
                  bool autoClean) {
    (void)sdk_.display().setFontSize(mapFont(font));
    (void)sdk_.display().textAt(text ? String(text) : String(""),
                                x,
                                y,
                                color,
                                count,
                                autoClean);
  }
  void canvasText(String text,
                  int16_t x,
                  int16_t y,
                  uint32_t color,
                  eFontSize_t font,
                  int count,
                  bool autoClean) {
    (void)sdk_.display().setFontSize(mapFont(font));
    (void)sdk_.display().textAt(text, x, y, color, count, autoClean);
  }
  void canvasText(float text,
                  int16_t x,
                  int16_t y,
                  uint32_t color,
                  eFontSize_t font,
                  int count,
                  bool autoClean) {
    (void)sdk_.display().setFontSize(mapFont(font));
    (void)sdk_.display().textAt(String(text), x, y, color, count, autoClean);
  }

  void canvasPoint(int16_t x, int16_t y, uint32_t color) {
    (void)sdk_.display().drawPoint(x, y, color);
  }
  void canvasSetLineWidth(uint8_t w = 10) { (void)sdk_.display().setLineWidth(w); }
  void canvasLine(int x1, int y1, int x2, int y2, uint32_t color) {
    (void)sdk_.display().drawLine(x1, y1, x2, y2, color);
  }
  void canvasCircle(int x, int y, int r, uint32_t color, uint32_t bgColor, bool fill) {
    (void)sdk_.display().drawCircle(x, y, r, color, bgColor, fill);
  }
  void canvasRectangle(int x,
                       int y,
                       int w,
                       int h,
                       uint32_t color,
                       uint32_t bgColor,
                       bool fill) {
    (void)sdk_.display().drawRect(x, y, w, h, color, bgColor, fill);
  }

  void canvasDrawBitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *bitmap) {
    (void)sdk_.display().drawBitmap(x, y, w, h, bitmap);
  }
  void canvasDrawImage(int16_t x, int16_t y, const void *path) {
    if (path == nullptr) return;
    (void)sdk_.display().drawImage(x, y, String((const char *)path));
  }
  void canvasDrawImage(int16_t x, int16_t y, String imagePath) {
    (void)sdk_.display().drawImage(x, y, imagePath);
  }

 private:
  ::Canvas::eFontSize_t mapFont(eFontSize_t font) {
    switch (font) {
      case eCNAndENFont16:
        return ::Canvas::eCNAndENFont16;
      case eJapanFont24x24:
        return ::Canvas::eJapanFont24x24;
      case eKoreanFont24x24:
        return ::Canvas::eKoreanFont24x24;
      case eGreeceFont12x24:
        return ::Canvas::eGreeceFont12x24;
      case eCNAndENFont24:
      default:
        return ::Canvas::eCNAndENFont24;
    }
  }

  UniHikerPro &sdk_;
};

class LegacyRgbProxy {
 public:
  explicit LegacyRgbProxy(UniHikerPro &sdk) : sdk_(sdk), brightness_(3) {}

  void write(int8_t index, uint8_t r, uint8_t g, uint8_t b) {
    (void)sdk_.led().setRgb(index, {r, g, b});
  }
  void write(int8_t index, uint32_t color) {
    write(index,
          (uint8_t)((color >> 16) & 0xFF),
          (uint8_t)((color >> 8) & 0xFF),
          (uint8_t)(color & 0xFF));
  }
  void brightness(uint8_t b) {
    brightness_ = b;
    (void)sdk_.led().setBrightness(b);
  }
  uint8_t brightness() const { return brightness_; }

 private:
  UniHikerPro &sdk_;
  uint8_t brightness_;
};

class LegacyButtonProxy {
 public:
  LegacyButtonProxy(UniHikerPro &sdk, ButtonId id) : sdk_(sdk), id_(id) {}

  bool isPressed() { return sdk_.input().pressed(id_); }

  void setPressedCallback(CBFunc callback) {
    (void)sdk_.input().onPress(id_, callback);
  }

  void setUnPressedCallback(CBFunc callback) {
    (void)sdk_.input().onRelease(id_, callback);
  }

 private:
  UniHikerPro &sdk_;
  ButtonId id_;
};

class UniHikerProLegacyShim {
 public:
  UniHikerProLegacyShim()
      : sdk_(),
        canvasProxy_(sdk_),
        rgbProxy_(sdk_),
        buttonAProxy_(sdk_, ButtonId::A),
        buttonBProxy_(sdk_, ButtonId::B),
        buttonABProxy_(sdk_, ButtonId::AB),
        canvas(&canvasProxy_),
        rgb(&rgbProxy_),
        buttonA(&buttonAProxy_),
        buttonB(&buttonBProxy_),
        buttonAB(&buttonABProxy_) {}

  LegacyCanvasProxy *canvas;
  LegacyRgbProxy *rgb;
  LegacyButtonProxy *buttonA;
  LegacyButtonProxy *buttonB;
  LegacyButtonProxy *buttonAB;

  void begin() {
    BootOptions boot;
    boot.initScreen = false;
    boot.createCanvas = false;
    boot.initCameraBackground = false;
    boot.initSd = false;
    boot.initAi = false;
    (void)sdk_.begin(boot);
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
    (void)sdk_.begin(boot);
  }

  void creatCanvas() { (void)sdk_.display().createCanvas(); }

  void setScreenBackground(uint32_t color) { (void)sdk_.display().setBackground(color); }

  void initBgCamerImage() {
    BootOptions boot;
    boot.initScreen = true;
    boot.createCanvas = true;
    boot.initCameraBackground = true;
    boot.initSd = false;
    boot.initAi = false;
    (void)sdk_.begin(boot);
  }

  void setBgCamerImage(bool enabled = true) { (void)sdk_.display().setCameraBackground(enabled); }

  void initSDFile() { (void)sdk_.storage().initSd(); }

  void photoSaveToTFCard(const char *imagePath) {
    if (imagePath == nullptr) return;
    (void)sdk_.camera().capture(String(imagePath));
  }
  void photoSaveToTFCard(String imagePath) { (void)sdk_.camera().capture(imagePath); }

  bool buttonAState() { return sdk_.input().buttonAPressed(); }
  bool buttonBState() { return sdk_.input().buttonBPressed(); }
  bool buttonABState() { return sdk_.input().buttonABPressed(); }

  void setRgb(int8_t index, uint8_t r, uint8_t g, uint8_t b) {
    (void)sdk_.led().setRgb(index, {r, g, b});
  }

  uint16_t readALS() { return sdk_.sensors().ambientLux(); }
  uint64_t readMICData() { return sdk_.sensors().micLevel(); }
  int getAccelerometerX() { return sdk_.sensors().accelX(); }
  int getAccelerometerY() { return sdk_.sensors().accelY(); }
  int getAccelerometerZ() { return sdk_.sensors().accelZ(); }

  UniHikerPro &sdk() { return sdk_; }

 private:
  UniHikerPro sdk_;
  LegacyCanvasProxy canvasProxy_;
  LegacyRgbProxy rgbProxy_;
  LegacyButtonProxy buttonAProxy_;
  LegacyButtonProxy buttonBProxy_;
  LegacyButtonProxy buttonABProxy_;
};

}  // namespace unihiker_pro
