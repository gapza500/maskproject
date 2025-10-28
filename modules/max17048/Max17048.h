#pragma once

#include <Arduino.h>
#include <Wire.h>

class Max17048 {
public:
  explicit Max17048(TwoWire& wire = Wire, uint8_t address = 0x36)
  : _wire(&wire), _addr(address) {}

  bool begin() {
    if (!_wire) return false;
    _wire->beginTransmission(_addr);
    return _wire->endTransmission() == 0;
  }

  float readPercent() {
    if (!_wire) return NAN;
    if (!readRegister(0x06, _cache, 2)) return NAN;
    return static_cast<float>(_cache[0]) + static_cast<float>(_cache[1]) / 256.0f;
  }

  float readVoltage() {
    if (!_wire) return NAN;
    if (!readRegister(0x02, _cache, 2)) return NAN;
    uint16_t raw = (static_cast<uint16_t>(_cache[0]) << 8) | _cache[1];
    // Voltage LSB = 1.25mV (per datasheet), top 12 bits used.
    raw >>= 4;
    return raw * 1.25f / 1000.0f; // volts
  }

private:
  bool readRegister(uint8_t reg, uint8_t* buf, size_t len) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return false;
    size_t got = _wire->requestFrom(_addr, static_cast<uint8_t>(len));
    if (got != len) return false;
    for (size_t i = 0; i < len; ++i) buf[i] = _wire->read();
    return true;
  }

  TwoWire* _wire;
  uint8_t  _addr;
  uint8_t  _cache[2];
};

