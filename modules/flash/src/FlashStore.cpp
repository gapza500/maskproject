#include <Arduino.h>
#include <algorithm>

#include "../include/FlashStore.h"

namespace {
constexpr uint16_t kMaxSectors = 4096;          // 16 MB / 4 KB
constexpr float kSectorBytes = 4096.0f;
constexpr float kEraseCycleLimit = 100000.0f;
}

static float bytesToMB(float bytes) {
  return bytes / (1024.0f * 1024.0f);
}

bool FlashStore::begin() {
  updateStats(/*usedSectors=*/0, /*totalEraseOps=*/0, /*avgBytesPerDay=*/0.0f);
  return true;
}

bool FlashStore::writeRecord(const uint8_t* data, size_t len) {
  (void)data;
  (void)len;
  return true;
}

size_t FlashStore::readLatest(uint8_t* out, size_t maxlen) {
  (void)out;
  (void)maxlen;
  return 0;
}

void FlashStore::simulateUsage(uint16_t usedSectors,
                               uint32_t totalEraseOps,
                               float avgBytesPerDay) {
  updateStats(usedSectors, totalEraseOps, avgBytesPerDay);
}

void FlashStore::updateStats(uint16_t usedSectors,
                             uint32_t totalEraseOps,
                             float avgBytesPerDay) {
  if (usedSectors >= kMaxSectors) {
    usedSectors = kMaxSectors - 1;
  }

  float totalMB = bytesToMB((kMaxSectors - 1) * kSectorBytes);
  float usedMB = bytesToMB(usedSectors * kSectorBytes);
  float freeMB = std::max(0.0f, totalMB - usedMB);
  float usedPercent = (totalMB > 0.0f) ? (usedMB / totalMB) * 100.0f : 0.0f;

  float avgCycles = static_cast<float>(totalEraseOps) /
                    std::max(1.0f, static_cast<float>(kMaxSectors - 1));
  float health = 1.0f - (avgCycles / kEraseCycleLimit);
  if (health < 0.0f) health = 0.0f;
  if (health > 1.0f) health = 1.0f;

  uint16_t estimatedDays = 0;
  if (avgBytesPerDay > 0.0f) {
    float freeBytes = freeMB * 1024.0f * 1024.0f;
    float days = freeBytes / avgBytesPerDay;
    if (days < 0.0f) days = 0.0f;
    if (days > 65535.0f) days = 65535.0f;
    estimatedDays = static_cast<uint16_t>(days);
  }

  _stats.totalMB = totalMB;
  _stats.usedMB = usedMB;
  _stats.freeMB = freeMB;
  _stats.usedPercent = usedPercent;
  _stats.healthPercent = health * 100.0f;
  _stats.estimatedDaysLeft = estimatedDays;
}
