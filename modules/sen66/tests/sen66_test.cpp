#include "../../test_support/Arduino.h"
#include "../include/Sen66Driver.h"

#include <cassert>

int main() {
  Sen66Driver driver;
  bool ok = driver.begin();
  assert(ok);

  bool readOk = driver.readOnce();
  assert(readOk);

  float pm = driver.pm25();
  float t = driver.temperature();
  float rh = driver.humidity();

  assert(pm >= 0.0f);
  assert(t > -50.0f && t < 150.0f);
  assert(rh >= 0.0f && rh <= 100.0f);
  return 0;
}

