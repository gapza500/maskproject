#include "../../test_support/Arduino.h"
#include "../Max17048.h"

#include <cassert>
#include <cmath>

int main() {
  Max17048 gauge;
  bool ok = gauge.begin();
  assert(ok);

  float soc = gauge.readPercent();
  float vbat = gauge.readVoltage();

  assert(!std::isnan(soc));
  assert(!std::isnan(vbat));
  return 0;
}

