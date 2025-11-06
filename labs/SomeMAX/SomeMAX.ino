#include <Wire.h>

#define MAX17048_ADDR 0x36

// ================= FUNCTIONS =================
// อ่าน 2 byte จาก register
uint16_t readRegister16(uint8_t reg) {
  Wire.beginTransmission(MAX17048_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MAX17048_ADDR, (uint8_t)2);

  uint16_t value = Wire.read() << 8;
  value |= Wire.read();
  return value;
}

// อ่านแรงดันแบต (mV)
float readVoltage() {
  uint16_t val = readRegister16(0x02); // VCELL register
  return val * 78.125 / 1000.0; // 78.125 µV per LSB → V
}

// อ่าน SOC (%)
float readSOC() {
  uint16_t val = readRegister16(0x04); // SOC register
  return (val >> 8) + (val & 0xFF)/256.0;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  Wire.begin(21,22); // SDA, SCL ESP32
  delay(100);

  Serial.println("==== MAX17048 TEST ====");

  // ตรวจสอบ device
  Wire.beginTransmission(MAX17048_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("MAX17048 detected!");
  } else {
    Serial.println("MAX17048 NOT detected!");
  }
}

// ================= LOOP =================
void loop() {
  float voltage = readVoltage();
  float soc     = readSOC();

  Serial.print("Battery Voltage: "); Serial.print(voltage); Serial.println(" V");
  Serial.print("Battery SOC    : "); Serial.print(soc); Serial.println(" %");
  Serial.println("-----------------------------");

  delay(2000);
}