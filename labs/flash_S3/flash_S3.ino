#include <Arduino.h>
#include <SPI.h>

// ====== Maker Feather AIoT S3 pins you gave ======
static const int PIN_FLASH_SCK  = 17;  // SCK
static const int PIN_FLASH_MISO = 18;  // MISO
static const int PIN_FLASH_MOSI = 8;   // MOSI
static const int PIN_FLASH_CS   = 7;   // SS / CS
static const int PIN_VPERIPH_EN = 11;  // VP_EN (power rail enable)
static const uint32_t SPI_HZ = 10'000'000; // 10 MHz, MODE0

SPIClass spi = SPI;

// ---- W25Q64 commands ----
#define CMD_RDID  0x9F
#define CMD_RDSR1 0x05
#define CMD_WREN  0x06
#define CMD_SE    0x20   // 4KB sector erase
#define CMD_PP    0x02   // Page program (<=256 B)
#define CMD_READ  0x03
#define SR1_WIP   0x01

uint8_t w25_readStatus() {
  uint8_t sr;
  digitalWrite(PIN_FLASH_CS, LOW);
  spi.transfer(CMD_RDSR1);
  sr = spi.transfer(0x00);
  digitalWrite(PIN_FLASH_CS, HIGH);
  return sr;
}
void w25_waitReady() { while (w25_readStatus() & SR1_WIP) delay(1); }
void w25_writeEnable() {
  digitalWrite(PIN_FLASH_CS, LOW);
  spi.transfer(CMD_WREN);
  digitalWrite(PIN_FLASH_CS, HIGH);
}
void w25_readJEDEC(uint8_t id[3]) {
  digitalWrite(PIN_FLASH_CS, LOW);
  spi.transfer(CMD_RDID);
  id[0] = spi.transfer(0x00);
  id[1] = spi.transfer(0x00);
  id[2] = spi.transfer(0x00);
  digitalWrite(PIN_FLASH_CS, HIGH);
}
void w25_sectorErase(uint32_t addr) {
  w25_writeEnable();
  digitalWrite(PIN_FLASH_CS, LOW);
  spi.transfer(CMD_SE);
  spi.transfer((addr >> 16) & 0xFF);
  spi.transfer((addr >> 8) & 0xFF);
  spi.transfer(addr & 0xFF);
  digitalWrite(PIN_FLASH_CS, HIGH);
  w25_waitReady();
}
void w25_pageProgram(uint32_t addr, const uint8_t* data, size_t len) {
  if (!len || len > 256) return;
  w25_writeEnable();
  digitalWrite(PIN_FLASH_CS, LOW);
  spi.transfer(CMD_PP);
  spi.transfer((addr >> 16) & 0xFF);
  spi.transfer((addr >> 8) & 0xFF);
  spi.transfer(addr & 0xFF);
  for (size_t i = 0; i < len; ++i) spi.transfer(data[i]);
  digitalWrite(PIN_FLASH_CS, HIGH);
  w25_waitReady();
}
void w25_readData(uint32_t addr, uint8_t* out, size_t len) {
  digitalWrite(PIN_FLASH_CS, LOW);
  spi.transfer(CMD_READ);
  spi.transfer((addr >> 16) & 0xFF);
  spi.transfer((addr >> 8) & 0xFF);
  spi.transfer(addr & 0xFF);
  for (size_t i = 0; i < len; ++i) out[i] = spi.transfer(0x00);
  digitalWrite(PIN_FLASH_CS, HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Power the peripheral rail (if your flash is on that rail)
  pinMode(PIN_VPERIPH_EN, OUTPUT);
  digitalWrite(PIN_VPERIPH_EN, HIGH);

  pinMode(PIN_FLASH_CS, OUTPUT);
  digitalWrite(PIN_FLASH_CS, HIGH); // deselect

  // Start SPI with your pins
  spi.begin(PIN_FLASH_SCK, PIN_FLASH_MISO, PIN_FLASH_MOSI, PIN_FLASH_CS);
  spi.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));

  Serial.println("\n=== W25Q64 Quick Test ===");

  uint8_t id[3] = {0};
  w25_readJEDEC(id);
  Serial.printf("JEDEC ID: %02X %02X %02X\r\n", id[0], id[1], id[2]);
  if (id[0]==0x00 && id[1]==0x00 && id[2]==0x00) {
    Serial.println("ERROR: No SPI response (check 3V3, GND, SCK/MISO/MOSI/CS, rail enable).");
  } else if (id[0] != 0xEF) {
    Serial.println("Warning: Not Winbond (0xEF). Continuing anyway...");
  }

  Serial.print("Erasing 4KB @0x000000 ... ");
  w25_sectorErase(0x000000);
  Serial.println("OK");

  uint8_t tx[256], rx[256];
  for (size_t i=0;i<sizeof(tx);++i) tx[i] = (uint8_t)(i ^ 0xA5);

  Serial.print("Programming 256B @0x000000 ... ");
  w25_pageProgram(0x000000, tx, sizeof(tx));
  Serial.println("OK");

  Serial.print("Verifying ... ");
  w25_readData(0x000000, rx, sizeof(rx));
  size_t mism = 0;
  for (size_t i=0;i<sizeof(tx);++i) if (tx[i]!=rx[i]) mism++;
  if (mism==0) Serial.println("PASS ✅");
  else         Serial.printf("FAIL ❌ (%u mismatches)\r\n", (unsigned)mism);
}

void loop() { }
