#include "../include/dashboard_view.h"

#include <algorithm>
#include <cstdio>

namespace dashboard_view {

void DashboardView::begin(screen_manager::ScreenManager& screen) {
  screen_ = &screen;
  samples_.fill(0.0f);
  sampleHead_ = 0;
  lastPageChangeMs_ = 0;
  samplesPrimed_ = false;
  screen_->setCustomRenderer(&DashboardView::renderThunk, this);
}

void DashboardView::setActive(bool active, uint32_t nowMs) {
  if (active_ == active) {
    if (active_) {
      lastPageChangeMs_ = nowMs;
    }
    return;
  }
  active_ = active;
  if (active_) {
    page_ = screen_config::DashboardPage::Summary;
    lastPageChangeMs_ = nowMs;
    markDirty();
  }
}

void DashboardView::updateData(const DashboardData& data, uint32_t nowMs) {
  data_ = data;
  pushSample(data.pm25);
  if (active_) {
    markDirty();
  }
  handlePageRotation(nowMs);
}

void DashboardView::tick(uint32_t nowMs) {
  if (!active_) {
    return;
  }
  handlePageRotation(nowMs);
}

void DashboardView::forcePage(screen_config::DashboardPage page, uint32_t nowMs) {
  if (page_ == page) {
    lastPageChangeMs_ = nowMs;
    return;
  }
  page_ = page;
  lastPageChangeMs_ = nowMs;
  markDirty();
}

void DashboardView::renderThunk(screen_config::BackendType& backend, void* ctx) {
  auto* self = static_cast<DashboardView*>(ctx);
  if (self) {
    self->render(backend);
  }
}

void DashboardView::render(screen_config::BackendType& backend) {
  if (!active_ || !screen_) {
    return;
  }

  const bool inverted = screen_->themeInverted();
  const uint8_t fg = inverted ? 0 : 1;
  const uint8_t bg = inverted ? 1 : 0;
  const auto& region = screen_config::kDataAreaRegion;

#ifdef SCREEN_BACKEND_U8G2
  backend.setDrawColor(bg);
  backend.drawBox(region.x, region.y, region.w, region.h);
  backend.setDrawColor(fg);
#else
  backend.fillRect(region.x, region.y, region.w, region.h, bg);
  backend.setTextColor(fg, bg);
#endif

  screen_config::backendSetFont(backend, screen_config::kFonts.dataFont);
  const int16_t titleY = screen_config::kDashboardTitleY;
  const bool summaryPage = (page_ == screen_config::DashboardPage::Summary);

  if (summaryPage) {
    drawSummaryPage(backend, titleY);
  } else {
    drawDetailPage(backend, titleY);
  }
}

void DashboardView::drawSummaryPage(screen_config::BackendType& backend, int16_t titleY) {
  const int16_t graphLeft = screen_config::kDashboardSidePadding;
  const int16_t graphRight =
      screen_config::kScreenWidth - screen_config::kDashboardSidePadding;
  const int16_t graphTop =
      screen_config::kStatusBarHeight + screen_config::kDashboardGraphTopOffset;
  const int16_t graphBottom =
      screen_config::kScreenHeight - screen_config::kDashboardGraphBottomMargin;

  const char* title = "Air Quality Trend";
  int16_t titleWidth = screen_config::backendTextWidth(backend, title);
  int16_t titleX = (screen_config::kScreenWidth - titleWidth) / 2;
  screen_config::backendDrawText(backend, titleX, titleY, title);

#ifdef SCREEN_BACKEND_U8G2
  backend.drawFrame(graphLeft - 1, graphTop - 1,
                    (graphRight - graphLeft) + 2,
                    (graphBottom - graphTop) + 2);
#else
  backend.drawRect(graphLeft - 1, graphTop - 1,
                   (graphRight - graphLeft) + 2,
                   (graphBottom - graphTop) + 2, 1);
#endif

  float minSample = samples_[0];
  float maxSample = samples_[0];
  for (float value : samples_) {
    minSample = std::min(minSample, value);
    maxSample = std::max(maxSample, value);
  }
  float span = maxSample - minSample;
  if (span < 0.5f) {
    float mid = (minSample + maxSample) * 0.5f;
    span = 0.5f;
    minSample = mid - span * 0.5f;
    maxSample = mid + span * 0.5f;
    if (minSample < 0.0f) {
      maxSample -= minSample;
      minSample = 0.0f;
    }
  }

  float lastX = static_cast<float>(graphLeft);
  uint8_t index = sampleHead_ % screen_config::kDashboardSampleCount;
  float lastSample = samples_[index];
  float graphHeight = static_cast<float>(graphBottom - graphTop);
  float lastY = graphBottom - ((lastSample - minSample) / span) * graphHeight;

  for (uint8_t i = 1; i < screen_config::kDashboardSampleCount; ++i) {
    index = (sampleHead_ + i) % screen_config::kDashboardSampleCount;
    float sample = samples_[index];
    float x = graphLeft +
              (i * (graphRight - graphLeft) /
               static_cast<float>(screen_config::kDashboardSampleCount - 1));
    float y = graphBottom - ((sample - minSample) / span) * graphHeight;
#ifdef SCREEN_BACKEND_U8G2
    backend.drawLine(static_cast<int16_t>(lastX), static_cast<int16_t>(lastY),
                     static_cast<int16_t>(x), static_cast<int16_t>(y));
#else
    backend.drawLine(static_cast<int16_t>(lastX), static_cast<int16_t>(lastY),
                     static_cast<int16_t>(x), static_cast<int16_t>(y), 1);
#endif
    lastX = x;
    lastY = y;
  }

  char footer[24];
  std::snprintf(footer, sizeof(footer), "Batt %u%%", data_.batteryPercent);
  int16_t footerWidth = screen_config::backendTextWidth(backend, footer);
  int16_t bottomY =
      screen_config::kScreenHeight - screen_config::kDashboardFooterBaselineOffset;
  screen_config::backendDrawText(backend,
                                 graphRight - footerWidth, bottomY, footer);

  char clock[6];
  formatClock(data_.epochSeconds, clock);
  screen_config::backendDrawText(backend, graphLeft, bottomY, clock);
}

void DashboardView::drawDetailPage(screen_config::BackendType& backend, int16_t titleY) {
  const int16_t columnWidth = screen_config::kScreenWidth / 2;
  const int16_t leftX = 0;
  const int16_t rightX = columnWidth;
  const int16_t topY = titleY + 2;

  char buffer[24];
  const char* quality = airQualityLabel(data_.pm25);
  std::snprintf(buffer, sizeof(buffer), "PM %.1f", data_.pm25);
  int16_t textWidth = screen_config::backendTextWidth(backend, buffer);
  int16_t textX = leftX + (columnWidth - textWidth) / 2;
  screen_config::backendDrawText(backend, textX, topY, buffer);

  EmojiAsset airEmoji = emojiForAir(data_.pm25);
  int16_t emojiX = leftX + (columnWidth - airEmoji.width) / 2;
  int16_t emojiY = topY + screen_config::kDashboardEmojiYOffset;
#ifdef SCREEN_BACKEND_U8G2
  backend.drawXBMP(emojiX, emojiY, airEmoji.width, airEmoji.height, airEmoji.bitmap);
#endif
  int16_t labelY = emojiY + airEmoji.height + screen_config::kDashboardLabelGap;
  int16_t qualityWidth = screen_config::backendTextWidth(backend, quality);
  int16_t qualityX = leftX + (columnWidth - qualityWidth) / 2;
  screen_config::backendDrawText(backend, qualityX, labelY, quality);

  std::snprintf(buffer, sizeof(buffer), "Temp %.1fC", data_.temperatureC);
  textWidth = screen_config::backendTextWidth(backend, buffer);
  textX = rightX + (columnWidth - textWidth) / 2;
  screen_config::backendDrawText(backend, textX, topY, buffer);

  EmojiAsset tempEmoji = emojiForTemperature(data_.temperatureC);
  emojiX = rightX + (columnWidth - tempEmoji.width) / 2;
  emojiY = topY + screen_config::kDashboardEmojiYOffset;
#ifdef SCREEN_BACKEND_U8G2
  backend.drawXBMP(emojiX, emojiY, tempEmoji.width, tempEmoji.height, tempEmoji.bitmap);
#endif
  const char* comfort = temperatureLabel(data_.temperatureC);
  labelY = emojiY + tempEmoji.height + screen_config::kDashboardLabelGap;
  int16_t comfortWidth = screen_config::backendTextWidth(backend, comfort);
  int16_t comfortX = rightX + (columnWidth - comfortWidth) / 2;
  screen_config::backendDrawText(backend, comfortX, labelY, comfort);
}

void DashboardView::pushSample(float sample) {
  if (!samplesPrimed_) {
    samples_.fill(sample);
    samplesPrimed_ = true;
    sampleHead_ = 1 % screen_config::kDashboardSampleCount;
    return;
  }
  samples_[sampleHead_] = sample;
  sampleHead_ = (sampleHead_ + 1) % screen_config::kDashboardSampleCount;
}

void DashboardView::markDirty() {
  if (!screen_) {
    return;
  }
  screen_->markRectDirty(screen_config::kDataAreaRegion);
}

void DashboardView::handlePageRotation(uint32_t nowMs) {
  if (!active_) {
    return;
  }
  if (nowMs - lastPageChangeMs_ < screen_config::kDashboardPageIntervalMs) {
    return;
  }
  page_ = (page_ == screen_config::DashboardPage::Summary)
              ? screen_config::DashboardPage::Detail
              : screen_config::DashboardPage::Summary;
  lastPageChangeMs_ = nowMs;
  markDirty();
}

const char* DashboardView::airQualityLabel(float pm25) {
  if (pm25 <= 12.0f) return "Good";
  if (pm25 <= 35.4f) return "Moderate";
  if (pm25 <= 55.4f) return "Unhealthy-SG";
  if (pm25 <= 150.4f) return "Unhealthy";
  return "Very Unhealthy";
}

const char* DashboardView::temperatureLabel(float temperatureC) {
  if (temperatureC < 18.0f) return "Chilly";
  if (temperatureC <= 28.0f) return "Comfort";
  if (temperatureC <= 34.0f) return "Warm";
  return "Hot";
}

EmojiAsset DashboardView::emojiForAir(float pm) {
  if (pm <= 12.0f) {
    return {pm_emoji::emojiVeryHappy, pm_emoji::emojiVeryHappyWidth,
            pm_emoji::emojiVeryHappyHeight};
  }
  if (pm <= 35.4f) {
    return {pm_emoji::emojiHappy, pm_emoji::emojiHappyWidth,
            pm_emoji::emojiHappyHeight};
  }
  if (pm <= 55.4f) {
    return {pm_emoji::emojiBad, pm_emoji::emojiBadWidth, pm_emoji::emojiBadHeight};
  }
  return {pm_emoji::emojiReallyBad, pm_emoji::emojiReallyBadWidth,
          pm_emoji::emojiReallyBadHeight};
}

EmojiAsset DashboardView::emojiForTemperature(float tempC) {
  if (tempC < 18.0f) {
    return {pm_emoji::emojiBad, pm_emoji::emojiBadWidth, pm_emoji::emojiBadHeight};
  }
  if (tempC <= 28.0f) {
    return {pm_emoji::emojiVeryHappy, pm_emoji::emojiVeryHappyWidth,
            pm_emoji::emojiVeryHappyHeight};
  }
  if (tempC <= 34.0f) {
    return {pm_emoji::emojiHappy, pm_emoji::emojiHappyWidth,
            pm_emoji::emojiHappyHeight};
  }
  return {pm_emoji::emojiReallyBad, pm_emoji::emojiReallyBadWidth,
          pm_emoji::emojiReallyBadHeight};
}

void DashboardView::formatClock(uint32_t epochSeconds, char* buffer) {
  uint32_t mins = (epochSeconds / 60) % 60;
  uint32_t hours = (epochSeconds / 3600) % 24;
  std::snprintf(buffer, 6, "%02lu:%02lu",
                static_cast<unsigned long>(hours),
                static_cast<unsigned long>(mins));
}

}  // namespace dashboard_view
