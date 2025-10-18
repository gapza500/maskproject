// W25Q64 quick test for Maker Feather AIoT S3
// Pins: CS=7, MOSI=8, SCK=17, MISO=18
#include <Arduino.h>
#include <SPI.h>

#define FLASH_CS   7
#define FLASH_SCK  17
#define FLASH_MOSI 8
#define FLASH_MISO 18

// Winbond basic cmds
#define CMD_RDID  0x9F
#define CMD_RDSR  0x05
#define CMD_WREN  0x06
#define CMD_READ  0x03
#define CMD_PP    0x02        // Page Program (up to 256 bytes)
#define CMD_SE4K  0x20        // Sector Erase 4KB

// Try 40 MHz first; if wiring is long/loose, drop to 8 MHz
#define SPI_HZ 40000000

static inline void spiBeginX()   { SPI.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0)); }
static inline void spiEndX()     { SPI.endTransaction(); }
static inline void csLow()       { digitalWrite(FLASH_CS, LOW); }
static inline void csHigh()      { digitalWrite(FLASH_CS, HIGH); }

void waitBusy() {
  spiBeginX();
  csLow();
  SPI.transfer(CMD_RDSR);
  uint8_t sr;
  do { sr = SPI.transfer(0x00); } while (sr & 0x01);  // WIP bit
  csHigh();
  spiEndX();
}

void wren() {
  spiBeginX();
  csLow();
  SPI.transfer(CMD_WREN);
  csHigh();
  spiEndX();
}

void rdJedec(uint8_t &mfg, uint8_t &mem, uint8_t &cap) {
  spiBeginX();
  csLow();
  SPI.transfer(CMD_RDID);
  mfg = SPI.transfer(0x00);
  mem = SPI.transfer(0x00);
  cap = SPI.transfer(0x00);
  csHigh();
  spiEndX();
}

void setup() {
  Serial.begin(115200);
  delay(400);

  pinMode(FLASH_CS, OUTPUT);
  csHigh();

  // start SPI bus with your custom pins
  SPI.begin(FLASH_SCK, FLASH_MISO, FLASH_MOSI, FLASH_CS);

  Serial.println(F("\n=== W25Q64 Test (Maker Feather AIoT S3) ==="));

  // 1) Read JEDEC ID
  uint8_t mfg, mem, cap;
  rdJedec(mfg, mem, cap);
  Serial.print(F("JEDEC ID: ")); Serial.print(mfg, HEX); Serial.print(' ');
  Serial.print(mem, HEX); Serial.print(' ');
  Serial.println(cap, HEX);

  if (mfg != 0xEF) {
    Serial.println(F("Warn: Not Winbond (mfg!=0xEF) or wiring/power issue. Still proceeding..."));
  }

  // 2) Erase 4KB sector @0x000000
  wren();
  spiBeginX();
  csLow();
  SPI.transfer(CMD_SE4K);
  SPI.transfer(0x00); SPI.transfer(0x00); SPI.transfer(0x00);
  csHigh();
  spiEndX();
  waitBusy();
  Serial.println(F("Erased 4KB sector @0x000000"));

  // 3) Program 16 bytes @0x000000
  uint8_t tx[16];
  for (int i = 0; i < 16; i++) tx[i] = i;  // 00..0F

  wren();
  spiBeginX();
  csLow();
  SPI.transfer(CMD_PP);
  SPI.transfer(0x00); SPI.transfer(0x00); SPI.transfer(0x00);
  for (int i = 0; i < 16; i++) SPI.transfer(tx[i]);
  csHigh();
  spiEndX();
  waitBusy();
  Serial.println(F("Programmed 16 bytes @0x000000"));

  // 4) Read back & verify
  uint8_t rx[16];
  spiBeginX();
  csLow();
  SPI.transfer(CMD_READ);
  SPI.transfer(0x00); SPI.transfer(0x00); SPI.transfer(0x00);
  for (int i = 0; i < 16; i++) rx[i] = SPI.transfer(0x00);
  csHigh();
  spiEndX();

  bool ok = true;
  for (int i = 0; i < 16; i++) if (rx[i] != tx[i]) { ok = false; break; }

  Serial.print(F("Readback: "));
  for (int i = 0; i < 16; i++) { Serial.print(rx[i], HEX); Serial.print(' '); }
  Serial.println();
  Serial.println(ok ? F("Verify: OK") : F("Verify: FAIL"));

  if (!ok) {
    Serial.println(F("Tips: check 3V3/GND, CS=7, MOSI=8, MISO=18, SCK=17; try SPI_HZ=8000000"));
  }
}

void loop() { /* nothing */ }
