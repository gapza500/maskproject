#pragma once
#include <Arduino.h>
#include <array>
#include <stdint.h>
#include "status_icons_8x8.h"

#if !defined(SCREEN_BACKEND_U8G2) && !defined(SCREEN_BACKEND_ADAFRUIT)
#define SCREEN_BACKEND_U8G2
#endif

#if !defined(SCREEN_BACKEND_U8G2) && !defined(SCREEN_BACKEND_ADAFRUIT)
#define SCREEN_BACKEND_U8G2
#endif

#if defined(SCREEN_BACKEND_U8G2) && defined(SCREEN_BACKEND_ADAFRUIT)
#error "Select only one screen backend: define SCREEN_BACKEND_U8G2 or SCREEN_BACKEND_ADAFRUIT"
#elif defined(SCREEN_BACKEND_U8G2)
#include <U8g2lib.h>
namespace screen_config {
  using BackendType = U8G2;
  using FontHandle = const uint8_t*;
  constexpr bool kBackendHasFullBuffer = true;
  constexpr bool kBackendSupportsPartialFlush = true;
  inline void backendBegin(BackendType& backend) { backend.begin(); }
  inline void backendStartFrame(BackendType& backend) { backend.setDrawColor(1); }
  inline void backendFinishFrame(BackendType& backend) { (void)backend; }
  inline void backendFlushRect(BackendType& backend, uint8_t tileX, uint8_t tileY, uint8_t tileW, uint8_t tileH) {
    backend.updateDisplayArea(tileX, tileY, tileW, tileH);
  }
  inline void backendSetContrast(BackendType& backend, uint8_t level) { backend.setContrast(level); }
  inline void backendSleep(BackendType& backend, bool sleep) { backend.setPowerSave(sleep ? 1 : 0); }
  inline void backendSetFont(BackendType& backend, FontHandle font) { backend.setFont(font); }
  inline void backendDrawText(BackendType& backend, int16_t x, int16_t y, const char* text) { backend.drawUTF8(x, y, text); }
  inline uint16_t backendTextWidth(BackendType& backend, const char* text) { return backend.getUTF8Width(text); }
  inline uint8_t backendFontAscent(BackendType& backend) { return backend.getAscent(); }
  inline uint8_t backendFontDescent(BackendType& backend) { return backend.getDescent(); }
}
#elif defined(SCREEN_BACKEND_ADAFRUIT)
#include <Adafruit_GFX.h>
namespace screen_config {
  using BackendType = Adafruit_GFX;
  using FontHandle = const GFXfont*;
  constexpr bool kBackendHasFullBuffer = false;
  constexpr bool kBackendSupportsPartialFlush = false;
  inline void backendBegin(BackendType& backend) { (void)backend; }
  inline void backendStartFrame(BackendType& backend) { (void)backend; }
  inline void backendFinishFrame(BackendType& backend) { (void)backend; }
  inline void backendFlushRect(BackendType& backend, uint8_t tileX, uint8_t tileY, uint8_t tileW, uint8_t tileH) {
    (void)backend; (void)tileX; (void)tileY; (void)tileW; (void)tileH;
  }
  inline void backendSetContrast(BackendType& backend, uint8_t level) { (void)backend; (void)level; }
  inline void backendSleep(BackendType& backend, bool sleep) { (void)backend; (void)sleep; }
  inline void backendSetFont(BackendType& backend, FontHandle font) { backend.setFont(font); }
  inline uint16_t backendTextWidth(BackendType& backend, const char* text) {
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    backend.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return w;
  }
  inline uint8_t backendFontAscent(BackendType& backend) { (void)backend; return 8; }
  inline uint8_t backendFontDescent(BackendType& backend) { (void)backend; return 0; }
}
#else
#error "screen_manager requires SCREEN_BACKEND_U8G2 or SCREEN_BACKEND_ADAFRUIT"
#endif

namespace screen_config {

enum class IconId : uint8_t {
  None = 0,
  BatteryEmpty,
  BatteryQuarter,
  BatteryHalf,
  BatteryThreeQuarter,
  BatteryFull,
  WifiOff,
  WifiOn,
  Bluetooth,
  Cloud,
  Usb,
  Storage,
  Info,
  Warning,
  Critical,
  Check,
  Cross,
  Dot,
  Signal0,
  Signal1,
  Signal2,
  Signal3,
};

enum class AlertLevel : uint8_t { Info = 0, Warn = 1, Crit = 2 };

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

struct BoxLayout {
  int16_t x;
  int16_t y;
  uint8_t w;
  uint8_t h;
};

struct GridCellLayout {
  int16_t x;
  int16_t y;
  uint8_t w;
  uint8_t h;
};

enum class DashboardPage : uint8_t { Summary = 0, Detail = 1 };

struct FontPack {
  FontHandle statusFont;
  FontHandle statusLabelFont;
  FontHandle dataFont;
  FontHandle alertTitleFont;
  FontHandle alertDetailFont;
};

#if defined(SCREEN_MANAGER_SMALL_HEAP)
constexpr size_t kStatusLabelMaxChars = 6;
constexpr size_t kDataCellMaxChars = 28;
#else
constexpr size_t kStatusLabelMaxChars = 10;
constexpr size_t kDataCellMaxChars = 40;
#endif
constexpr size_t kStatusValueTextMaxChars = 8;

constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 64;

constexpr uint8_t kStatusBoxCount = 5;
constexpr uint8_t kStatusIconSize = 8;
constexpr uint8_t kStatusLabelBaselineOffset = 4;
constexpr uint8_t kStatusValueBaselineOffset = 9;
constexpr uint8_t kStatusBarHeight = 11;
constexpr uint8_t kStatusBoxPaddingH = 1;
constexpr uint8_t kStatusBoxPaddingV = 1;
constexpr uint8_t kStatusLabelValueGap = 1;
constexpr uint8_t kStatusIconValueGap = 2;
constexpr bool kStatusFramesDefault = false;

constexpr uint8_t kDataRows = 5;
constexpr uint8_t kDataCols = 1;
constexpr uint8_t kDataCellPaddingH = 1;
constexpr uint8_t kDataCellPaddingV = 1;
constexpr bool kDataFramesDefault = false;

constexpr uint8_t kAlertIconSize = 12;
constexpr uint8_t kAlertPaddingX = 6;
constexpr uint8_t kAlertPaddingY = 4;
constexpr uint8_t kAlertBarHeight = 36;
constexpr uint8_t kAlertTitleBaselineOffset = 24;
constexpr uint8_t kAlertDetailBaselineOffset = 34;
constexpr uint8_t kAlertIconTextGap = 2;
constexpr uint8_t kAlertFadeSteps = 6;
constexpr uint16_t kAlertFadeStepMs = 40;
constexpr uint16_t kAlertDefaultDurationMs = 10000;
constexpr uint16_t kAlertCooldownMs = 250;

constexpr uint8_t kDashboardSampleCount = 60;
constexpr uint32_t kDashboardPageIntervalMs = 10000;
constexpr int16_t kDashboardTitleY = static_cast<int16_t>(kStatusBarHeight + 6);
constexpr int16_t kDashboardSidePadding = 4;
constexpr int16_t kDashboardGraphTopOffset = 9;
constexpr int16_t kDashboardGraphBottomMargin = 10;
constexpr int16_t kDashboardFooterBaselineOffset = 2;
constexpr int16_t kDashboardEmojiYOffset = 1;
constexpr int16_t kDashboardLabelGap = 8;

constexpr uint8_t kMaxDirtyRegions = 10;

constexpr uint32_t kDefaultMaxFps = 20;
constexpr uint32_t kMinFrameIntervalMs = 1000 / kDefaultMaxFps;
constexpr uint32_t kFrameIdleIntervalMs = 1000 / 5;
constexpr uint32_t kInactivityDimTimeoutMs = 45000;
constexpr uint32_t kInactivitySleepTimeoutMs = 120000;
constexpr uint8_t kDimActiveLevel = 200;
constexpr uint8_t kDimIdleLevel = 60;

constexpr bool kDefaultThemeInverted = false;

constexpr uint32_t kDataStaleMs = 15000;

constexpr uint32_t kI2cClockHz = 400000;
constexpr uint32_t kSpiClockHz = 8000000;
constexpr uint8_t kI2cAddress = 0x3C;

constexpr uint8_t kIconAtlasTileSize = 16;
constexpr uint8_t kTilePixelSpan = 8;
constexpr uint8_t kMinFlushTiles = 1;

constexpr uint16_t kStatusValueMax = 100;
constexpr uint16_t kStatusValueMin = -32768;

constexpr size_t kStatusValueBufferLen = 12;
constexpr size_t kDataScratchPadding = 8;

constexpr uint16_t kAlertRateLimitPerLoopDefault = 2;

#if defined(SCREEN_MANAGER_METRICS)
constexpr bool kMetricsEnabled = true;
#else
constexpr bool kMetricsEnabled = false;
#endif

inline constexpr std::array<BoxLayout, kStatusBoxCount> kStatusBoxes{{
  {0,   0, 12, kStatusBarHeight},
  {12,  0, 12, kStatusBarHeight},
  {24,  0, 12, kStatusBarHeight},
  {36,  0, 12, kStatusBarHeight},
  {48,  0, 80, kStatusBarHeight},
}};

inline constexpr std::array<GridCellLayout, kDataRows * kDataCols> kGridCells{{
  {0, static_cast<int16_t>(kStatusBarHeight), kScreenWidth, 10},
  {0, static_cast<int16_t>(kStatusBarHeight + 10), kScreenWidth, 10},
  {0, static_cast<int16_t>(kStatusBarHeight + 20), kScreenWidth, 10},
  {0, static_cast<int16_t>(kStatusBarHeight + 30), kScreenWidth, 10},
  {0, static_cast<int16_t>(kStatusBarHeight + 40), kScreenWidth, 10},
}};

inline constexpr Rect kAlertRegion{
  0,
  0,
  kScreenWidth,
  kScreenHeight
};

inline constexpr Rect kDataAreaRegion{
  0,
  static_cast<int16_t>(kStatusBarHeight),
  kScreenWidth,
  static_cast<int16_t>(kScreenHeight - kStatusBarHeight)
};

// Fonts per backend
#if defined(SCREEN_BACKEND_U8G2)
inline constexpr FontPack kFonts{
  u8g2_font_4x6_tr,
  u8g2_font_3x5im_tr,
  u8g2_font_5x8_tf,
  u8g2_font_helvB08_tf,
  u8g2_font_4x6_tr
};
#else
inline constexpr FontPack kFonts{
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr
};
#endif

struct IconInfo {
  IconId id;
  uint8_t width;
  uint8_t height;
  const uint8_t* bitmap;
};

inline constexpr std::array<IconInfo, 22> kIconTable{{
  {IconId::None, 0, 0, nullptr},
  {IconId::BatteryEmpty, 8, 8, status_icons::battery_empty},
  {IconId::BatteryQuarter, 8, 8, status_icons::battery_quarter},
  {IconId::BatteryHalf, 8, 8, status_icons::battery_half},
  {IconId::BatteryThreeQuarter, 8, 8, status_icons::battery_three_quarter},
  {IconId::BatteryFull, 8, 8, status_icons::battery_full},
  {IconId::WifiOff, 8, 8, status_icons::wifi_off},
  {IconId::WifiOn, 8, 8, status_icons::wifi_on},
  {IconId::Bluetooth, 8, 8, status_icons::bluetooth},
  {IconId::Cloud, 0, 0, nullptr},
  {IconId::Usb, 0, 0, nullptr},
  {IconId::Storage, 0, 0, nullptr},
  {IconId::Info, 0, 0, nullptr},
  {IconId::Warning, 8, 8, status_icons::warning},
  {IconId::Critical, 0, 0, nullptr},
  {IconId::Check, 0, 0, nullptr},
  {IconId::Cross, 0, 0, nullptr},
  {IconId::Dot, 8, 8, status_icons::dot},
  {IconId::Signal0, 8, 8, status_icons::signal0},
  {IconId::Signal1, 8, 8, status_icons::signal1},
  {IconId::Signal2, 8, 8, status_icons::signal2},
  {IconId::Signal3, 8, 8, status_icons::signal3},
}};

inline constexpr std::array<IconId, 3> kAlertIcons{
  IconId::Info,
  IconId::Warning,
  IconId::Critical
};

inline constexpr IconId batteryIconForPercent(uint8_t percent) {
  if (percent >= 95) return IconId::BatteryFull;
  if (percent >= 70) return IconId::BatteryThreeQuarter;
  if (percent >= 40) return IconId::BatteryHalf;
  if (percent >= 15) return IconId::BatteryQuarter;
  return IconId::BatteryEmpty;
}

inline constexpr IconId signalIconForRssi(uint8_t level) {
  if (level >= 75) return IconId::Signal3;
  if (level >= 50) return IconId::Signal2;
  if (level >= 25) return IconId::Signal1;
  return IconId::Signal0;
}

inline const IconInfo* iconInfo(IconId id) {
  for (const auto& info : kIconTable) {
    if (info.id == id) {
      return &info;
    }
  }
  return nullptr;
}

}  // namespace screen_config
