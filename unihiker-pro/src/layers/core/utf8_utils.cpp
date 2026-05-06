#include "utf8_utils.h"

namespace unihiker_pro {
namespace utf8 {

static bool isUtf8ContinuationByte(uint8_t b) {
  return (b & 0xC0) == 0x80;
}

static size_t utf8SequenceLength(uint8_t b0) {
  if ((b0 & 0x80) == 0) return 1;
  if ((b0 & 0xE0) == 0xC0) return 2;
  if ((b0 & 0xF0) == 0xE0) return 3;
  if ((b0 & 0xF8) == 0xF0) return 4;
  return 0;
}

static bool hasOnlyAscii(const String &input) {
  for (size_t i = 0; i < input.length(); ++i) {
    if (((uint8_t)input[i]) & 0x80) {
      return false;
    }
  }
  return true;
}

bool isAscii(const String &input) {
  return hasOnlyAscii(input);
}

static bool mapLatin1Accent(uint8_t b1, char *out) {
  if (out == nullptr) return false;
  switch (b1) {
    // Lowercase accents.
    case 0xA1: case 0xA0: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
      *out = 'a';
      return true;
    case 0xA7:
      *out = 'c';
      return true;
    case 0xA9: case 0xA8: case 0xAA: case 0xAB:
      *out = 'e';
      return true;
    case 0xAD: case 0xAC: case 0xAE: case 0xAF:
      *out = 'i';
      return true;
    case 0xB1:
      *out = 'n';
      return true;
    case 0xB3: case 0xB2: case 0xB4: case 0xB5: case 0xB6:
      *out = 'o';
      return true;
    case 0xBA: case 0xB9: case 0xBB: case 0xBC:
      *out = 'u';
      return true;

    // Uppercase accents.
    case 0x81: case 0x80: case 0x82: case 0x83: case 0x84: case 0x85:
      *out = 'A';
      return true;
    case 0x87:
      *out = 'C';
      return true;
    case 0x89: case 0x88: case 0x8A: case 0x8B:
      *out = 'E';
      return true;
    case 0x8D: case 0x8C: case 0x8E: case 0x8F:
      *out = 'I';
      return true;
    case 0x91:
      *out = 'N';
      return true;
    case 0x93: case 0x92: case 0x94: case 0x95: case 0x96:
      *out = 'O';
      return true;
    case 0x9A: case 0x99: case 0x9B: case 0x9C:
      *out = 'U';
      return true;
    default:
      return false;
  }
}

String sanitize(const String &input) {
  if (hasOnlyAscii(input)) {
    return input;
  }

  String out;
  out.reserve(input.length());

  size_t i = 0;
  while (i < input.length()) {
    uint8_t b0 = (uint8_t)input[i];
    size_t expected = utf8SequenceLength(b0);

    if (expected == 1) {
      out += (char)b0;
      ++i;
      continue;
    }

    if (expected == 0 || i + expected > input.length()) {
      out += '?';
      ++i;
      continue;
    }

    bool ok = true;
    for (size_t k = 1; k < expected; ++k) {
      if (!isUtf8ContinuationByte((uint8_t)input[i + k])) {
        ok = false;
        break;
      }
    }

    if (!ok) {
      out += '?';
      ++i;
      continue;
    }

    for (size_t k = 0; k < expected; ++k) {
      out += input[i + k];
    }
    i += expected;
  }

  return out;
}

String clipCodepoints(const String &input, size_t maxCodepoints) {
  String safe = sanitize(input);
  String out;
  out.reserve(safe.length());

  size_t i = 0;
  size_t cp = 0;
  while (i < safe.length() && cp < maxCodepoints) {
    uint8_t b0 = (uint8_t)safe[i];
    size_t n = utf8SequenceLength(b0);
    if (n == 0 || i + n > safe.length()) {
      break;
    }
    for (size_t k = 0; k < n; ++k) {
      out += safe[i + k];
    }
    i += n;
    cp++;
  }

  return out;
}

void latinDisplayFallbackTo(const String &input,
                           String *out,
                           bool *outHasNonAscii,
                           bool skipAsciiFastPath) {
  if (out == nullptr) return;

  if (!skipAsciiFastPath && hasOnlyAscii(input)) {
    *out = input;
    if (outHasNonAscii != nullptr) {
      *outHasNonAscii = false;
    }
    return;
  }

  out->remove(0);
  out->reserve(input.length());
  bool hasNonAscii = false;

  size_t i = 0;
  while (i < input.length()) {
    uint8_t b0 = (uint8_t)input[i];
    size_t expected = utf8SequenceLength(b0);

    if (expected == 1) {
      *out += (char)b0;
      ++i;
      continue;
    }

    if (expected == 0 || i + expected > input.length()) {
      *out += '?';
      ++i;
      continue;
    }

    bool ok = true;
    for (size_t k = 1; k < expected; ++k) {
      if (!isUtf8ContinuationByte((uint8_t)input[i + k])) {
        ok = false;
        break;
      }
    }

    if (!ok) {
      *out += '?';
      ++i;
      continue;
    }

    if (expected == 2 && b0 == 0xC3) {
      char mapped = '\0';
      if (mapLatin1Accent((uint8_t)input[i + 1], &mapped)) {
        *out += mapped;
        i += 2;
        continue;
      }
    }

    hasNonAscii = true;
    for (size_t k = 0; k < expected; ++k) {
      *out += input[i + k];
    }
    i += expected;
  }

  if (outHasNonAscii != nullptr) {
    *outHasNonAscii = hasNonAscii;
  }
}

String latinDisplayFallback(const String &input) {
  String out;
  latinDisplayFallbackTo(input, &out, nullptr, false);
  return out;
}

}  // namespace utf8
}  // namespace unihiker_pro
