#pragma once

#include <RTClib.h>

namespace device_status {

class GlobalTime {
 public:
  static void update(const DateTime& dt);
  static DateTime current();
  static uint32_t unix();
  static bool valid();

 private:
  static DateTime current_;
  static bool valid_;
};

}  // namespace device_status

