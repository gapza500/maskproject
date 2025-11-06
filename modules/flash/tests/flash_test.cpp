#include "../../test_support/Arduino.h"
#include "../include/FlashStore.h"

#include <cassert>

int main() {
  FlashStore flash;
  bool ok = flash.begin();
  assert(ok);

  flash.simulateUsage(/*usedSectors=*/512, /*totalEraseOps=*/10000, /*avgBytesPerDay=*/256.0f * 1024.0f);
  FlashStore::FlashStats stats = flash.getStats();

  assert(stats.totalMB > 0.0f);
  assert(stats.usedPercent > 0.0f);
  assert(stats.healthPercent <= 100.0f);
  return 0;
}

