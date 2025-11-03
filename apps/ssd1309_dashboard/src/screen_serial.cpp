#include "../include/screen_serial.h"

#include "../include/screen_control.h"

namespace screen_serial {

using namespace screen_control;

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  screen next|prev|<1-6>"));
  Serial.println(F("  wifi on|off"));
  Serial.println(F("  bt on|off"));
  Serial.println(F("  link 0|1|2|3"));
  Serial.println(F("  battery <percent>"));
  Serial.println(F("  pm <ug/m3>"));
  Serial.println(F("  temp <celsius>"));
  Serial.println(F("  hum <percent>"));
  Serial.println(F("  co2 <ppm>"));
  Serial.println(F("  voc <ppm>"));
  Serial.println(F("  nox <ppm>"));
  Serial.println(F("  warn on|off"));
  Serial.println(F("  dash summary|detail"));
  Serial.println(F("  connect"));
  Serial.println(F("  ssid <name>"));
  Serial.println(F("  user <id>"));
  Serial.println(F("  model <text>"));
  Serial.println(F("  help"));
  Serial.println();
}

bool handleCommand(ControlContext& ctx, const String& cmd, uint32_t nowMs) {
  if (cmd == "screen next") {
    setActiveScreen(ctx, nextScreen(ctx.activeScreen), nowMs);
    Serial.print(F("Screen -> "));
    Serial.println(screenName(ctx.activeScreen));
    return true;
  }

  if (cmd == "screen prev") {
    setActiveScreen(ctx, prevScreen(ctx.activeScreen), nowMs);
    Serial.print(F("Screen -> "));
    Serial.println(screenName(ctx.activeScreen));
    return true;
  }

  if (cmd.startsWith("screen ")) {
    int index = cmd.substring(7).toInt();
    if (index >= 1 && index <= static_cast<int>(ScreenId::Count)) {
      setActiveScreen(ctx, static_cast<ScreenId>(index - 1), nowMs);
      Serial.print(F("Screen -> "));
      Serial.println(screenName(ctx.activeScreen));
    } else {
      Serial.println(F("Invalid screen index (1-6)"));
    }
    return true;
  }

  if (cmd == "dash summary") {
    if (ctx.dashboard) {
      ctx.dashboard->forcePage(screen_config::DashboardPage::Summary, nowMs);
    }
    if (ctx.screen) ctx.screen->markAllDirty();
    Serial.println(F("Dashboard page -> summary"));
    return true;
  }

  if (cmd == "dash detail") {
    if (ctx.dashboard) {
      ctx.dashboard->forcePage(screen_config::DashboardPage::Detail, nowMs);
    }
    if (ctx.screen) ctx.screen->markAllDirty();
    Serial.println(F("Dashboard page -> detail"));
    return true;
  }

  if (cmd == "wifi on") {
    setWifi(ctx, true);
    Serial.println(F("WiFi ON"));
    return true;
  }

  if (cmd == "wifi off") {
    setWifi(ctx, false);
    Serial.println(F("WiFi OFF"));
    return true;
  }

  if (cmd == "bt on") {
    setBluetooth(ctx, true);
    Serial.println(F("Bluetooth ON"));
    return true;
  }

  if (cmd == "bt off") {
    setBluetooth(ctx, false);
    Serial.println(F("Bluetooth OFF"));
    return true;
  }

  if (cmd.startsWith("link ")) {
    int value = cmd.substring(5).toInt();
    if (value < 0) value = 0;
    if (value > 3) value = 3;
    setLinkBars(ctx, static_cast<uint8_t>(value));
    Serial.print(F("Link level -> "));
    Serial.println(value);
    return true;
  }

  if (cmd.startsWith("battery ")) {
    int value = cmd.substring(8).toInt();
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    setBatteryPercent(ctx, static_cast<uint8_t>(value), nowMs);
    Serial.print(F("Battery -> "));
    Serial.println(value);
    return true;
  }

  if (cmd.startsWith("pm ")) {
    float value = cmd.substring(3).toFloat();
    if (value < 0.0f) value = 0.0f;
    setPm25(ctx, value, nowMs);
    Serial.print(F("PM2.5 -> "));
    Serial.println(formatFloat(value));
    return true;
  }

  if (cmd.startsWith("temp ")) {
    float value = cmd.substring(5).toFloat();
    setTemperature(ctx, value, nowMs);
    Serial.print(F("Temp -> "));
    Serial.println(formatFloat(value));
    return true;
  }

  if (cmd.startsWith("hum ")) {
    float value = cmd.substring(4).toFloat();
    setHumidity(ctx, value);
    Serial.print(F("Humidity -> "));
    Serial.println(formatFloat(value));
    return true;
  }

  if (cmd.startsWith("co2 ")) {
    float value = cmd.substring(4).toFloat();
    setCo2(ctx, value);
    Serial.print(F("CO2 -> "));
    Serial.println(formatFloat(value, 0));
    return true;
  }

  if (cmd.startsWith("voc ")) {
    float value = cmd.substring(4).toFloat();
    setVoc(ctx, value);
    Serial.print(F("VOC -> "));
    Serial.println(formatFloat(value, 2));
    return true;
  }

  if (cmd.startsWith("nox ")) {
    float value = cmd.substring(4).toFloat();
    setNox(ctx, value);
    Serial.print(F("NOx -> "));
    Serial.println(formatFloat(value, 3));
    return true;
  }

  if (cmd == "warn on") {
    setWarning(ctx, true, nowMs);
    Serial.println(F("Warning ON"));
    return true;
  }

  if (cmd == "warn off") {
    setWarning(ctx, false, nowMs);
    Serial.println(F("Warning OFF"));
    return true;
  }

  if (cmd == "connect") {
    recordConnection(ctx, nowMs);
    Serial.println(F("Connection timestamp updated."));
    return true;
  }

  if (cmd.startsWith("ssid ")) {
    setSsid(ctx, cmd.substring(5));
    Serial.print(F("SSID -> "));
    Serial.println(ctx.user.ssid);
    return true;
  }

  if (cmd.startsWith("user ")) {
    setUser(ctx, cmd.substring(5));
    Serial.print(F("User ID -> "));
    Serial.println(ctx.user.userId);
    return true;
  }

  if (cmd.startsWith("model ")) {
    setModel(ctx, cmd.substring(6));
    Serial.print(F("Model -> "));
    Serial.println(ctx.user.model);
    return true;
  }

  if (cmd == "help") {
    printHelp();
    return true;
  }

  return false;
}

}  // namespace screen_serial

