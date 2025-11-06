#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>

using String = std::string;

struct __FlashStringHelper {};

#ifndef F
#define F(x) reinterpret_cast<const __FlashStringHelper*>(x)
#endif

inline void delay(unsigned long) {}

inline unsigned long millis() {
  static unsigned long fake = 0;
  fake += 10;
  return fake;
}

class SerialStub {
 public:
  template <typename T>
  void print(const T&) {}
  template <typename T>
  void println(const T&) {}
  operator bool() const { return true; }
};

static SerialStub Serial;

