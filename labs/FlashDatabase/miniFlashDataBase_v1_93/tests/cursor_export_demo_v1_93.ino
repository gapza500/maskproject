        #include <Arduino.h>
        #include <Wire.h>
        #include <SPI.h>
        #include <RTClib.h>

        #include "../FlashLogger.h"
        #include "../FlashLogger.cpp"

        // Cytron Maker Feather AIoT S3 default wiring (update if you use another board)
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

        static void printRow(const char* line, void* user) {
          if (!user) return;
          ((Stream*)user)->print(line);
        }

        static void dumpCursor(const char* label, const SyncCursor& c) {
          Serial.printf("%s day=%u sector=%d addr=0x%06lX seq=%lu
",
                        label, c.dayID, c.sector, (unsigned long)c.addr, (unsigned long)c.seq_next);
        }

        static void appendSamples(uint8_t count) {
          DateTime base(2025, 1, 1, 12, 0, 0);
          for (uint8_t i = 0; i < count; ++i) {
            rtc.adjust(base + TimeSpan(0, 0, i * 5, 0));
            String payload = "{"temp":" + String(25 + i) + ","bat":" + String(85 - i * 5) + "}";
            if (!logger.append(payload)) {
              Serial.printf("[append] failed at %u
", i);
            }
            delay(5);
          }
        }

        static void runExportDemo() {
          SyncCursor from{};
          logger.getCursor(from);
          from.addr = 0;
          from.seq_next = 0;

          Serial.println("
[export] full scan (exportSince)");
          uint32_t rows = logger.exportSince(from, 0, printRow, &Serial);
          Serial.printf("[export] rows=%lu
", (unsigned long)rows);
        }

        static void runShellDemo() {
          Serial.println("
[shell] cursor + export");
          logger.handleCursorCommand("cursor show", Serial);
          logger.handleCursorCommand("cursor clear", Serial);
          logger.handleCursorCommand("cursor show", Serial);
          logger.handleCursorCommand("export 2", Serial);
        }

        static void runPushedDemo(uint16_t dayID) {
          Serial.printf("
[mark] markDaysPushedUntil(%u)
", dayID);
          logger.markDaysPushedUntil(dayID);
          logger.handleCommand("stats", Serial);
        }

        void setup() {
          Serial.begin(115200);
          delay(400);
          Serial.println("
miniFlashDataBase v1.93 – cursor/export sanity");

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

          if (!logger.begin(&rtc)) {
            Serial.println("[err] logger.begin failed");
            while (true) delay(1000);
          }

          logger.factoryReset("847291506314");
          logger.reinitAfterFactoryReset();

          appendSamples(5);

          SyncCursor head{};
          logger.getCursor(head);
          dumpCursor("
[cursor] write-head", head);

          runExportDemo();
          runShellDemo();
          runPushedDemo(head.dayID);

          Serial.println("
[done] v1.93 sanity complete");
        }

        void loop() {
          delay(1000);
        }
