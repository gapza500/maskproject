/*
  W25Q64 Universal Tester for ESP32 dev boards
  - Detects target (ESP32 DevKit v1 / ESP32C6 / ESP32S3) at compile time
  - Binds SPI to correct pins (override with FLASH_CHECK_PIN_* macros if needed)
  - Powers S3 peripheral rail (VP_EN)
  - JEDEC ID -> sector erase -> page program -> verify
*/

#include <Arduino.h>
#include <SPI.h>

// ===================== Board Profiles (auto) =====================
#if defined(CONFIG_IDF_TARGET_ESP32S3)
// --- Maker Feather AIoT S3 ---
#define FLASH_CHECK_PIN_SCK_DEFAULT    17
#define FLASH_CHECK_PIN_MISO_DEFAULT   18
#define FLASH_CHECK_PIN_MOSI_DEFAULT    8
#define FLASH_CHECK_PIN_CS_DEFAULT      7
#define FLASH_CHECK_PIN_VPERIPH_DEFAULT 11
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
// --- ESP32-C6 mini (your earlier wiring) ---
#define FLASH_CHECK_PIN_SCK_DEFAULT    17
#define FLASH_CHECK_PIN_MISO_DEFAULT   18
#define FLASH_CHECK_PIN_MOSI_DEFAULT    8
#define FLASH_CHECK_PIN_CS_DEFAULT      4
#define FLASH_CHECK_PIN_VPERIPH_DEFAULT -1
#elif defined(CONFIG_IDF_TARGET_ESP32)
// --- ESP32 DevKit v1 ---
#define FLASH_CHECK_PIN_SCK_DEFAULT    18
#define FLASH_CHECK_PIN_MISO_DEFAULT   19
#define FLASH_CHECK_PIN_MOSI_DEFAULT   23
#define FLASH_CHECK_PIN_CS_DEFAULT      5
#define FLASH_CHECK_PIN_VPERIPH_DEFAULT -1
#else
#warning "Unknown ESP32 target. Defaulting to S3 mapping. Adjust pins below."
#define FLASH_CHECK_PIN_SCK_DEFAULT    17
#define FLASH_CHECK_PIN_MISO_DEFAULT   18
#define FLASH_CHECK_PIN_MOSI_DEFAULT    8
#define FLASH_CHECK_PIN_CS_DEFAULT      7
#define FLASH_CHECK_PIN_VPERIPH_DEFAULT -1
#endif

#ifndef FLASH_CHECK_PIN_SCK
#define FLASH_CHECK_PIN_SCK FLASH_CHECK_PIN_SCK_DEFAULT
#endif
#ifndef FLASH_CHECK_PIN_MISO
#define FLASH_CHECK_PIN_MISO FLASH_CHECK_PIN_MISO_DEFAULT
#endif
#ifndef FLASH_CHECK_PIN_MOSI
#define FLASH_CHECK_PIN_MOSI FLASH_CHECK_PIN_MOSI_DEFAULT
#endif
#ifndef FLASH_CHECK_PIN_CS
#define FLASH_CHECK_PIN_CS FLASH_CHECK_PIN_CS_DEFAULT
#endif
#ifndef FLASH_CHECK_PIN_VPERIPH
#define FLASH_CHECK_PIN_VPERIPH FLASH_CHECK_PIN_VPERIPH_DEFAULT
#endif

static const int PIN_FLASH_SCK   = FLASH_CHECK_PIN_SCK;    // SCK
static const int PIN_FLASH_MISO  = FLASH_CHECK_PIN_MISO;   // MISO
static const int PIN_FLASH_MOSI  = FLASH_CHECK_PIN_MOSI;   // MOSI
static const int PIN_FLASH_CS    = FLASH_CHECK_PIN_CS;     // CS
static const int PIN_VPERIPH_EN  = FLASH_CHECK_PIN_VPERIPH; // power rail (if available)

// ===== SPI settings =====
static const uint32_t SPI_HZ = 10'000'000; // 10 MHz
static const uint8_t  SPI_MODE = SPI_MODE0;

SPIClass spi = SPI;

// ===== W25Q64 commands =====
#define CMD_RDID   0x9F
#define CMD_RDSR1  0x05
#define CMD_WREN   0x06
#define CMD_READ   0x03
#define CMD_PP     0x02
#define CMD_SE4K   0x20

// ===== Helpers =====
static inline void flashSelect()   { digitalWrite(PIN_FLASH_CS, LOW); }
static inline void flashDeselect() { digitalWrite(PIN_FLASH_CS, HIGH); }

uint8_t w25_readStatus1() {
  flashSelect();
  spi.transfer(CMD_RDSR1);
  uint8_t s = spi.transfer(0x00);
  flashDeselect();
  return s;
}

void w25_writeEnable() {
  flashSelect();
  spi.transfer(CMD_WREN);
  flashDeselect();
}

void w25_waitBusy() {
  // Wait until WIP (bit0) clears
  while (w25_readStatus1() & 0x01) {
    delay(1);
  }
}

void w25_readJEDEC(uint8_t id[3]) {
  flashSelect();
  spi.transfer(CMD_RDID);
  id[0] = spi.transfer(0x00);
  id[1] = spi.transfer(0x00);
  id[2] = spi.transfer(0x00);
  flashDeselect();
}

void w25_readData(uint32_t addr, uint8_t* buf, size_t len) {
  flashSelect();
  spi.transfer(CMD_READ);
  spi.transfer((addr >> 16) & 0xFF);
  spi.transfer((addr >> 8) & 0xFF);
  spi.transfer((addr >> 0) & 0xFF);
  for (size_t i = 0; i < len; ++i) buf[i] = spi.transfer(0x00);
  flashDeselect();
}

void w25_pageProgram(uint32_t addr, const uint8_t* data, size_t len) {
  // len <= 256, must not cross 256-byte page boundary
  w25_writeEnable();
  flashSelect();
  spi.transfer(CMD_PP);
  spi.transfer((addr >> 16) & 0xFF);
  spi.transfer((addr >> 8) & 0xFF);
  spi.transfer((addr >> 0) & 0xFF);
  for (size_t i = 0; i < len; ++i) spi.transfer(data[i]);
  flashDeselect();
  w25_waitBusy();
}

void w25_erase4K(uint32_t addr) {
  // addr should be 4K aligned
  w25_writeEnable();
  flashSelect();
  spi.transfer(CMD_SE4K);
  spi.transfer((addr >> 16) & 0xFF);
  spi.transfer((addr >> 8) & 0xFF);
  spi.transfer((addr >> 0) & 0xFF);
  flashDeselect();
  w25_waitBusy();
}

// ===== Setup/Run =====
void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("=== W25Q64 Universal Test (ESP32-C6 / S3) ===");

  // 1) Power rail for S3
  if (PIN_VPERIPH_EN >= 0) {
    pinMode(PIN_VPERIPH_EN, OUTPUT);
    digitalWrite(PIN_VPERIPH_EN, HIGH);
    delay(5); // allow rail to settle
  }

  // 2) CS idle high
  pinMode(PIN_FLASH_CS, OUTPUT);
  digitalWrite(PIN_FLASH_CS, HIGH);

  // 3) Bind SPI to given pins
  spi.begin(PIN_FLASH_SCK, PIN_FLASH_MISO, PIN_FLASH_MOSI, PIN_FLASH_CS);
  spi.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE));

  // 4) JEDEC ID
  uint8_t id[3];
  w25_readJEDEC(id);
  Serial.printf("JEDEC ID: %02X %02X %02X\r\n", id[0], id[1], id[2]);
  if (id[0] == 0x00 && id[1] == 0x00 && id[2] == 0x00) {
    Serial.println("No SPI response (power/wiring/CS).");
  } else if (id[0] != 0xEF) {
    Serial.println("Warning: Manufacturer is not Winbond (0xEF). Continuing anyway.");
  }

  // 5) Erase + Program + Verify
  const uint32_t base = 0x000000;
  Serial.print("Erasing 4KB sector @0x000000 ... ");
  w25_erase4K(base);
  Serial.println("OK");

  // Prepare test pattern (256 bytes)
  uint8_t tx[256], rx[256];
  for (size_t i = 0; i < sizeof(tx); ++i) tx[i] = (uint8_t)i;

  Serial.print("Programming 256B @0x000000 ... ");
  w25_pageProgram(base, tx, sizeof(tx));
  Serial.println("OK");

  Serial.print("Verifying ... ");
  w25_readData(base, rx, sizeof(rx));
  size_t mism = 0;
  for (size_t i = 0; i < sizeof(tx); ++i) if (tx[i] != rx[i]) mism++;
  if (mism == 0) Serial.println("PASS ✅");
  else           Serial.printf("FAIL ❌ (%u mismatches)\r\n", (unsigned)mism);

  // Small readback demo
  const char hello[] = "Hello W25Q64!";
  w25_pageProgram(0x000100, (const uint8_t*)hello, sizeof(hello));
  uint8_t buf[sizeof(hello)];
  w25_readData(0x000100, buf, sizeof(buf));

  Serial.print("Readback @0x000100: ");
  for (size_t i = 0; i < sizeof(buf); ++i) Serial.print((char)buf[i]);
  Serial.println();
}

void loop() { }
