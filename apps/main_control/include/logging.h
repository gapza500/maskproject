#pragma once

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

namespace logx {

enum class Level : uint8_t {
  Info,
  Warn,
  Error,
  Debug,
};

inline const char* levelToString(Level lvl) {
  switch (lvl) {
    case Level::Info: return "INFO";
    case Level::Warn: return "WARN";
    case Level::Error: return "ERROR";
    case Level::Debug: return "DEBUG";
  }
  return "INFO";
}

inline void emit(Level lvl, const char* component, const char* message) {
  const unsigned long ms = millis();
  Serial.printf("[%010lu][%s][%s] %s\n",
                ms,
                levelToString(lvl),
                component ? component : "core",
                message ? message : "");
}

inline void log(Level lvl, const char* component, const char* message) {
  emit(lvl, component, message);
}

inline void logf(Level lvl, const char* component, const char* fmt, ...) {
  char buffer[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  emit(lvl, component, buffer);
}

inline void info(const char* component, const char* message) {
  log(Level::Info, component, message);
}

inline void infof(const char* component, const char* fmt, ...) {
  char buffer[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  emit(Level::Info, component, buffer);
}

inline void warn(const char* component, const char* message) {
  log(Level::Warn, component, message);
}

inline void warnf(const char* component, const char* fmt, ...) {
  char buffer[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  emit(Level::Warn, component, buffer);
}

inline void error(const char* component, const char* message) {
  log(Level::Error, component, message);
}

inline void errorf(const char* component, const char* fmt, ...) {
  char buffer[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  emit(Level::Error, component, buffer);
}

inline void debug(const char* component, const char* message) {
  log(Level::Debug, component, message);
}

inline void debugf(const char* component, const char* fmt, ...) {
  char buffer[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  emit(Level::Debug, component, buffer);
}

}  // namespace logx
