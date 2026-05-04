#include <Arduino.h>
#include <Wire.h>
#include <esp_err.h>
#include <initBoard.h>

namespace {

constexpr uint8_t kIoExpanderAddress = 0x20;
constexpr uint8_t kRegInputPort0 = 0x00;
constexpr uint8_t kRegInputPort1 = 0x01;
constexpr uint8_t kRegOutputPort0 = 0x02;
constexpr uint8_t kRegOutputPort1 = 0x03;
constexpr uint8_t kRegConfigPort0 = 0x06;
constexpr uint8_t kRegConfigPort1 = 0x07;

constexpr uint8_t kButtonBMask = 1u << 2;
constexpr uint8_t kButtonAMask = 1u << 4;

bool g_initialized = false;
uint8_t g_outputPort0 = 0x00;
uint8_t g_outputPort1 = 0x00;
uint8_t g_configPort0 = kButtonBMask;
uint8_t g_configPort1 = kButtonAMask;

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kIoExpanderAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(kIoExpanderAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(kIoExpanderAddress, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  value = Wire.read();
  return true;
}

uint8_t pinToBit(ePin_t pin) {
  return static_cast<uint8_t>(pin) & 0x07;
}

bool isPort1(ePin_t pin) {
  return static_cast<uint8_t>(pin) >= 8;
}

uint8_t pinMask(ePin_t pin) {
  return static_cast<uint8_t>(1u << pinToBit(pin));
}

void syncConfig() {
  writeRegister(kRegConfigPort0, g_configPort0);
  writeRegister(kRegConfigPort1, g_configPort1);
}

void syncOutput() {
  writeRegister(kRegOutputPort0, g_outputPort0);
  writeRegister(kRegOutputPort1, g_outputPort1);
}

}  // namespace

extern "C" esp_err_t __wrap_init_board(void) {
  if (!g_initialized) {
    Wire.begin(47, 48);
    delay(5);

    g_outputPort0 = 0x00;
    g_outputPort1 = 0x00;
    g_configPort0 = kButtonBMask;
    g_configPort1 = kButtonAMask;

    syncOutput();
    syncConfig();

    g_initialized = true;
  }

  return ESP_OK;
}

extern "C" void __wrap_digital_write(ePin_t pin, uint8_t state) {
  if (!g_initialized) {
    __wrap_init_board();
  }

  const uint8_t mask = pinMask(pin);
  if (isPort1(pin)) {
    g_configPort1 &= static_cast<uint8_t>(~mask);
    if (state) {
      g_outputPort1 |= mask;
    } else {
      g_outputPort1 &= static_cast<uint8_t>(~mask);
    }
  } else {
    g_configPort0 &= static_cast<uint8_t>(~mask);
    if (state) {
      g_outputPort0 |= mask;
    } else {
      g_outputPort0 &= static_cast<uint8_t>(~mask);
    }
  }

  syncConfig();
  syncOutput();
}

extern "C" uint8_t __wrap_digital_read(ePin_t pin) {
  if (!g_initialized) {
    __wrap_init_board();
  }

  const uint8_t mask = pinMask(pin);

  if (isPort1(pin)) {
    uint8_t value = 0;
    if ((g_configPort1 & mask) != 0) {
      if (readRegister(kRegInputPort1, value)) {
        return (value & mask) ? 1 : 0;
      }
      return 1;
    }
    return (g_outputPort1 & mask) ? 1 : 0;
  }

  uint8_t value = 0;
  if ((g_configPort0 & mask) != 0) {
    if (readRegister(kRegInputPort0, value)) {
      return (value & mask) ? 1 : 0;
    }
    return 1;
  }
  return (g_outputPort0 & mask) ? 1 : 0;
}