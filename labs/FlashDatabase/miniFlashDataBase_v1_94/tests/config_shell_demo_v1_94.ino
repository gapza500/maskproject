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
          DateTime base(2025, 1, 2, 9, 0, 0);
          for (uint8_t i = 0; i < count; ++i) {
            rtc.adjust(base + TimeSpan(0, 0, i * 10, 0));
            String payload = "{"temp":" + String(24 + i) + ","hum":" + String(55 + i) +
                             ","bat":" + String(88 - i * 2) + "}";
            if (!logger.append(payload)) {
              Serial.printf("[append] failed at %u
", i);
            }
            delay(5);
          }
        }

        static void runShellWalkthrough() {
          Serial.println("
[shell] ls");
          logger.handleCommand("ls", Serial);

          Serial.println("
[shell] ls sectors");
          logger.handleCommand("ls sectors", Serial);

          Serial.println("
[shell] cd day #0 / info / print");
          logger.handleCommand("cd day #0", Serial);
          logger.handleCommand("info", Serial);
          logger.handleCommand("print", Serial);

          Serial.println("
[shell] stats / factory");
          logger.handleCommand("stats", Serial);
          logger.handleCommand("factory", Serial);
        }

        void setup() {
          Serial.begin(115200);
          delay(400);
          Serial.println("
miniFlashDataBase v1.94 – config/shell sanity");

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
          cfg.csvColumns     = "ts,temp,hum,bat";
          cfg.dateStyle      = DATE_ISO;
          cfg.enableShell    = true;

          bool cfgInit = logger.begin(cfg);
          Serial.printf("[cfg] begin(cfg) -> %s
", cfgInit ? "OK" : "FAIL");

          logger.factoryReset("847291506314");
          logger.reinitAfterFactoryReset();

          appendSamples(5);

          FlashStats fs = logger.getFlashStats(3200.0f);
          Serial.printf("
[stats] total=%.2fMB used=%.2fMB free=%.2fMB used=%.1f%% health=%.1f%% estDays=%u
",
                        fs.totalMB, fs.usedMB, fs.freeMB, fs.usedPercent, fs.healthPercent, fs.estimatedDaysLeft);

          runShellWalkthrough();

          Serial.println("
[done] v1.94 sanity complete");
        }

        void loop() {
          delay(1000);
        }
