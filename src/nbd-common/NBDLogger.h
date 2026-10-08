#pragma once
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

namespace nbd {

enum class NBDLogLevel { None = 0, Error, Info, Debug };

/**
 * @brief Minimal logger: call NBDLogger::begin(Serial, NBDLogLevel::Info) to
 * activate the output.
 */
class NBDLogger {
 public:
  static void begin(Print& out, NBDLogLevel level = NBDLogLevel::Info) {
    output() = &out;
    logLevel() = level;
  }

  static bool isActive(NBDLogLevel level) {
    return output() != nullptr && level != NBDLogLevel::None &&
           level <= logLevel();
  }

  static void log(NBDLogLevel level, const char* fmt, ...) {
    if (!isActive(level)) return;
    char msg[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    Print& out = *output();
    out.print("[nbd] ");
    out.print(levelName(level));
    out.print(": ");
    out.println(msg);
  }

 protected:
  static Print*& output() {
    static Print* out = nullptr;
    return out;
  }
  static NBDLogLevel& logLevel() {
    static NBDLogLevel level = NBDLogLevel::Info;
    return level;
  }
  static const char* levelName(NBDLogLevel level) {
    switch (level) {
      case NBDLogLevel::Error:
        return "E";
      case NBDLogLevel::Info:
        return "I";
      default:
        return "D";
    }
  }
};

}  // namespace nbd

#define NBD_LOGE(...) nbd::NBDLogger::log(nbd::NBDLogLevel::Error, __VA_ARGS__)
#define NBD_LOGI(...) nbd::NBDLogger::log(nbd::NBDLogLevel::Info, __VA_ARGS__)
#define NBD_LOGD(...) nbd::NBDLogger::log(nbd::NBDLogLevel::Debug, __VA_ARGS__)
