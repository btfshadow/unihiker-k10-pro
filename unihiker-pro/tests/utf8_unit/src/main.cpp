#include <Arduino.h>
#include "layers/core/utf8_utils.h"

using namespace unihiker_pro;

static uint32_t gPass = 0;
static uint32_t gFail = 0;

static void expectEq(const char *name, const String &expected, const String &actual) {
  if (expected == actual) {
    gPass++;
    USBSerial.printf("[PASS] %s\n", name);
    return;
  }

  gFail++;
  USBSerial.printf("[FAIL] %s\n", name);
  USBSerial.printf("  expected: %s\n", expected.c_str());
  USBSerial.printf("  actual  : %s\n", actual.c_str());
}

static void testAsciiPreserved() {
  String input = "simple text 123";
  expectEq("ascii preserved", input, utf8::sanitize(input));
}

static void testAccentsPreserved() {
  String input = "ação seção coração maçã açúcar canção";
  expectEq("accent preserved", input, utf8::sanitize(input));
}

static void testCedillaUpperLowerPreserved() {
  String input = "Ç ç";
  expectEq("cedilla upper/lower preserved", input, utf8::sanitize(input));
}

static void testInvalidContinuationSanitized() {
  String input = "abc ";
  input += (char)0xC3;
  input += 'X';
  expectEq("invalid continuation", "abc ?X", utf8::sanitize(input));
}

static void testTruncatedSequenceSanitized() {
  String input = "fim ";
  input += (char)0xE2;
  input += (char)0x82;
  expectEq("truncated sequence", "fim ??", utf8::sanitize(input));
}

static void testInvalidLeadingByteSanitized() {
  String input;
  input += (char)0x80;
  input += 'a';
  expectEq("invalid leading byte", "?a", utf8::sanitize(input));
}

static void testClipCodepointsAccents() {
  String input = "ação";
  expectEq("clip accents cp=3", "açã", utf8::clipCodepoints(input, 3));
}

static void testClipCodepointsCedilla() {
  String input = "açúcar";
  expectEq("clip cedilla cp=2", "aç", utf8::clipCodepoints(input, 2));
}

static void testClipEmptyWhenZero() {
  String input = "ação";
  expectEq("clip cp=0", "", utf8::clipCodepoints(input, 0));
}

static void testLatinDisplayFallbackPortuguese() {
  String input = "ação seção coração maçã açúcar canção";
  expectEq("latin fallback ptbr",
           "acao secao coracao maca acucar cancao",
           utf8::latinDisplayFallback(input));
}

static void testLatinDisplayFallbackCedilla() {
  String input = "Ç ç";
  expectEq("latin fallback cedilla",
           "C c",
           utf8::latinDisplayFallback(input));
}

static void runAllUtf8UnitTests() {
  USBSerial.println("=== utf8 unit tests ===");
  testAsciiPreserved();
  testAccentsPreserved();
  testCedillaUpperLowerPreserved();
  testInvalidContinuationSanitized();
  testTruncatedSequenceSanitized();
  testInvalidLeadingByteSanitized();
  testClipCodepointsAccents();
  testClipCodepointsCedilla();
  testClipEmptyWhenZero();
  testLatinDisplayFallbackPortuguese();
  testLatinDisplayFallbackCedilla();

  USBSerial.printf("utf8 unit summary: pass=%lu fail=%lu\n",
                   (unsigned long)gPass,
                   (unsigned long)gFail);
}

void setup() {
  USBSerial.begin(115200);
  unsigned long t0 = millis();
  while (!USBSerial && millis() - t0 < 8000) {
    delay(10);
  }

  runAllUtf8UnitTests();
}

void loop() {
  delay(250);
}
