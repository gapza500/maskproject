#pragma once

#include <array>

#include "pm_emoji_bitmaps.h"
#include "../../../modules/screen_manager/include/screen_config.h"
#include "../../../modules/screen_manager/include/screen_manager.h"

namespace dashboard_view {

struct DashboardData {
  float pm25 = 0.0f;
  float temperatureC = 0.0f;
  uint8_t batteryPercent = 0;
  uint32_t epochSeconds = 0;
};

struct EmojiAsset {
  const uint8_t* bitmap;
  uint8_t width;
  uint8_t height;
};

class DashboardView {
 public:
  void begin(screen_manager::ScreenManager& screen);
  void setActive(bool active, uint32_t nowMs);
  void updateData(const DashboardData& data, uint32_t nowMs);
  void tick(uint32_t nowMs);
  void forcePage(screen_config::DashboardPage page, uint32_t nowMs);

 private:
  static void renderThunk(screen_config::BackendType& backend, void* ctx);
  void render(screen_config::BackendType& backend);
  void drawSummaryPage(screen_config::BackendType& backend, int16_t titleY);
  void drawDetailPage(screen_config::BackendType& backend, int16_t titleY);
  void pushSample(float sample);
  void markDirty();
  void handlePageRotation(uint32_t nowMs);
  static const char* airQualityLabel(float pm25);
  static const char* temperatureLabel(float temperatureC);
  static EmojiAsset emojiForAir(float pm);
  static EmojiAsset emojiForTemperature(float tempC);
  static void formatClock(uint32_t epochSeconds, char* buffer);

  screen_manager::ScreenManager* screen_ = nullptr;
  DashboardData data_{};
  std::array<float, screen_config::kDashboardSampleCount> samples_{};
  uint8_t sampleHead_ = 0;
  bool active_ = false;
  screen_config::DashboardPage page_ = screen_config::DashboardPage::Summary;
  uint32_t lastPageChangeMs_ = 0;
  bool samplesPrimed_ = false;
};

}  // namespace dashboard_view
