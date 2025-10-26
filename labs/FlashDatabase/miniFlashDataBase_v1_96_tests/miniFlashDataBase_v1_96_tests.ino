#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include "../../modules/cytron_maker_feather_aiot_s3/pins_cytron_maker_feather_aiot_s3.h"
#include "../../modules/dfrobot_beetle_esp32c6_mini/pins_dfrobot_beetle_esp32c6_mini.h"
#include "../FlashDatabase/miniFlashDataBase_v1_96/FlashLogger.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)
namespace board = board_pins::cytron_maker_feather_aiot_s3;
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
namespace board = board_pins::dfrobot_beetle_esp32c6_mini;
#else
namespace board = board_pins::cytron_maker_feather_aiot_s3;
#endif

static const int kFlashSck  = board::FLASH_SCK;
static const int kFlashMiso = board::FLASH_MISO;
static const int kFlashMosi = board::FLASH_MOSI;
static const int kFlashCs   = board::FLASH_CS;
static const int kVperiphEn = board::PERIPH_ENABLE_PIN;

RTC_DS3231 rtc;
FlashLogger logger(kFlashCs);

namespace w25x {
  constexpr uint8_t kRdsr1 = 0x05;
  constexpr uint8_t kRdsr2 = 0x35;
  constexpr uint8_t kWrsr  = 0x01;
  constexpr uint8_t kWren  = 0x06;
  inline void csLow()  { digitalWrite(kFlashCs, LOW); }
  inline void csHigh() { digitalWrite(kFlashCs, HIGH); }
  inline uint8_t rd(uint8_t cmd){
    SPI.beginTransaction(SPISettings(8'000'000, MSBFIRST, SPI_MODE0));
    csLow();
    SPI.transfer(cmd);
    uint8_t v = SPI.transfer(0);
    csHigh();
    SPI.endTransaction();
    return v;
  }
  inline void wren(){
    SPI.beginTransaction(SPISettings(8'000'000, MSBFIRST, SPI_MODE0));
    csLow();
    SPI.transfer(kWren);
    csHigh();
    SPI.endTransaction();
  }
  inline void wrsr(uint8_t sr1, uint8_t sr2){
    SPI.beginTransaction(SPISettings(8'000'000, MSBFIRST, SPI_MODE0));
    csLow();
    SPI.transfer(kWrsr);
    SPI.transfer(sr1);
    SPI.transfer(sr2);
    csHigh();
    SPI.endTransaction();
  }
  inline void waitWip(){ while (rd(kRdsr1) & 0x01) delay(1); }
  inline void globalUnprotect(){
    uint8_t sr1b = rd(kRdsr1);
    uint8_t sr2b = rd(kRdsr2);
    uint8_t sr1 = sr1b & ~((1<<7)|(1<<5)|(1<<4)|(1<<3)|(1<<2));
    uint8_t sr2 = sr2b & ~(1<<6);
    wren();
    wrsr(sr1, sr2);
    waitWip();
    Serial.printf("[unprotect] SR1 %02X->%02X, SR2 %02X->%02X\n", sr1b, sr1, sr2b, sr2);
  }
}

static void printRow(const char* line, void* user) {
  if (!user) return;
  ((Stream*)user)->print(line);
}

static void appendSamples(size_t count) {
  DateTime base(2025, 1, 1, 0, 0, 0);
  for (size_t i = 0; i < count; ++i) {
    rtc.adjust(base + TimeSpan((int32_t)(i * 300))); // advance 5 minutes per sample
    String payload = "{\"temp\":" + String(25 + (int)i) + ",\"seq\":" + String((unsigned)i) + "}";
    if (!logger.append(payload)) {
      Serial.printf("[test] append %u failed\n", (unsigned)i);
    }
    delay(5);
  }
}

static void testForwardPagination() {
  Serial.println("\n[test] forward pagination");
  QuerySpec q;
  q.max_records = 2;
  q.out = OUT_JSONL;
  q.compact_json = true;

  String token;
  uint32_t page1 = logger.queryLogs(q, printRow, &Serial, nullptr, &token);
  Serial.printf("[page1] count=%lu token=%s\n", (unsigned long)page1, token.c_str());

  if (token.length()) {
    String nextToken;
    uint32_t page2 = logger.queryLogs(q, printRow, &Serial, &token, &nextToken);
    Serial.printf("[page2] count=%lu token=%s\n", (unsigned long)page2, nextToken.c_str());
    token = nextToken;
  }

  if (token.length()) {
    String nextToken;
    uint32_t page3 = logger.queryLogs(q, printRow, &Serial, &token, &nextToken);
    Serial.printf("[page3] count=%lu token=%s\n", (unsigned long)page3, nextToken.c_str());
  }
}

static void testLatestPagination() {
  Serial.println("\n[test] latest pagination");
  String token;
  uint32_t first = logger.queryLatest(3, printRow, &Serial, nullptr, &token);
  Serial.printf("[latest1] count=%lu token=%s\n", (unsigned long)first, token.c_str());

  if (token.length()) {
    String token2;
    uint32_t second = logger.queryLatest(3, printRow, &Serial, &token, &token2);
    Serial.printf("[latest2] count=%lu token=%s\n", (unsigned long)second, token2.c_str());
  }
}

static void testExportPersistence() {
  Serial.println("\n[test] export pagination + NVS cursor");
  logger.handleCursorCommand("cursor clear", Serial);
  logger.handleCursorCommand("export 2", Serial);

  SyncCursor saved{};
  if (logger.loadCursorNVS(saved)) {
    Serial.printf("[nvs] day=%u sector=%d addr=0x%06lX seq=%lu\n",
                  saved.dayID, saved.sector, (unsigned long)saved.addr, (unsigned long)saved.seq_next);
  } else {
    Serial.println("[nvs] load failed");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[Test Harness v1.96]");

  if (kVperiphEn >= 0) {
    pinMode(kVperiphEn, OUTPUT);
    digitalWrite(kVperiphEn, HIGH);
  }

  pinMode(kFlashCs, OUTPUT);
  digitalWrite(kFlashCs, HIGH);
  SPI.begin(kFlashSck, kFlashMiso, kFlashMosi, kFlashCs);
  delay(10);

  w25x::globalUnprotect();

  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("[rtc] NOT found — tests aborted");
    while (true) delay(1000);
  }

  FlashLoggerConfig cfg;
  cfg.rtc            = &rtc;
  cfg.spi_sck_pin    = kFlashSck;
  cfg.spi_miso_pin   = kFlashMiso;
  cfg.spi_mosi_pin   = kFlashMosi;
  cfg.spi_cs_pin     = kFlashCs;
  cfg.spi_clock_hz   = 8'000'000;
  cfg.totalSizeBytes = 16UL * 1024UL * 1024UL;
  cfg.model          = "AirMonitor C6";
  cfg.flashModel     = "W25Q128JV";
  cfg.deviceId       = "001245678912";
  cfg.csvColumns     = "ts,temp,seq";
  cfg.enableShell    = true;

  if (!logger.begin(cfg)) {
    Serial.println("[err] logger.begin() failed");
    while (true) delay(1000);
  }
  logger.rescanAndRefresh(true, false);

  logger.factoryReset("847291506314");
  logger.rescanAndRefresh(true, false);

  appendSamples(6);

  testForwardPagination();
  testLatestPagination();
  testExportPersistence();

  Serial.println("\n[test] complete");
}

void loop() { delay(1000); }
