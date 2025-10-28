#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <Preferences.h>

#include "../miniFlashDataBase_v1_99/FlashLogger.h"
#include "../miniFlashDataBase_v1_99/UploadHelpers.h"

// Pull in implementations so the sketch builds standalone.
#include "../miniFlashDataBase_v1_99/FlashLogger.cpp"
#include "../miniFlashDataBase_v1_99/UploadHelpers.cpp"

#include "../../../modules/cytron_maker_feather_aiot_s3/pins_cytron_maker_feather_aiot_s3.h"
#include "../../../modules/dfrobot_beetle_esp32c6_mini/pins_dfrobot_beetle_esp32c6_mini.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)
namespace board = board_pins::cytron_maker_feather_aiot_s3;
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
namespace board = board_pins::dfrobot_beetle_esp32c6_mini;
#else
namespace board = board_pins::cytron_maker_feather_aiot_s3;
#endif

RTC_DS3231 rtc;
FlashLogger logger(board::FLASH_CS);

static void setRtcAndWait(const DateTime& dt) {
  rtc.adjust(dt);
  const uint32_t target = dt.unixtime();
  for (uint8_t i = 0; i < 25; ++i) {  // wait up to ~250ms
    delay(10);
    if (rtc.now().unixtime() >= target) return;
  }
}

static void appendSample(const DateTime& dt, float temp, float bat, int hb) {
  setRtcAndWait(dt);
  String json = "{\"ts\":" + String(dt.unixtime()) + ",\"temp\":" + String(temp,1) + ",\"bat\":" + String(bat,1) + ",\"hb\":" + String(hb) + "}";
  if (!logger.append(json)) Serial.println(F("[append] failed"));
}

static void queryCallback(const char* line, void* user) {
  ((Stream*)user)->print(line);
}

static void ensureRtcBaseline() {
  DateTime now = rtc.now();
  if (now.unixtime() < 1700000000UL) {
    setRtcAndWait(DateTime(2025, 1, 1, 8, 0, 0));
  }
}

static bool uploadCallback(const RecordHeader& rh, const String& payload, void* user) {
  Stream* io = (Stream*)user;
  io->printf("[upload] seq=%lu ts=%lu payload=%s\n", (unsigned long)rh.seq, (unsigned long)rh.ts, payload.c_str());
  return true;
}

void runPredicateTest() {
  Serial.println(F("\n[test] predicates (bat<=20)"));
  QuerySpec q;
  q.max_records = 10;
  q.out = OUT_JSONL;
  q.predicates[0] = {"bat", PRED_LE, 20.0f};
  q.predicateCount = 1;
  logger.queryLogs(q, queryCallback, &Serial, nullptr, nullptr);
}

bool firstCursor(SyncCursor& out) {
  logger.buildSummaries();
  if (logger.lastDayCount() <= 0) return false;
  uint16_t dayID;
  if (!logger.dayIndexToDayID(0, dayID)) return false;
  if (!logger.selectDay(dayID)) return false;
  if (logger.lastSectorCount() <= 0) return false;
  int sector;
  if (!logger.sectorIndexToSector(0, sector)) return false;
  out.dayID = dayID;
  out.sector = sector;
  out.addr = 0;
  out.seq_next = 0;
  return true;
}

void runExportTest() {
  Serial.println(F("\n[test] exportSinceWithMeta"));
  SyncCursor cur{};
  if (!firstCursor(cur)) {
    Serial.println(F("[export] no data cursor"));
    return;
  }
  FlashLoggerUploadPolicy pol;
  pol.maxAttempts = 1;
  QuerySpec filter;
  filter.predicates[0] = {"temp", PRED_GE, 25.0f};
  filter.predicateCount = 1;
  logger.exportSinceWithMeta(cur, 5, uploadCallback, &Serial, &filter, nullptr);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\nminiFlashDataBase_v1_99 test harness"));

  Wire.begin();
  rtc.begin();

  ensureRtcBaseline();

  Preferences rtcStore;
  if (rtcStore.begin("flog", false)) {
    rtcStore.clear();  // drop any stale RTC metadata so logger appends immediately
    rtcStore.end();
  }

  FlashLoggerConfig cfg;
  cfg.rtc = &rtc;
  cfg.spi_sck_pin  = board::FLASH_SCK;
  cfg.spi_miso_pin = board::FLASH_MISO;
  cfg.spi_mosi_pin = board::FLASH_MOSI;
  cfg.spi_cs_pin   = board::FLASH_CS;
  cfg.spi_clock_hz = 8'000'000;
  cfg.totalSizeBytes = 16UL * 1024UL * 1024UL;
  cfg.persistConfig = false;
  cfg.enableShell = false;
  cfg.csvColumns = "ts,temp,bat";

  if (!logger.begin(cfg)) {
    Serial.println(F("logger.begin failed"));
    while (true) delay(1000);
  }

  logger.factoryReset("847291506314");

  logger.rescanAndRefresh(true, false);

  DateTime baseline(2025, 1, 1, 12, 0, 0);
  DateTime seed = rtc.now();
  if (seed.unixtime() < baseline.unixtime()) {
    setRtcAndWait(baseline);
    seed = rtc.now();
  }

  appendSample(seed + TimeSpan(0, 0, 0, 0), 24.5f, 35.0f, 1);
  appendSample(seed + TimeSpan(0, 0, 1, 0), 26.0f, 18.0f, 1);
  appendSample(seed + TimeSpan(0, 0, 2, 0), 27.5f, 15.0f, 1);
  appendSample(seed + TimeSpan(0, 0, 3, 0), 29.0f, 22.0f, 1);
  appendSample(seed + TimeSpan(0, 0, 4, 0), 30.5f, 10.0f, 1);

  runPredicateTest();
  runExportTest();

  Serial.println(F("\n[test] simulate restart"));
  logger.rescanAndRefresh(true, false);
  runPredicateTest();
  runExportTest();
}

void loop() {
  delay(1000);
}
