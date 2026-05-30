#include "log.h"
#include <stdio.h>
#include <stdarg.h>

// Por padrão, envia para USBSerial se disponível, senão printf
#if defined(ARDUINO)
  #if defined(__has_include)
    #if __has_include(<USBSerial.h>)
      #include <USBSerial.h>
      #define LOG_OUTPUT_USB
    #endif
  #endif
#endif

void log_internal(const char* level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    #ifdef LOG_OUTPUT_USB
      char buf[256];
      vsnprintf(buf, sizeof(buf), fmt, args);
      USBSerial.print(level);
      USBSerial.println(buf);
    #else
      printf("%s", level);
      vprintf(fmt, args);
      printf("\n");
    #endif
    va_end(args);
}
