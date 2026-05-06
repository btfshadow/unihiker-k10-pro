#pragma once

#include <Arduino.h>

namespace unihiker_pro {
namespace utf8 {

bool isAscii(const String &input);
String sanitize(const String &input);
String clipCodepoints(const String &input, size_t maxCodepoints);
void latinDisplayFallbackTo(const String &input,
						   String *out,
						   bool *outHasNonAscii = nullptr,
						   bool skipAsciiFastPath = false);
String latinDisplayFallback(const String &input);

}  // namespace utf8
}  // namespace unihiker_pro
