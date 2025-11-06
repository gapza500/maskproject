#pragma once
#include <Arduino.h>
#include <array>
#include <stdint.h>
#include "screen_config.h"

namespace screen_manager {

enum class TextAlign : uint8_t { Left = 0, Center = 1, Right = 2 };

struct AlertCfg {
  screen_config::AlertLevel level = screen_config::AlertLevel::Info;
  const char* title = nullptr;
  const char* detail = nullptr;
  uint16_t durationMs = screen_config::kAlertDefaultDurationMs;
  screen_config::IconId iconOverride = screen_config::IconId::None;
  bool sticky = false;
};

struct DataFormat {
  const char* units = nullptr;
  bool showUnitsInline = true;
};

using CustomRenderer = void (*)(screen_config::BackendType& backend, void* userData);

class ScreenManager {
 public:
  ScreenManager();

  bool begin(screen_config::BackendType& backend);
  void reset();

  bool setStatusBox(uint8_t slot,
                    screen_config::IconId icon,
                    int16_t value,
                    const char* label = nullptr);
  bool setStatusValue(uint8_t slot, int16_t value);
  bool setStatusValueText(uint8_t slot, const char* text);
  bool notifyStatusValue(uint8_t slot, int16_t value);
  bool setStatusLabel(uint8_t slot, const char* label);
  bool setStatusIcon(uint8_t slot, screen_config::IconId icon);
  bool setStatusValueVisible(uint8_t slot, bool visible);
  bool clearStatusLabel(uint8_t slot);

  bool setData(uint8_t row, uint8_t col, const String& text);
  bool setDataRaw(uint8_t index, const char* text);
  bool setDataAlignment(uint8_t row, uint8_t col, TextAlign align);
  bool setDataFormat(uint8_t row, uint8_t col, const DataFormat& fmt);
  void clearData();
  void setDataAreaEnabled(bool enabled);

  void setCustomRenderer(CustomRenderer renderer, void* userData = nullptr);
  void clearCustomRenderer() { setCustomRenderer(nullptr, nullptr); }

  void markRectDirty(const screen_config::Rect& rect);
  void markAllDirty();

  bool drawStatusBar();
  bool drawDataArea();

  bool showAlert(const AlertCfg& cfg);
  void clearAlert();
  void setAlertRateLimit(uint16_t maxPerLoop);

  bool tick(uint32_t nowMs);
  bool render(uint32_t nowMs);

  void setDim(uint8_t level);
  void sleepDisplay(bool sleep);
  void setThemeInverted(bool inverted);
  void toggleTheme();
  bool themeInverted() const { return themeInverted_; }

  void setAutoDimEnabled(bool enabled);
  void setAutoSleepEnabled(bool enabled);
  void suspendRefresh(bool suspend);
  bool refreshSuspended() const { return refreshSuspended_; }

  void setMaxFps(uint8_t fps);
  void setMinFrameInterval(uint16_t ms);
  void setFrameIdleInterval(uint16_t ms);

  void touch();

  bool dirty() const { return hasDirty_; }

  void setStatusFramesEnabled(bool enabled);
  void setDataFramesEnabled(bool enabled);

  struct StatusSnapshot {
    screen_config::IconId icon;
    int16_t value;
    char label[screen_config::kStatusLabelMaxChars + 1];
    char valueText[screen_config::kStatusValueTextMaxChars + 1];
  };

  void snapshotStatus(StatusSnapshot* out, size_t maxSlots) const;

  struct DirtyRegion {
    screen_config::Rect rect;
    bool active;
  };

  const std::array<DirtyRegion, screen_config::kMaxDirtyRegions>& dirtyRegions() const {
    return dirtyRegions_;
  }

#if defined(SCREEN_MANAGER_METRICS)
  struct Metrics {
    uint32_t framesDrawn = 0;
    uint32_t dirtyMerges = 0;
    uint32_t maxRenderDurationUs = 0;
    uint32_t droppedFrames = 0;
    uint32_t alertsShown = 0;
  };
  const Metrics& metrics() const { return metrics_; }
#endif

 private:
  struct StatusSlot {
    screen_config::IconId icon = screen_config::IconId::None;
    int16_t value = 0;
    char label[screen_config::kStatusLabelMaxChars + 1]{};
    char valueText[screen_config::kStatusValueTextMaxChars + 1]{};
    bool hasLabel = false;
    bool dirty = true;
    bool showValue = true;
    bool useTextValue = false;
    volatile bool pendingUpdate = false;
    volatile int16_t pendingValue = 0;
  };

  struct DataCell {
    char text[screen_config::kDataCellMaxChars + 1]{};
    bool dirty = true;
    TextAlign align = TextAlign::Left;
    uint32_t lastUpdateMs = 0;
    DataFormat format;
  };

  struct ActiveAlert {
    bool active = false;
    bool dirty = false;
    bool fadeOut = false;
    uint8_t fadeStep = 0;
    uint32_t startMs = 0;
    uint32_t deadlineMs = 0;
    AlertCfg cfg;
  };

  bool validStatus(uint8_t slot) const;
  bool validData(uint8_t row, uint8_t col) const;
  uint8_t dataIndex(uint8_t row, uint8_t col) const;
  void applyPendingStatus();
  void setLabel(StatusSlot& slot, const char* label);
  void markStatusDirty(uint8_t slot);
  void markDataDirty(uint8_t index);
  void trackDirtyRect(const screen_config::Rect& rect);
  void mergeDirtyRegions();
  void resetDirtyRegions();
  void drawStatusSlot(uint8_t slot);
  void drawDataCell(uint8_t index);
  void clearRegion(const screen_config::Rect& rect);
  void drawIcon(screen_config::IconId icon, int16_t x, int16_t y);
  void drawAlertOverlay();
  void updatePowerState(uint32_t nowMs);
  void updateAlertState(uint32_t nowMs);
  void setFrontendFont(screen_config::FontHandle font);
  void applyTheme();
  void updateDimLevel(uint8_t level);

  CustomRenderer customRenderer_ = nullptr;
  void* customRendererUser_ = nullptr;
  screen_config::BackendType* backend_ = nullptr;
  std::array<StatusSlot, screen_config::kStatusBoxCount> statusSlots_{};
  std::array<DataCell, screen_config::kDataRows * screen_config::kDataCols> dataCells_{};
  std::array<DirtyRegion, screen_config::kMaxDirtyRegions> dirtyRegions_{};
  ActiveAlert alert_{};
  uint32_t lastTickMs_ = 0;
  uint32_t lastRenderMs_ = 0;
  uint32_t lastActivityMs_ = 0;
  uint16_t alertRateLimit_ = screen_config::kAlertRateLimitPerLoopDefault;
  uint16_t alertsShownThisLoop_ = 0;
  uint32_t lastAlertToggleMs_ = 0;
  uint16_t minFrameIntervalMs_ = screen_config::kMinFrameIntervalMs;
  uint16_t idleFrameIntervalMs_ = screen_config::kFrameIdleIntervalMs;
  uint8_t maxFps_ = screen_config::kDefaultMaxFps;
  uint8_t currentDimLevel_ = screen_config::kDimActiveLevel;
  bool themeInverted_ = screen_config::kDefaultThemeInverted;
  bool autoDimEnabled_ = true;
  bool autoSleepEnabled_ = true;
  bool displaySleeping_ = false;
  bool refreshSuspended_ = false;
  bool forceFullFrame_ = true;
  bool hasDirty_ = true;
  bool dimDirty_ = true;
  bool themeDirty_ = true;
  bool showStatusFrames_ = screen_config::kStatusFramesDefault;
  bool showDataFrames_ = screen_config::kDataFramesDefault;
  bool dataAreaEnabled_ = true;

#if defined(SCREEN_MANAGER_METRICS)
  Metrics metrics_{};
#endif
};

}  // namespace screen_manager
