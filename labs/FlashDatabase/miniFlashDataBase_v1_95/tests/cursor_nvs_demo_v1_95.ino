        #include <Arduino.h>
        #include <Wire.h>
        #include <SPI.h>
        #include <RTClib.h>

        #include "../FlashLogger.h"
        #include "../FlashLogger.cpp"

        static const int kFlashSck   = 17;
        static const int kFlashMiso  = 18;
        static const int kFlashMosi  = 8;
        static const int kFlashCs    = 7;
        static const int kPeriphEn   = 11;
        static const int kI2cSda     = 42;
        static const int kI2cScl     = 41;

        RTC_DS3231 rtc;
        FlashLogger logger(kFlashCs);

        namespace w25x {
          constexpr uint8_t kRdsr1 = 0x05;
          constexpr uint8_t kRdsr2 = 0x35;
          constexpr uint8_t kWrsr  = 0x01;
          constexpr uint8_t kWren  = 0x06;

          inline void csLow()  { digitalWrite(kFlashCs, LOW); }
          inline void csHigh() { digitalWrite(kFlashCs, HIGH); }

          inline uint8_t rd(uint8_t cmd) {
            SPI.beginTransaction(SPISettings(8'000'000, MSBFIRST, SPI_MODE0));
            csLow();
            SPI.transfer(cmd);
            uint8_t v = SPI.transfer(0);
            csHigh();
            SPI.endTransaction();
            return v;
          }

          inline void wren() {
            SPI.beginTransaction(SPISettings(8'000'000, MSBFIRST, SPI_MODE0));
            csLow();
            SPI.transfer(kWren);
            csHigh();
            SPI.endTransaction();
          }

          inline void wrsr(uint8_t sr1, uint8_t sr2) {
            SPI.beginTransaction(SPISettings(8'000'000, MSBFIRST, SPI_MODE0));
            csLow();
            SPI.transfer(kWrsr);
            SPI.transfer(sr1);
            SPI.transfer(sr2);
            csHigh();
            SPI.endTransaction();
          }

          inline void waitWip() {
            while (rd(kRdsr1) & 0x01) delay(1);
          }

          inline void globalUnprotect() {
            uint8_t sr1b = rd(kRdsr1);
            uint8_t sr2b = rd(kRdsr2);
            uint8_t sr1 = sr1b & ~((1 << 7) | (1 << 5) | (1 << 4) | (1 << 3) | (1 << 2));
            uint8_t sr2 = sr2b & ~(1 << 6);
            wren();
            wrsr(sr1, sr2);
            waitWip();
            Serial.printf("[unprotect] SR1 %02X->%02X, SR2 %02X->%02X
", sr1b, sr1, sr2b, sr2);
          }
        }  // namespace w25x

        static void appendSamples(uint8_t count) {
          DateTime base(2025, 1, 3, 8, 0, 0);
          for (uint8_t i = 0; i < count; ++i) {
            rtc.adjust(base + TimeSpan(0, 0, i * 7, 0));
            String payload = "{"ts":" + String(rtc.now().unixtime()) +
                             ","temp":" + String(22 + i) +
                             ","bat":" + String(60 - i * 3) +
                             ","hb":1}";
            if (!logger.append(payload)) {
              Serial.printf("[append] failed at %u
", i);
            }
            delay(5);
          }
        }

        static void runCursorPersistence() {
          Serial.println("
[cursor] clear -> export 3");
          logger.clearCursor();
          logger.handleCursorCommand("cursor show", Serial);
          logger.handleCursorCommand("export 3", Serial);

          SyncCursor tail{};
          logger.getCursor(tail);
          logger.setCursor(tail);
          logger.handleCursorCommand("cursor show", Serial);

          Serial.println("
[nvs] save flogdemo cur1");
          logger.handleCursorCommand("cursor save flogdemo cur1", Serial);

          Serial.println("
[nvs] clear + load");
          logger.clearCursor();
          logger.handleCursorCommand("cursor show", Serial);
          logger.handleCursorCommand("cursor load flogdemo cur1", Serial);
          logger.handleCursorCommand("cursor show", Serial);

          SyncCursor persisted{};
          if (logger.loadCursorNVS(persisted, "flogdemo", "cur1")) {
            Serial.printf("[nvs] loaded day=%u sector=%d addr=0x%06lX seq=%lu
",
                          persisted.dayID, persisted.sector,
                          (unsigned long)persisted.addr, (unsigned long)persisted.seq_next);
          } else {
            Serial.println("[nvs] loadCursorNVS failed");
          }
        }

        static void runRescanAndDiagnostics() {
          Serial.println("
[rescan] rescanAndRefresh(true, true)");
          logger.rescanAndRefresh(true, true);
          logger.handleCommand("stats", Serial);

          Serial.println("
[scanbad]");
          logger.handleCommand("scanbad", Serial);
        }

        void setup() {
          Serial.begin(115200);
          delay(400);
          Serial.println("
miniFlashDataBase v1.95 – cursor/NVS/scanbad sanity");

          if (kPeriphEn >= 0) {
            pinMode(kPeriphEn, OUTPUT);
            digitalWrite(kPeriphEn, HIGH);
          }

          pinMode(kFlashCs, OUTPUT);
          digitalWrite(kFlashCs, HIGH);
          SPI.begin(kFlashSck, kFlashMiso, kFlashMosi, kFlashCs);
          delay(10);
          w25x::globalUnprotect();

          Wire.begin(kI2cSda, kI2cScl);
          if (!rtc.begin()) {
            Serial.println("[rtc] not found – aborting");
            while (true) delay(1000);
          }
          if (rtc.lostPower()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
          }

          FlashLoggerConfig cfg;
          cfg.rtc            = &rtc;
          cfg.spi_cs_pin     = kFlashCs;
          cfg.spi_sck_pin    = kFlashSck;
          cfg.spi_mosi_pin   = kFlashMosi;
          cfg.spi_miso_pin   = kFlashMiso;
          cfg.spi_clock_hz   = 8'000'000;
          cfg.totalSizeBytes = 16UL * 1024UL * 1024UL;
          cfg.defaultOut     = OUT_JSONL;
          cfg.csvColumns     = "ts,temp,bat,hb";
          cfg.dateStyle      = DATE_ISO;
          cfg.enableShell    = true;

          bool cfgInit = logger.begin(cfg);
          Serial.printf("[cfg] begin(cfg) -> %s
", cfgInit ? "OK" : "FAIL");

          logger.factoryReset("847291506314");
          logger.reinitAfterFactoryReset();

          appendSamples(6);

          runCursorPersistence();
          runRescanAndDiagnostics();

          Serial.println("
[done] v1.95 sanity complete");
        }

        void loop() {
          delay(1000);
        }
