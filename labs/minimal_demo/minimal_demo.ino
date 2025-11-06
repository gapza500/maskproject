#include <Wire.h>
#include <U8g2lib.h>
#include "icon.h"
#include "screen_manager.h"
#include "screen_showing.h"

U8G2_SSD1309_128X64_NONAME2_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
ScreenManagerU8g2 screen;
ScreenShowingU8g2 showing;

String getText(const String& key){
  if (key=="pm25") return "PM2.5: 22";
  if (key=="temp") return "Temp: 27°C";
  if (key=="hum")  return "Hum: 61%";
  return "-";
}
int16_t getLevel(const String& key){ if (key=="batt") return 75; return 0; }

void setup(){
  Wire.begin(); Wire.setClock(100000);
  u8g2.begin(); u8g2.setContrast(180);

  screen.begin(&u8g2);
  screen.setMaxFps(20);
  screen.setStabilityMode(true);

  StatusMap smap;
  smap.cfg = { .sectors=12, .heightMin=12, .autoSlot=true };
  smap.items = {
    {0, "wifi", &ICON_WIFI_12x12, 1, false},
    {1, "batt", &ICON_BATT_16x8,  1, true}
  };

  Providers prov{ getText, getLevel, nullptr };
  PageSpec home{ .id=PageId::Home, .dataLayout=DataLayout{.cols=2,.rows=2,.autoGrid=true}, 
                 .dataKeys={"pm25","temp","hum"}, .formatters={}, .refreshMs=1000 };

  AlertPolicy ap;
  ap.rule = {.maxPerTypePerRound=3,.roundMs=8000,.maxQueue=10};
  ap.maps = { {"warn", &ICON_WARN_16x16, 2, AlertType::Warn, 6000} };

  showing.begin(&screen);
  showing.setProviders(prov);
  showing.setStatus(smap);
  showing.setPages({home}, PageId::Home);
  showing.setAlertPolicy(ap);
  showing.triggerAlert("warn", "Filter Dirty", "Clean Fan");
}

void loop(){
  showing.loop(millis());
}
