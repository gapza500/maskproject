#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <vector>
#include "icon.h"

enum class Fx : uint8_t { None, Fade, SlideDown, SlideUp, Blink };
enum class ScreenClass : uint8_t { Small, Mid, Large, Auto };
enum class DataFlow : uint8_t { TopDown, LeftRight };
enum class AlertType : uint8_t { Info, Warn, Error };

struct FxConfig {
  Fx alertEnter = Fx::SlideDown;
  Fx alertExit = Fx::Fade;
  uint16_t speedMs = 200;
  uint8_t  steps = 5;
  bool reduceMotion = true;
};

struct StatusConfig {
  uint8_t sectors = 12;
  uint8_t heightMin = 12;
  bool autoSlot = true;
  bool showGrid = false;
};

struct DataLayout {
  uint8_t cols = 2;
  uint8_t rows = 3;
  bool autoGrid = true;
  DataFlow flow = DataFlow::TopDown;
  uint8_t fontScale = 1;
  uint8_t vGap = 2, hGap = 4;
  bool wordWrap = true;
};

struct AlertRule {
  uint8_t maxPerTypePerRound = 3;
  uint32_t roundMs = 8000;
  uint8_t maxQueue = 10;
};

struct AlertItem {
  AlertType type = AlertType::Info;
  const Icon* icon = nullptr;
  String title, subtitle;
  uint8_t iconScale = 2;
  uint32_t durationMs = 8000;
};

struct ScreenLayoutConfig {
  ScreenClass classHint = ScreenClass::Auto;
  StatusConfig status;
  DataLayout data;
};

struct StatusItem {
  const Icon* icon = nullptr;
  String label;
  int value = 0;
  bool visible = true;
};

class ScreenManagerU8g2 {
public:
  void begin(U8G2* driver);

  // config
  void setStabilityMode(bool on);
  void setMaxFps(uint8_t fps);
  void setContrast(uint8_t c);        // 0–255
  void setBrightnessSoft(float f);    // 0.0–1.0
  void setScreenClass(ScreenClass c);
  void setLayout(const ScreenLayoutConfig& lay);
  void setFx(const FxConfig& fx);
  void setAlertRule(const AlertRule& r);

  // data & status
  void setStatusItem(uint8_t index, const StatusItem& item);
  void setDataArray(const std::vector<String>& arr);

  // alert
  bool pushAlert(const AlertItem& a);
  void clearAlerts();

  // draw loop
  void loop(uint32_t nowMs);

private:
  void autoClassFromSize();
  void drawStatus();
  void drawData();
  void drawAlert(uint32_t nowMs);
  void drawIcon(int16_t x, int16_t y, const Icon* ic, uint8_t scale, bool invert=false);
  void printText(int16_t x, int16_t y, const String& text, uint8_t scale=1);

  U8G2* u8g2_ = nullptr;
  int16_t W_=128, H_=64;

  FxConfig fx_;
  StatusConfig statusCfg_;
  DataLayout dataLayout_;
  AlertRule alertRule_;
  ScreenLayoutConfig layout_;

  bool stability_=true;
  uint8_t maxFps_=20;
  float softBright_=1.0f;
  ScreenClass sclass_=ScreenClass::Auto;

  std::vector<StatusItem> statusSlots_;
  std::vector<String> dataItems_;

  struct AlertState {
    std::vector<AlertItem> q;
    uint32_t roundStart=0;
    uint8_t usedPerType[8]{};
    bool showing=false;
    uint32_t until=0;
    AlertItem cur;
  } alert_;

  uint32_t lastFrame_=0;
};
