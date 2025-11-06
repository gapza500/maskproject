#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>
#include "screen_manager.h"

// ==== Providers ====
struct Providers {
  std::function<String(const String& key)>  getText;
  std::function<int16_t(const String& key)> getLevel;
  std::function<bool(const String& key)>    getToggle;
};

// ==== Page ====
using Formatter = std::function<String(const String& raw)>;
enum class PageId: uint8_t { Home, Detail, Debug, Custom1, Custom2 };

struct PageSpec {
  PageId id;
  DataLayout dataLayout;
  std::vector<String> dataKeys;
  std::vector<Formatter> formatters;
  uint32_t refreshMs = 1000;
  Fx enterFx = Fx::None, exitFx = Fx::None;
};

// ==== Status ====
struct StatusMap {
  struct Item {
    uint8_t sector; 
    String key; 
    const Icon* icon = nullptr; 
    uint8_t iconScale = 1;
    bool showValue = false;   // ถ้าอยากเพิ่มตัวเลข/label ข้าง ๆ
  };
  StatusConfig cfg;
  std::vector<Item> items;
};

// ==== Alert ====
struct AlertPolicy {
  AlertRule rule;
  struct Map {
    String key; const Icon* icon; uint8_t iconScale=2; 
    AlertType type=AlertType::Info; uint32_t durationMs=6000;
    Formatter titleFmt, subtitleFmt;
  };
  std::vector<Map> maps;
};

// ==== ScreenShowing ====
class ScreenShowingU8g2 {
public:
  void begin(ScreenManagerU8g2* sm);
  void setProviders(const Providers& p);
  void setStatus(const StatusMap& sm);
  void setPages(const std::vector<PageSpec>& pages, PageId initial);
  void setAlertPolicy(const AlertPolicy& ap);
  void setDefaultFx(const FxConfig& fx);
  void setDefaultRefresh(uint32_t ms);

  void show(PageId id);
  PageId current() const;

  void triggerAlert(const String& key, const String& rawTitle, const String& rawSubtitle);

  void requestDisplayOff(uint32_t ms=10UL*60UL*1000UL);
  void wakeDisplay();

  void loop(uint32_t nowMs);

private:
  void _renderStatus();
  void _applyPage(PageId id);
  void _refreshDataIfNeeded(uint32_t nowMs);
  String _fmt(uint16_t i, const String& raw);

  ScreenManagerU8g2* sm_ = nullptr;
  Providers prov_;
  StatusMap status_;
  AlertPolicy alert_;
  FxConfig fxDefault_{};
  uint32_t defaultRefreshMs_ = 1000;

  std::vector<PageSpec> pages_;
  int8_t curIndex_ = -1;
  uint32_t lastRefresh_ = 0;
};
