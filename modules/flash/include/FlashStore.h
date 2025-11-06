#pragma once

#include <Arduino.h>

class FlashStore {
 public:
  struct FlashStats {
    float totalMB = 0.0f;
    float usedMB = 0.0f;
    float freeMB = 0.0f;
    float usedPercent = 0.0f;
    float healthPercent = 0.0f;
    uint16_t estimatedDaysLeft = 0;
  };

  bool begin();
  bool writeRecord(const uint8_t* data, size_t len);
  size_t readLatest(uint8_t* out, size_t maxlen);

  FlashStats getStats() const { return _stats; }
  void simulateUsage(uint16_t usedSectors,
                     uint32_t totalEraseOps,
                     float avgBytesPerDay = 0.0f);

 private:
  void updateStats(uint16_t usedSectors,
                   uint32_t totalEraseOps,
                   float avgBytesPerDay);

  FlashStats _stats;
};

