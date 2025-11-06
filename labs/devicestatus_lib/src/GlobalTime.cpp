#include "../include/device_status/GlobalTime.h"

namespace device_status {

DateTime GlobalTime::current_;
bool GlobalTime::valid_ = false;

void GlobalTime::update(const DateTime& dt) {
  current_ = dt;
  valid_ = true;
}

DateTime GlobalTime::current() {
  return current_;
}

uint32_t GlobalTime::unix() {
  return valid_ ? current_.unixtime() : 0;
}

bool GlobalTime::valid() {
  return valid_;
}

}  // namespace device_status
