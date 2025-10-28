/*
  miniFlashDataBase_v2_0.ino — ESP32-S3 / ESP32-C6 + W25Q128

  Requires in FlashLogger.h:  #define MAX_SECTORS 4096
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include "FlashLogger.h"

#include "../../../modules/cytron_maker_feather_aiot_s3/pins_cytron_maker_feather_aiot_s3.h"
#include "../../../modules/dfrobot_beetle_esp32c6_mini/pins_dfrobot_beetle_esp32c6_mini.h"
#include "../../../modules/max17048/Max17048.h"

// ===== Board profiles =====
#if defined(CONFIG_IDF_TARGET_ESP32S3)
namespace board = board_pins::cytron_maker_feather_aiot_s3;
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
namespace board = board_pins::dfrobot_beetle_esp32c6_mini;
#else
namespace board = board_pins::cytron_maker_feather_aiot_s3;
#endif

static const int PIN_FLASH_SCK   = board::FLASH_SCK;
static const int PIN_FLASH_MISO  = board::FLASH_MISO;
static const int PIN_FLASH_MOSI  = board::FLASH_MOSI;
static const int PIN_FLASH_CS    = board::FLASH_CS;
static const int PIN_VPERIPH_EN  = board::PERIPH_ENABLE_PIN;

constexpr float BATTERY_LOW_PERCENT     = 15.0f;
constexpr float BATTERY_RESUME_PERCENT  = 20.0f;
constexpr unsigned long BATTERY_POLL_MS = 30'000UL;

Max17048 fuelGauge;
bool      fuelGaugePresent   = false;
float     lastBatteryPercent = NAN;
bool      loggingPausedForBattery = false;
unsigned long lastBatteryPollMs = 0;

// ===== Globals =====
RTC_DS3231   rtc;
// IMPORTANT: use CS in ctor so library’s _cs is correct even inside low-level ops.
FlashLogger  logger(PIN_FLASH_CS);

// ===== W25Q128 unprotect (no name collisions) =====
namespace w25x {
  constexpr uint8_t RDSR1 = 0x05, RDSR2 = 0x35, WRSR = 0x01, WREN = 0x06;
  inline void csLow()  { digitalWrite(PIN_FLASH_CS, LOW); }
  inline void csHigh() { digitalWrite(PIN_FLASH_CS, HIGH); }
  inline uint8_t rd(uint8_t cmd){ SPI.beginTransaction(SPISettings(8'000'000,MSBFIRST,SPI_MODE0)); csLow(); SPI.transfer(cmd); uint8_t v=SPI.transfer(0); csHigh(); SPI.endTransaction(); return v; }
  inline void wren(){ SPI.beginTransaction(SPISettings(8'000'000,MSBFIRST,SPI_MODE0)); csLow(); SPI.transfer(WREN); csHigh(); SPI.endTransaction(); }
  inline void wrsr(uint8_t sr1,uint8_t sr2){ SPI.beginTransaction(SPISettings(8'000'000,MSBFIRST,SPI_MODE0)); csLow(); SPI.transfer(WRSR); SPI.transfer(sr1); SPI.transfer(sr2); csHigh(); SPI.endTransaction(); }
  inline void waitWip(){ while (rd(RDSR1)&0x01) delay(1); }
  inline void globalUnprotect(){
    uint8_t sr1b=rd(RDSR1), sr2b=rd(RDSR2);
    uint8_t sr1 = sr1b & ~((1<<7)|(1<<5)|(1<<4)|(1<<3)|(1<<2)); // SRP0,TB,BP2..0 = 0
    uint8_t sr2 = sr2b & ~(1<<6);                                // CMP = 0
    wren(); wrsr(sr1,sr2); waitWip();
    Serial.printf("[unprotect] SR1 %02X->%02X, SR2 %02X->%02X\n", sr1b,sr1,sr2b,sr2);
  }
}

// ===== small serial shell =====
String readLine(){
  static String buf;
  while (Serial.available()){
    char c=(char)Serial.read();
    if (c=='\r') continue;
    if (c=='\n'){ String out=buf; buf=""; return out; }
    buf+=c;
  }
  return String();
}

unsigned long lastBeatMs=0;

void updateBatteryState(bool force = false) {
  if (!fuelGaugePresent) return;
  unsigned long nowMs = millis();
  if (!force && (nowMs - lastBatteryPollMs) < BATTERY_POLL_MS) return;
  lastBatteryPollMs = nowMs;

  float pct = fuelGauge.readPercent();
  if (isnan(pct)) {
    Serial.println("[bat] read failed");
    return;
  }
  pct = constrain(pct, 0.0f, 100.0f);
  lastBatteryPercent = pct;

  if (!loggingPausedForBattery && pct <= BATTERY_LOW_PERCENT) {
    String evt = String("{\"event\":\"battery_low\",\"pct\":") + String(pct, 1) + "}";
    logger.append(evt);
    loggingPausedForBattery = true;
    Serial.printf("[bat] %.1f%% — logging paused\n", pct);
  } else if (loggingPausedForBattery && pct >= BATTERY_RESUME_PERCENT) {
    loggingPausedForBattery = false;
    String evt = String("{\"event\":\"battery_ok\",\"pct\":") + String(pct, 1) + "}";
    logger.append(evt);
    Serial.printf("[bat] %.1f%% — logging resumed\n", pct);
  }
}

String buildHeartbeat(uint32_t ts) {
  String json = "{\"ts\":";
  json += String(ts);
  if (!isnan(lastBatteryPercent)) {
    json += ",\"bat\":";
    json += String(lastBatteryPercent, 1);
  }
  json += ",\"hb\":1}";
  return json;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nBooting...");

  // Power rail (S3)
  if (PIN_VPERIPH_EN >= 0) { pinMode(PIN_VPERIPH_EN, OUTPUT); digitalWrite(PIN_VPERIPH_EN, HIGH); }

  // SPI lines
  pinMode(PIN_FLASH_CS, OUTPUT); digitalWrite(PIN_FLASH_CS, HIGH);
  SPI.begin(PIN_FLASH_SCK, PIN_FLASH_MISO, PIN_FLASH_MOSI, PIN_FLASH_CS);
  delay(10);

  // Unprotect top/bottom blocks
  w25x::globalUnprotect();

  // I2C + RTC
  Wire.begin();
  fuelGaugePresent = fuelGauge.begin();
  if (!fuelGaugePresent) Serial.println("[bat] MAX17048 not found");
  if (!rtc.begin()) { Serial.println("[rtc] NOT found"); }
  else {
    Serial.println("[rtc] ok");
    if (rtc.lostPower()) { rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); Serial.println("[rtc] time set"); }
  }

  // Logger config (note: library still uses _cs internally; ctor fixed that)
  FlashLoggerConfig cfg;
  cfg.rtc            = &rtc;
  cfg.spi_sck_pin    = PIN_FLASH_SCK;
  cfg.spi_miso_pin   = PIN_FLASH_MISO;
  cfg.spi_mosi_pin   = PIN_FLASH_MOSI;
  cfg.spi_cs_pin     = PIN_FLASH_CS;
  cfg.spi_clock_hz   = 8'000'000;                 // conservative
  cfg.totalSizeBytes = 16UL * 1024UL * 1024UL;    // W25Q128 (16 MB)
  cfg.model          = "AirMonitor C6";
  cfg.flashModel     = "W25Q128JV";
  cfg.deviceId       = "001245678912";
  cfg.csvColumns     = "ts,bat,temp";
  cfg.enableShell    = true;

  Serial.println("[chk] about to begin logger...");
  if (!logger.begin(cfg)) {                        // begin(cfg) does NOT scan/select in this lib
    Serial.println("[err] logger.begin() failed."); return;
  }
  Serial.println("[ok] logger.begin() returned.");

  // Build index + anchors + pick today’s sector + write head
  logger.rescanAndRefresh(true, false);            // mandatory with this lib’s begin(cfg) :contentReference[oaicite:3]{index=3}

  // First write (creates day if needed)
  updateBatteryState(true);
  String j = buildHeartbeat(rtc.now().unixtime());
  if (!logger.append(j)) Serial.println("[append] failed (sector not ready)");

  Serial.println("[ready] cmds: ls / print / stats / factory / export 10 / gc / reset");
}

void loop() {
  String cmd = readLine();
  if (cmd.length()) logger.handleCommand(cmd, Serial);

  updateBatteryState();

  if (!loggingPausedForBattery && millis() - lastBeatMs >= 10'000UL) {
    lastBeatMs = millis();
    DateTime now = rtc.now();
    String j = buildHeartbeat(now.unixtime());
    logger.append(j);
  }
}
