#pragma once
#include <functional>
#include <Arduino.h>
#include "layers/services/services.h"

namespace unihiker_pro {


struct ButtonAction {
  std::function<void()> fn;
  String label;
  bool isLong;
  ButtonAction() : fn(nullptr), label(""), isLong(false) {}
  ButtonAction(std::function<void()> f, const String& l, bool il) : fn(f), label(l), isLong(il) {}
};

struct ButtonControlTheme {
  uint32_t normalColor = 0x444444;
  uint32_t longColor = 0x0077cc;
  uint32_t bgColor = 0xffffff;
  Canvas::eFontSize_t font = Canvas::eCNAndENFont16;
  uint16_t y = 274;
};

struct ButtonControlMapping {
  ButtonAction a_fast, a_long, b_fast, b_long, ab_long;
};

void button_control(
  InputService& input,
  DisplayService& display,
  const ButtonControlMapping& mapping,
  const ButtonControlTheme& theme = ButtonControlTheme()
);

} // namespace unihiker_pro
