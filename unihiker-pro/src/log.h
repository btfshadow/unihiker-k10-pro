#pragma once

// Níveis de log
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

// Defina LOG_LEVEL_DEFAULT se não definido externamente
#ifndef LOG_LEVEL_DEFAULT
#define LOG_LEVEL_DEFAULT LOG_LEVEL_INFO
#endif

// Macro para selecionar nível de log em build (pode ser sobrescrito via -DLOG_LEVEL_DEFAULT=...)
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEFAULT
#endif

// Macros de log
#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...)   log_internal("[ERROR] ", fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(fmt, ...)    log_internal("[WARN]  ", fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...)    log_internal("[INFO]  ", fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...)   log_internal("[DEBUG] ", fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

// Função interna de log (implementação em log.cpp)
void log_internal(const char* level, const char* fmt, ...);

// Logs críticos (sempre aparecem)
#define LOG_CRITICAL(fmt, ...) log_internal("[CRIT]  ", fmt, ##__VA_ARGS__)
