#include "FlashLogger.h"
#include <string.h>
#include <math.h>
#include <Preferences.h>

namespace {
  int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  }
}

// ===== CRC16 (Modbus/A001) =====
uint16_t FlashLogger::crc16(const uint8_t* data, uint32_t len, uint16_t seed) {
  uint16_t crc = seed;
  for (uint32_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
  }
  return crc;
}

// ===== ctor =====
FlashLogger::FlashLogger(uint8_t csPin) : _cs(csPin) {}

// ===== begin =====
bool FlashLogger::begin(RTC_DS3231* rtc) {
  _rtc = rtc;

  SPI.begin(17, 18, 8, _cs);
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  delay(5);

  for (int i = 0; i < MAX_SECTORS; ++i) _index[i] = {false, 0, false, 0};

  if (!loadFactoryInfo()) {
    memset(&_factory, 0, sizeof(_factory));
    _factory.magic = 0x46414354UL; // 'FACT'
    strncpy(_factory.model, "AirMonitor C6", sizeof(_factory.model)-1);
    strncpy(_factory.flashModel, "W25Q64JV", sizeof(_factory.flashModel)-1);
    strncpy(_factory.deviceId, "000000000000", sizeof(_factory.deviceId)-1);
    _factory.firstDayID = dayIDFromDateTime(_rtc->now());
    _factory.defaultDateStyle = (uint8_t)_dateStyle;
    _factory.totalEraseOps = 0;
    _factory.bootCounter = 0;
    _factory.startHint = 0;
    _factory.badCount = 0;
    sectorErase(sectorBaseAddr(FACTORY_SECTOR), false);
    saveFactoryInfo();
  } else {
    if (_factory.defaultDateStyle >= 1 && _factory.defaultDateStyle <= 3)
      _dateStyle = (DateStyle)_factory.defaultDateStyle;
  }

  // bump boot counter & set generation
  _factory.bootCounter++;
  saveFactoryInfo();
  _generation = _factory.bootCounter;

  scanAllSectorsBuildIndex();
  if (!loadAnchorsFromNVS()) buildAnchors(true);

  DateTime now = _rtc->now();
  _currentDay = dayIDFromDateTime(now);

  selectOrCreateTodaySector();
  if (_currentSector >= 0) {
    findLastWritePositionInSector(_currentSector);
    _writeAddr = _index[_currentSector].writePtr;
  } else {
    Serial.println("FlashLogger: no sector available!");
    return false;
  }

  _seqCounter  = 0;
  _todayBytes  = 0;
  _lowSpace    = false;

  Serial.printf("FlashLogger v1.8 ready. Gen=%lu Day=%u Sector=%d Next=0x%06lX\n",
                (unsigned long)_generation, _currentDay, _currentSector, _writeAddr);
  return true;
}

bool FlashLogger::begin(const FlashLoggerConfig& cfg) {
  _cfg = cfg;
  if (_cfg.persistConfig) {
    FlashLoggerConfig stored = _cfg;
    if (loadConfigFromNVS(stored, _cfg.configNamespace ? _cfg.configNamespace : "flcfg")) {
      stored.rtc = _cfg.rtc; // preserve runtime pointer
      _cfg = stored;
    }
  }
  _rtc = cfg.rtc;
  if (!_rtc) { Serial.println("FlashLogger: RTC is required"); return false; }

  if (cfg.spi_cs_pin >= 0) _cs = (uint8_t)cfg.spi_cs_pin;

  // SPI setup (respect custom pins if provided)
  if (cfg.spi_sck_pin >= 0 && cfg.spi_mosi_pin >= 0 && cfg.spi_miso_pin >= 0) {
    SPI.begin(cfg.spi_sck_pin, cfg.spi_miso_pin, cfg.spi_mosi_pin, cfg.spi_cs_pin);
  } else {
    SPI.begin(); // defaults
  }
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  delay(5);

  for (int i = 0; i < MAX_SECTORS; ++i) _index[i] = {false, 0, false, 0};

  if (!loadFactoryInfo()) {
    memset(&_factory, 0, sizeof(_factory));
    _factory.magic = 0x46414354UL; // 'FACT'
    strncpy(_factory.model, "AirMonitor C6", sizeof(_factory.model) - 1);
    strncpy(_factory.flashModel, "W25Q64JV", sizeof(_factory.flashModel) - 1);
    strncpy(_factory.deviceId, "000000000000", sizeof(_factory.deviceId) - 1);
    _factory.firstDayID = dayIDFromDateTime(_rtc->now());
    _factory.defaultDateStyle = (uint8_t)_dateStyle;
    _factory.totalEraseOps = 0;
    _factory.bootCounter = 0;
    _factory.startHint = 0;
    _factory.badCount = 0;
    sectorErase(sectorBaseAddr(FACTORY_SECTOR), false);
    saveFactoryInfo();
  } else {
    if (_factory.defaultDateStyle >= 1 && _factory.defaultDateStyle <= 3)
      _dateStyle = (DateStyle)_factory.defaultDateStyle;
  }

  if (cfg.dateStyle >= DATE_THAI && cfg.dateStyle <= DATE_US) {
    _dateStyle = cfg.dateStyle;
    _factory.defaultDateStyle = (uint8_t)_dateStyle;
  }
  setOutputFormat(cfg.defaultOut);
  setCsvColumns(cfg.csvColumns);

  // Set factory info once if empty (reuse your existing setFactoryInfo logic)
  setFactoryInfo(cfg.model, cfg.flashModel, cfg.deviceId);

  _factory.bootCounter++;
  saveFactoryInfo();
  _generation = _factory.bootCounter;

  if (_cfg.persistConfig) {
    saveConfigToNVS(_cfg, _cfg.configNamespace ? _cfg.configNamespace : "flcfg");
  }

  scanAllSectorsBuildIndex();
  if (!loadAnchorsFromNVS()) buildAnchors(true);

  uint32_t storedUnix = 0;
  if (!loadLastTimestampNVS(storedUnix)) storedUnix = 0;
  _lastGoodUnix = storedUnix;
  if (_rtc) {
    uint32_t bootUnix = _rtc->now().unixtime();
    if (isRtcTimestampValid(bootUnix)) {
      _rtcHealthy = true;
      _rtcWarningShown = false;
      _lastGoodUnix = bootUnix;
      saveLastTimestampNVS(bootUnix);
    } else {
      _rtcHealthy = false;
      _rtcWarningShown = false;
      Serial.println("FlashLogger: RTC invalid; logging paused until time restored.");
    }
  } else {
    _rtcHealthy = false;
    _rtcWarningShown = false;
    Serial.println("FlashLogger: RTC not available; logging paused.");
  }

  DateTime now = _rtc ? _rtc->now() : DateTime((uint32_t)_lastGoodUnix);
  _currentDay = dayIDFromDateTime(now);

  selectOrCreateTodaySector();
  if (_currentSector >= 0) {
    findLastWritePositionInSector(_currentSector);
    _writeAddr = _index[_currentSector].writePtr;
  } else {
    Serial.println("FlashLogger: no sector available!");
    return false;
  }

  _seqCounter = 0;
  _todayBytes = 0;
  _lowSpace = false;

  if (_cfg.enableShell) Serial.println("[FlashLogger] shell enabled (ls/cd/print/q/fmt/cursor/export/reset/gc/stats/factory)");
  Serial.printf("FlashLogger v1.8 ready. Gen=%lu Day=%u Sector=%d Next=0x%06lX\n",
                (unsigned long)_generation, _currentDay, _currentSector, _writeAddr);
  return true;
}

void FlashLogger::setOutputFormat(OutFmt fmt) { _outFmt = fmt; }

void FlashLogger::setCsvColumns(const char* cols) {
  if (!cols || !*cols) return;
  strncpy(_csvCols, cols, sizeof(_csvCols)-1);
  _csvCols[sizeof(_csvCols)-1] = 0;
}

// ===== append (atomic: header -> payload -> commit) =====
bool FlashLogger::append(const String& json) {
  if (_currentSector < 0) return false;

  if (!_rtc) {
    if (!_rtcWarningShown) Serial.println("FlashLogger: RTC unavailable; append blocked.");
    _rtcHealthy = false;
    _rtcWarningShown = true;
    return false;
  }

  DateTime now = _rtc->now();
  uint32_t unixNow = now.unixtime();
  if (!isRtcTimestampValid(unixNow)) {
    if (!_rtcWarningShown) Serial.println("FlashLogger: RTC timestamp invalid/backwards; append blocked.");
    _rtcHealthy = false;
    _rtcWarningShown = true;
    return false;
  }
  _rtcHealthy = true;
  _rtcWarningShown = false;

  // Newline-terminated payload (no NUL)
  String payload = json;
  if (payload.isEmpty() || payload[payload.length()-1] != '\n') payload += '\n';
  const uint16_t payLen = (uint16_t)payload.length();
  const uint32_t need   = sizeof(RecordHeader) + payLen + 1; // + commit

  // New day?
  uint16_t today = dayIDFromDateTime(now);
  if (today != _currentDay) {
    _currentDay  = today;
    _todayBytes  = 0;
    _lowSpace    = false;

    if (!moveToNextSectorSameDay()) {
      // find any empty sector (round-robin is in selectOrCreate...)
      selectOrCreateTodaySector();
      if (_currentSector < 0) {
        Serial.println("FlashLogger: no sector to start new day.");
        return false;
      }
    }
  }

  // Back-pressure (optional)
  if (_maxDailyBytes) {
    if (_todayBytes + need > _maxDailyBytes) {
      _lowSpace = true;
      Serial.println("Back-pressure: max daily bytes exceeded, append blocked.");
      return false;
    }
  }

  // Sector full?
  if (!sectorHasSpace(_currentSector, need)) {
    if (!moveToNextSectorSameDay()) {
      Serial.println("FlashLogger: out of sectors; append aborted.");
      return false;
    }
  }

  // Build header
  RecordHeader rh{};
  rh.len   = payLen;
  rh.ts    = now.secondstime();
  rh.seq   = _seqCounter++;
  rh.flags = 0;
  rh.rsv   = 0;
  rh.crc   = crc16((const uint8_t*)payload.c_str(), payLen, 0xFFFF);

  // Write: header
  pageProgram(_index[_currentSector].writePtr, (const uint8_t*)&rh, sizeof(rh));
  if (!verifyWrite(_index[_currentSector].writePtr, (const uint8_t*)&rh, sizeof(rh))) {
    Serial.println("Header verify FAIL (append), aborting record.");
    return false;
  }
  _index[_currentSector].writePtr += sizeof(rh);

  // Write: payload
  pageProgram(_index[_currentSector].writePtr, (const uint8_t*)payload.c_str(), payLen);
  if (!verifyWrite(_index[_currentSector].writePtr, (const uint8_t*)payload.c_str(), payLen)) {
    Serial.println("Payload verify FAIL (append), aborting record.");
    return false;
  }
  _index[_currentSector].writePtr += payLen;

  // Write: commit
  uint8_t cm = REC_COMMIT;
  pageProgram(_index[_currentSector].writePtr, &cm, 1);
  if (!verifyWrite(_index[_currentSector].writePtr, &cm, 1)) {
    Serial.println("Commit verify FAIL (append), aborting record.");
    return false;
  }
  _index[_currentSector].writePtr += 1;

  _writeAddr  = _index[_currentSector].writePtr;
  _todayBytes += need;
  _lastGoodUnix = unixNow;
  saveLastTimestampNVS(unixNow);

  Serial.printf("APPEND @0x%06lX (sec %d) len=%u\n",
                _writeAddr - need, _currentSector, (unsigned)payLen);
  return true;
}

// ===== formatted print (CRC + commit enforced) =====
void FlashLogger::printFormattedLogs() {
  buildSummaries();
  for (int i = 0; i < _dayCount; ++i) {
    const uint16_t day = _days[i].dayID;
    char dateBuf[20]; formatDayID(day, dateBuf, sizeof(dateBuf));
    Serial.println("----------------------------------");
    Serial.printf("date %s\n{\n", dateBuf);

    // print all sectors that belong to this day (order doesn’t matter)
    for (int s = 0; s < MAX_SECTORS; ++s) {
      if (s == FACTORY_SECTOR) continue;
      if (!_index[s].present) continue;
      if (_index[s].dayID != day) continue;
      printSectorData(s);
      yield();
    }
    Serial.println("}\n----------------------------------");
  }
}


// ===== raw dump (valid records only) =====
void FlashLogger::readAll() {
  Serial.println("=== RAW DUMP (valid records only) ===");
  uint8_t b;

  for (int s = 0; s < MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present) continue;

    uint32_t base = sectorBaseAddr(s);
    Serial.printf("\n[SECTOR %d @ 0x%06lX] dayID=%u pushed=%d\n",
                  s, base, _index[s].dayID, (int)_index[s].pushed);

    uint32_t ptr = base + sizeof(SectorHeader);
    while (ptr + sizeof(RecordHeader) < base + SECTOR_SIZE) {
      RecordHeader rh;
      readData(ptr, (uint8_t*)&rh, sizeof(rh));
      if (rh.len == 0xFFFF || rh.len == 0x0000) break;
      if (ptr + sizeof(rh) + rh.len + 1 > base + SECTOR_SIZE) break;

      // read payload + compute crc
      uint32_t p = ptr + sizeof(rh);
      uint16_t remaining = rh.len;
      uint16_t crc = 0xFFFF;
      static uint8_t buf[PAGE_SIZE];

      while (remaining) {
        uint16_t chunk = min<uint16_t>(remaining, PAGE_SIZE);
        readData(p, buf, chunk);
        crc = crc16(buf, chunk, crc);
        for (int i=0;i<chunk;i++) Serial.write(buf[i]);
        p += chunk;
        remaining -= chunk;
        yield();
      }

      // commit check
      readData(ptr + sizeof(rh) + rh.len, &b, 1);
      if (b != REC_COMMIT || crc != rh.crc) {
        Serial.println(F("[corrupt/partial record skipped]"));
        break;
      }

      ptr += sizeof(rh) + rh.len + 1;
    }
  }
  Serial.println("\n=== END RAW DUMP ===");
}

// ===== push marking =====
void FlashLogger::markDayPushed(uint16_t dayID) {
  for (int s = 0; s < MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (_index[s].present && _index[s].dayID == dayID) {
      _index[s].pushed = true;
      SectorHeader hdr;
      if (readSectorHeader(s, hdr)) {
        hdr.pushed = 1;
        pageProgram(sectorBaseAddr(s), (const uint8_t*)&hdr, sizeof(SectorHeader));
        verifyWrite(sectorBaseAddr(s), (const uint8_t*)&hdr, sizeof(SectorHeader));
      }
    }
  }
  Serial.printf("Marked day %u as pushed.\n", dayID);
}
void FlashLogger::markCurrentDayPushed() { markDayPushed(_currentDay); }

// ===== gc (generation-aware note in print) =====
void FlashLogger::gc() {
  DateTime now = _rtc->now();
  uint16_t todayID = dayIDFromDateTime(now);
  Serial.println("🧹 GC: checking sectors...");

  for (int s = 0; s < MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present) continue;
    if (!_index[s].pushed) continue;

    SectorHeader hdr;
    if (!readSectorHeader(s, hdr)) continue;

    if (isOlderThanNDays(todayID, hdr.dayID, 7)) {
      if (!markSectorEraseIntent(s)) {
        Serial.printf("  skip sector %d: failed to mark erase intent\n", s);
        continue;
      }
      uint32_t base = sectorBaseAddr(s);
      sectorErase(base); // counts erase & verifies (quarantine if fail)
      _index[s] = {false, 0, false, 0};
      Serial.printf("  erased sector %d (day=%u, gen=%lu)\n", s, hdr.dayID, (unsigned long)hdr.generation);
    }
    yield();
  }
  buildAnchors(true);
}

// ===== setDateStyle =====
void FlashLogger::setDateStyle(uint8_t style) {
  if (style < 1 || style > 3) return;
  _dateStyle = (DateStyle)style;
  _factory.defaultDateStyle = style;
  saveFactoryInfo();
}

// ===== low-level =====
void FlashLogger::writeEnable() {
  const uint32_t hz = _cfg.spi_clock_hz ? _cfg.spi_clock_hz : 40000000;
  SPI.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  SPI.transfer(CMD_WREN);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

uint8_t FlashLogger::readStatusReg() {
  const uint32_t hz = _cfg.spi_clock_hz ? _cfg.spi_clock_hz : 40000000;
  SPI.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  SPI.transfer(CMD_RDSR1);
  uint8_t sr = SPI.transfer(0x00);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  return sr;
}

void FlashLogger::waitWhileBusy(uint32_t timeout_ms) {
  uint32_t start = millis();
  while (true) {
    if ((readStatusReg() & 0x01) == 0) break;
    delay(1);
    if (timeout_ms && (millis() - start) >= timeout_ms) break;
  }
}

void FlashLogger::readData(uint32_t addr, uint8_t* buf, uint16_t len) {
  const uint32_t hz = _cfg.spi_clock_hz ? _cfg.spi_clock_hz : 40000000;
  SPI.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  SPI.transfer(CMD_READ);
  SPI.transfer((addr >> 16) & 0xFF);
  SPI.transfer((addr >> 8)  & 0xFF);
  SPI.transfer(addr         & 0xFF);
  for (uint16_t i = 0; i < len; ++i) buf[i] = SPI.transfer(0x00);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

// chunk-safe page program (won't cross 256-byte page boundary)
void FlashLogger::pageProgram(uint32_t addr, const uint8_t* buf, uint32_t len) {
  while (len) {
    uint32_t pageOff = addr & (PAGE_SIZE - 1);
    uint32_t room    = PAGE_SIZE - pageOff;
    uint32_t n       = (len < room) ? len : room;

    writeEnable();
    const uint32_t hz = _cfg.spi_clock_hz ? _cfg.spi_clock_hz : 40000000;
    SPI.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
    digitalWrite(_cs, LOW);
    SPI.transfer(CMD_PP);
    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8)  & 0xFF);
    SPI.transfer(addr         & 0xFF);
    for (uint32_t i = 0; i < n; ++i) SPI.transfer(buf[i]);
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
    waitWhileBusy(20);

    addr += n; buf += n; len -= n;
    yield();
  }
}

void FlashLogger::sectorErase(uint32_t addr, bool countErase) {
  writeEnable();
  const uint32_t hz = _cfg.spi_clock_hz ? _cfg.spi_clock_hz : 40000000;
  SPI.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  SPI.transfer(CMD_SE);
  SPI.transfer((addr >> 16) & 0xFF);
  SPI.transfer((addr >> 8)  & 0xFF);
  SPI.transfer(addr         & 0xFF);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  waitWhileBusy(1000);

  // VERIFY ERASE; quarantine if failed
  if (!verifyErase(addr)) {
    int s = (int)(addr / SECTOR_SIZE);
    Serial.printf("Erase verify FAILED on sector %d -> quarantine\n", s);
    quarantineSector(s);
    return;
  }

  if (countErase) {
    _factory.totalEraseOps++;
    saveFactoryInfo();
  }
}

// ===== sector/header helpers =====
bool FlashLogger::readSectorHeader(int sector, SectorHeader& hdr) {
  if (sector == FACTORY_SECTOR) return false;
  uint32_t base = sectorBaseAddr(sector);
  uint8_t buf[sizeof(SectorHeader)];
  readData(base, buf, sizeof(SectorHeader));
  memcpy(&hdr, buf, sizeof(SectorHeader));
  return (hdr.magic == 0x4C4F4747UL); // 'LOGG'
}

void FlashLogger::writeSectorHeader(int sector, uint16_t dayID, bool pushed) {
  SectorHeader hdr {};
  hdr.magic  = 0x4C4F4747UL;
  hdr.dayID  = dayID;
  hdr.pushed = pushed ? 1 : 0;
  hdr.reserved = 0;
  hdr.generation = _generation;
  pageProgram(sectorBaseAddr(sector), (const uint8_t*)&hdr, sizeof(SectorHeader));
  if (!verifyWrite(sectorBaseAddr(sector), (const uint8_t*)&hdr, sizeof(SectorHeader))) {
    Serial.printf("Header write verify FAILED on sector %d -> quarantine\n", sector);
    quarantineSector(sector);
  }
}

bool FlashLogger::markSectorEraseIntent(int sector) {
  if (sector == FACTORY_SECTOR) return false;
  SectorHeader hdr;
  if (!readSectorHeader(sector, hdr)) return false;
  if (hdr.reserved == HEADER_INTENT_ERASE) return true;
  hdr.reserved = HEADER_INTENT_ERASE;
  pageProgram(sectorBaseAddr(sector), (const uint8_t*)&hdr, sizeof(SectorHeader));
  if (!verifyWrite(sectorBaseAddr(sector), (const uint8_t*)&hdr, sizeof(SectorHeader))) {
    Serial.printf("Erase-intent mark verify FAILED on sector %d\n", sector);
    return false;
  }
  return true;
}

bool FlashLogger::sectorIsEmpty(int sector) {
  if (sector == FACTORY_SECTOR) return false;
  uint8_t b;
  readData(sectorBaseAddr(sector), &b, 1);
  return (b == 0xFF);
}

void FlashLogger::scanAllSectorsBuildIndex() {
  for (int s = 0; s < MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    _index[s] = {false, 0, false, 0};
    SectorHeader hdr;
    if (readSectorHeader(s, hdr)) {
      if (hdr.reserved == HEADER_INTENT_ERASE) {
        Serial.printf("[recovery] pending GC erase on sector %d\n", s);
        sectorErase(sectorBaseAddr(s));
        continue;
      }
      _index[s].present = true;
      _index[s].dayID   = hdr.dayID;
      _index[s].pushed  = (hdr.pushed != 0);
      _index[s].writePtr = sectorBaseAddr(s) + sizeof(SectorHeader); // provisional
    }
  }
}

int FlashLogger::nextRoundRobinStart() {
  for (int attempts = 0; attempts < MAX_SECTORS; ++attempts) {
    uint16_t s = (_factory.startHint + attempts) % (MAX_SECTORS - 1); // [0..MAX-2]
    if (s == FACTORY_SECTOR) continue;
    if (!isBadSector(s)) {
      _factory.startHint = (s + 1) % (MAX_SECTORS - 1);
      saveFactoryInfo();
      return s;
    }
  }
  return 0;
}

void FlashLogger::selectOrCreateTodaySector() {
  int last = -1;
  for (int s = MAX_SECTORS - 1; s >= 0; --s) {
    if (s == FACTORY_SECTOR) continue;
    if (_index[s].present && _index[s].dayID == _currentDay && !isBadSector(s)) {
      last = s; break;
    }
  }
  if (last >= 0) { _currentSector = last; return; }

  int start = nextRoundRobinStart();
  for (int off = 0; off < MAX_SECTORS - 1; ++off) {
    int s = (start + off) % (MAX_SECTORS - 1);
    if (s == FACTORY_SECTOR) continue;
    if (isBadSector(s)) continue;
    if (sectorIsEmpty(s)) {
      sectorErase(sectorBaseAddr(s));
      writeSectorHeader(s, _currentDay, false);
      _index[s] = {true, _currentDay, false, sectorBaseAddr(s) + sizeof(SectorHeader)};
      _currentSector = s;
      return;
    }
    yield();
  }
  _currentSector = -1;
}

// header-aware rebuild to last valid record
void FlashLogger::findLastWritePositionInSector(int sector) {
  uint32_t base = sectorBaseAddr(sector);
  uint32_t ptr  = base + sizeof(SectorHeader);

  while (ptr + sizeof(RecordHeader) < base + SECTOR_SIZE) {
    RecordHeader rh;
    readData(ptr, (uint8_t*)&rh, sizeof(rh));
    if (rh.len == 0xFFFF || rh.len == 0x0000) break;
    if (ptr + sizeof(rh) + rh.len + 1 > base + SECTOR_SIZE) break;

    // commit byte
    uint8_t cm=0;
    readData(ptr + sizeof(rh) + rh.len, &cm, 1);
    if (cm != REC_COMMIT) break;

    // CRC check (no print)
    static uint8_t buf[PAGE_SIZE];
    uint32_t p = ptr + sizeof(rh);
    uint16_t remaining = rh.len;
    uint16_t crc = 0xFFFF;

    while (remaining) {
      uint16_t chunk = min<uint16_t>(remaining, PAGE_SIZE);
      readData(p, buf, chunk);
      crc = crc16(buf, chunk, crc);
      p += chunk;
      remaining -= chunk;
      yield();
    }
    if (crc != rh.crc) break;

    ptr += sizeof(rh) + rh.len + 1;
  }
  _index[sector].writePtr = ptr;
}

bool FlashLogger::sectorHasSpace(int sector, uint32_t needBytes) {
  uint32_t base = sectorBaseAddr(sector);
  uint32_t wp   = _index[sector].writePtr;
  return (wp + needBytes) <= (base + SECTOR_SIZE);
}

bool FlashLogger::moveToNextSectorSameDay() {
  for (int s = _currentSector + 1; s < MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (isBadSector(s)) continue;
    if (sectorIsEmpty(s)) {
      sectorErase(sectorBaseAddr(s));
      writeSectorHeader(s, _currentDay, false);
      _index[s] = {true, _currentDay, false, sectorBaseAddr(s) + sizeof(SectorHeader)};
      _currentSector = s;
      _writeAddr = _index[s].writePtr;
      Serial.printf("Rolled to next sector %d for day %u\n", s, _currentDay);
      return true;
    }
    yield();
  }
  return false;
}

// ===== wear-leveling & quarantine =====
bool FlashLogger::isBadSector(int sector) const {
  for (int i = 0; i < _factory.badCount && i < 16; ++i)
    if (_factory.badList[i] == sector) return true;
  return false;
}

void FlashLogger::quarantineSector(int sector) {
  if (sector <= 0 || sector >= FACTORY_SECTOR) return;
  if (isBadSector(sector)) return;
  if (_factory.badCount < 16) {
    _factory.badList[_factory.badCount++] = sector;
    saveFactoryInfo();
  }
}

// ===== verify ops =====
bool FlashLogger::verifyErase(uint32_t base) {
  uint8_t buf[16];
  readData(base, buf, sizeof(buf));
  for (uint8_t b : buf) { if (b != 0xFF) return false; }
  return true;
}

bool FlashLogger::verifyWrite(uint32_t addr, const uint8_t* buf, uint32_t len) {
  uint8_t tmp[PAGE_SIZE];
  uint32_t off = 0;
  while (off < len) {
    uint32_t n = min<uint32_t>(PAGE_SIZE, len - off);
    readData(addr + off, tmp, n);
    if (memcmp(tmp, buf + off, n) != 0) return false;
    off += n;
    yield();
  }
  return true;
}

// ===== factory info helpers =====
bool FlashLogger::loadFactoryInfo() {
  uint8_t buf[sizeof(FactoryInfo)];
  readData(sectorBaseAddr(FACTORY_SECTOR), buf, sizeof(FactoryInfo));
  memcpy(&_factory, buf, sizeof(FactoryInfo));
  return (_factory.magic == 0x46414354UL);
}

void FlashLogger::saveFactoryInfo() {
  // Ensure the factory sector is clean before writing
  sectorErase(sectorBaseAddr(FACTORY_SECTOR), false); // don’t count toward wear
  pageProgram(sectorBaseAddr(FACTORY_SECTOR), (const uint8_t*)&_factory, sizeof(FactoryInfo));
}


bool FlashLogger::isOlderThanNDays(uint16_t baseDay, uint16_t targetDay, uint16_t n) {
  if (baseDay >= targetDay) return (baseDay - targetDay) >= n;
  return false;
}

// ===== printing helpers =====
void FlashLogger::formatDate(const DateTime& dt, char* out, size_t outLen) const {
  switch (_dateStyle) {
    case DATE_THAI: snprintf(out, outLen, "%02d/%02d/%02d", dt.day(), dt.month(), (dt.year()-2000)); break;
    case DATE_ISO:  snprintf(out, outLen, "%04d-%02d-%02d", dt.year(), dt.month(), dt.day()); break;
    case DATE_US:   snprintf(out, outLen, "%02d/%02d/%02d", dt.month(), dt.day(), (dt.year()-2000)); break;
    default:        snprintf(out, outLen, "%02d/%02d/%02d", dt.day(), dt.month(), (dt.year()-2000)); break;
  }
}

void FlashLogger::printSectorData(int sector) {
  uint32_t base = sectorBaseAddr(sector);
  uint32_t ptr  = base + sizeof(SectorHeader);

  while (ptr + sizeof(RecordHeader) < base + SECTOR_SIZE) {
    RecordHeader rh;
    readData(ptr, (uint8_t*)&rh, sizeof(rh));
    if (rh.len == 0xFFFF || rh.len == 0x0000) break;
    if (ptr + sizeof(rh) + rh.len + 1 > base + SECTOR_SIZE) break;

    // read payload & compute CRC while printing
    uint32_t p = ptr + sizeof(rh);
    uint16_t remaining = rh.len;
    uint16_t crc = 0xFFFF;
    static uint8_t buf[PAGE_SIZE];

    while (remaining) {
      uint16_t chunk = min<uint16_t>(remaining, PAGE_SIZE);
      readData(p, buf, chunk);
      crc = crc16(buf, chunk, crc);
      for (int i=0;i<chunk;i++) Serial.write(buf[i]); // prints newline too
      p += chunk;
      remaining -= chunk;
      yield();
    }

    uint8_t cm=0;
    readData(ptr + sizeof(rh) + rh.len, &cm, 1);
    if (cm != REC_COMMIT || crc != rh.crc) {
      Serial.println(F("[corrupt/partial record skipped]"));
      break;
    }

    ptr += sizeof(rh) + rh.len + 1;
  }
}

// ===== capacity / stats =====
uint32_t FlashLogger::countUsedSectors() const {
  uint32_t used = 0;
  for (int s = 0; s < MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (_index[s].present) used++;
  }
  return used;
}
float FlashLogger::getFreeSpaceMB() {
  float totalMB = (MAX_SECTORS - 1) * (SECTOR_SIZE / 1024.0f / 1024.0f);
  float usedMB  = countUsedSectors() * (SECTOR_SIZE / 1024.0f / 1024.0f);
  return max(0.0f, totalMB - usedMB);
}
float FlashLogger::getUsedSpaceMB() {
  return countUsedSectors() * (SECTOR_SIZE / 1024.0f / 1024.0f);
}
float FlashLogger::getUsedPercent() {
  float totalMB = (MAX_SECTORS - 1) * (SECTOR_SIZE / 1024.0f / 1024.0f);
  float usedMB  = getUsedSpaceMB();
  if (totalMB <= 0.0f) return 0.0f;
  return (usedMB / totalMB) * 100.0f;
}
float FlashLogger::getFlashHealth() {
  const float kCycles = 100000.0f;
  float avgCycles = (_factory.totalEraseOps) / max(1.0f, (float)(MAX_SECTORS - 1));
  float health = 1.0f - (avgCycles / kCycles);
  if (health < 0) health = 0;
  return health * 100.0f;
}
uint16_t FlashLogger::estimateDaysRemaining(float avgBytesPerDay) {
  if (avgBytesPerDay <= 0.0f) return 0;
  float freeBytes = getFreeSpaceMB() * 1024.0f * 1024.0f;
  float days = freeBytes / avgBytesPerDay;
  if (days < 0) days = 0;
  if (days > 65535) days = 65535;
  return (uint16_t)days;
}
FlashStats FlashLogger::getFlashStats(float avgBytesPerDay) {
  FlashStats s{};
  s.totalMB = (MAX_SECTORS - 1) * (SECTOR_SIZE / 1024.0f / 1024.0f);
  s.usedMB  = getUsedSpaceMB();
  s.freeMB  = max(0.0f, s.totalMB - s.usedMB);
  s.usedPercent = (s.totalMB > 0) ? (s.usedMB / s.totalMB) * 100.0f : 0.0f;
  s.healthPercent = getFlashHealth();
  s.estimatedDaysLeft = estimateDaysRemaining(avgBytesPerDay);
  return s;
}

// ===== factory info public =====
bool FlashLogger::setFactoryInfo(const String& model, const String& flashModel, const String& deviceID) {
  bool changed = false;
  if (_factory.magic != 0x46414354UL) return false;

  if (model.length())      { strncpy(_factory.model, model.c_str(), sizeof(_factory.model)-1); changed = true; }
  if (flashModel.length()) { strncpy(_factory.flashModel, flashModel.c_str(), sizeof(_factory.flashModel)-1); changed = true; }
  if (deviceID.length())   { strncpy(_factory.deviceId, deviceID.c_str(), sizeof(_factory.deviceId)-1); changed = true; }

  if (_factory.firstDayID == 0) { _factory.firstDayID = dayIDFromDateTime(_rtc->now()); changed = true; }
  _factory.defaultDateStyle = (uint8_t)_dateStyle;

  if (changed) saveFactoryInfo();
  return true;
}

void FlashLogger::printFactoryInfo() {
  Serial.println("=== Factory Info ===");
  Serial.printf("Model        : %s\n", _factory.model);
  Serial.printf("Flash Model  : %s\n", _factory.flashModel);
  Serial.printf("Device ID    : %s\n", _factory.deviceId);
  Serial.printf("First DayID  : %u\n", _factory.firstDayID);
  Serial.printf("Default Style: %u\n", (unsigned)_factory.defaultDateStyle);
  Serial.printf("Erase Ops    : %lu\n", (unsigned long)_factory.totalEraseOps);
  Serial.printf("Boot Count   : %lu\n", (unsigned long)_factory.bootCounter);
  Serial.printf("Bad Sectors  : %u\n", (unsigned)_factory.badCount);
  Serial.println("====================");
}

bool FlashLogger::factoryReset(const String& code12) {
  if (code12.length() != 12) return false;
  for (uint8_t c : code12) if (c < '0' || c > '9') return false;
  if (code12 != "847291506314") return false;

  Serial.println("FACTORY RESET: erasing all data sectors...");
  for (int s = 0; s < MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (_index[s].present || sectorIsEmpty(s) == false) {
      sectorErase(sectorBaseAddr(s)); // counts erase, verifies; may quarantine
      _index[s] = {false, 0, false, 0};
    }
    yield();
  }
  _currentDay = dayIDFromDateTime(_rtc->now());
  selectOrCreateTodaySector();
  if (_currentSector >= 0) {
    findLastWritePositionInSector(_currentSector);
    _writeAddr = _index[_currentSector].writePtr;
  }
  Serial.println("FACTORY RESET: done.");
  return true;
}

// ===== back-pressure =====
void FlashLogger::setMaxDailyBytes(uint32_t bytes) { _maxDailyBytes = bytes; }
bool FlashLogger::isLowSpace() const { return _lowSpace; }

// ===== Sorting newest-first by dayID =====
int FlashLogger::cmpDayDesc(const void* a, const void* b) {
  const DaySummary* A = (const DaySummary*)a;
  const DaySummary* B = (const DaySummary*)b;
  if (A->dayID == B->dayID) return 0;
  return (A->dayID > B->dayID) ? -1 : 1;
}

// ===== DayID <-> date helpers =====
void FlashLogger::formatDayID(uint16_t dayID, char* out, size_t outLen) const {
  DateTime now = _rtc->now();
  int32_t delta = (int32_t)dayIDFromDateTime(now) - (int32_t)dayID;
  DateTime dt = now - TimeSpan(delta * 86400L);
  formatDate(dt, out, outLen);
}
bool FlashLogger::parseDateYYYYMMDD(const String& s, uint16_t& outDayID) {
  if (s.length() != 10 || s.charAt(4)!='-' || s.charAt(7)!='-') return false;
  int y = s.substring(0,4).toInt();
  int m = s.substring(5,7).toInt();
  int d = s.substring(8,10).toInt();
  if (y < 2000 || y > 2099 || m<1 || m>12 || d<1 || d>31) return false;
  DateTime dt(y,m,d,0,0,0);
  outDayID = (uint16_t)(dt.secondstime()/86400UL);
  return true;
}

// ===== valid-bytes scan (header-aware) =====
uint32_t FlashLogger::computeValidBytesInSector(int sector, uint32_t& firstTs, uint32_t& lastTs) {
  firstTs = 0; lastTs = 0;
  uint32_t base = sectorBaseAddr(sector);
  uint32_t ptr  = base + sizeof(SectorHeader);
  uint32_t bytes = 0;

  while (ptr + sizeof(RecordHeader) < base + SECTOR_SIZE) {
    RecordHeader rh;
    readData(ptr, (uint8_t*)&rh, sizeof(rh));
    if (rh.len == 0xFFFF || rh.len == 0x0000) break;
    if (ptr + sizeof(rh) + rh.len + 1 > base + SECTOR_SIZE) break;
    uint8_t cm=0;
    readData(ptr + sizeof(rh) + rh.len, &cm, 1);
    if (cm != REC_COMMIT) break;

    if (!firstTs) firstTs = rh.ts;
    lastTs = rh.ts;

    uint32_t rec = sizeof(RecordHeader) + rh.len + 1;
    bytes += rec;
    ptr   += rec;
    yield();
  }
  return bytes;
}

// ===== sector/day summaries =====
void FlashLogger::summarizeSector(int sector, SectorSummary& out) {
  out = {};
  SectorHeader hdr;
  if (!readSectorHeader(sector, hdr)) return;
  out.sector = sector;
  out.dayID  = hdr.dayID;
  out.pushed = (hdr.pushed != 0);
  out.bytes  = computeValidBytesInSector(sector, out.firstTs, out.lastTs);
}
void FlashLogger::summarizeDay(uint16_t dayID, DaySummary& out) {
  out = {};
  out.dayID = dayID;
  out.pushed = true;
  for (int s=0; s<MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present) continue;
    if (_index[s].dayID != dayID) continue;
    SectorSummary ss; summarizeSector(s, ss);
    out.sectors++;
    out.bytes += ss.bytes;
    if (!ss.pushed) out.pushed = false;
    if (!out.firstTs || (ss.firstTs && ss.firstTs < out.firstTs)) out.firstTs = ss.firstTs;
    if (ss.lastTs && ss.lastTs > out.lastTs) out.lastTs = ss.lastTs;
    yield();
  }
}

// ===== build cached day list =====
void FlashLogger::buildSummaries() {
  _dayCount = 0;
  uint16_t seen[MAX_DAYS_CACHE]; int seenN = 0;

  for (int s=0; s<MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present) continue;
    uint16_t d = _index[s].dayID;
    bool dup=false; for(int i=0;i<seenN;i++){ if (seen[i]==d){ dup=true; break; } }
    if (!dup && seenN < MAX_DAYS_CACHE) seen[seenN++] = d;
    yield();
  }
  for (int i=0;i<seenN;i++) {
    if (_dayCount >= MAX_DAYS_CACHE) break;
    summarizeDay(seen[i], _days[_dayCount++]);
    yield();
  }
  if (_dayCount > 1) qsort(_days, _dayCount, sizeof(DaySummary), cmpDayDesc);

  // refresh sector cache if a day is selected
  if (_selKind == SEL_DAY) listSectors(_selDay);
}

// ===== listings =====
void FlashLogger::listDays() {
  buildSummaries();
  Serial.println(F("\n#  DATE        DAYID  SECT  BYTES     STATUS  RANGE"));
  for (int i=0;i<_dayCount;i++) {
    char dateBuf[20]; formatDayID(_days[i].dayID, dateBuf, sizeof(dateBuf));
    const char* st = _days[i].pushed ? "PUSHED" : "OPEN";
    char rng[32]; snprintf(rng,sizeof(rng), "%lu..%lu", (unsigned long)_days[i].firstTs, (unsigned long)_days[i].lastTs);
    Serial.printf("%-2d %-10s  %-5u  %-4u  %-8lu  %-6s  %s\n",
      i, dateBuf, _days[i].dayID, _days[i].sectors, (unsigned long)_days[i].bytes, st, rng);
    yield();
  }
  if (_dayCount==0) Serial.println("(no days)");
}
void FlashLogger::listSectors(uint16_t dayID) {
  _sectCount = 0;
  for (int s=0; s<MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present) continue;
    if (_index[s].dayID != dayID) continue;
    if (_sectCount >= MAX_SECT_CACHE) break;
    summarizeSector(s, _sects[_sectCount++]);
    yield();
  }
  Serial.printf("\nSectors for day %u:\n", dayID);
  Serial.println(F("#  SECTOR  BYTES     STATUS   TS_RANGE"));
  for (int i=0;i<_sectCount;i++) {
    const char* st = _sects[i].pushed ? "PUSHED" : "OPEN";
    char rng[32]; snprintf(rng,sizeof(rng), "%lu..%lu", (unsigned long)_sects[i].firstTs, (unsigned long)_sects[i].lastTs);
    Serial.printf("%-2d %-6d  %-8lu %-7s  %s\n", i, _sects[i].sector, (unsigned long)_sects[i].bytes, st, rng);
    yield();
  }
  if (_sectCount==0) Serial.println("(no sectors)");
}

// ===== selection & print =====
bool FlashLogger::dayIndexToDayID(int idx, uint16_t& out) const {
  if (idx < 0 || idx >= _dayCount) return false; out = _days[idx].dayID; return true;
}
bool FlashLogger::sectorIndexToSector(int idx, int& out) const {
  if (idx < 0 || idx >= _sectCount) return false; out = _sects[idx].sector; return true;
}
bool FlashLogger::selectDay(uint16_t dayID) {
  _selKind = SEL_DAY; _selDay = dayID; _selSector = -1;
  listSectors(dayID);
  return true;
}
bool FlashLogger::selectSector(int sector) {
  if (_selKind == SEL_DAY) {
    SectorHeader hdr; if (!readSectorHeader(sector, hdr)) return false;
    if (hdr.dayID != _selDay) return false;
  }
  _selKind = SEL_SECTOR; _selSector = sector;
  return true;
}
void FlashLogger::printSelectedInfo() {
  if (_selKind == SEL_DAY) {
    DaySummary d; summarizeDay(_selDay, d);
    char dateBuf[20]; formatDayID(d.dayID, dateBuf, sizeof(dateBuf));
    Serial.printf("[DAY] %s  dayID=%u  sectors=%u  bytes=%lu  status=%s\n",
      dateBuf, d.dayID, d.sectors, (unsigned long)d.bytes, d.pushed?"PUSHED":"OPEN");
  } else if (_selKind == SEL_SECTOR) {
    SectorSummary s; summarizeSector(_selSector, s);
    Serial.printf("[SECTOR] #%d  dayID=%u  bytes=%lu  status=%s  ts=%lu..%lu\n",
      s.sector, s.dayID, (unsigned long)s.bytes, s.pushed?"PUSHED":"OPEN",
      (unsigned long)s.firstTs, (unsigned long)s.lastTs);
  } else {
    Serial.println("(nothing selected)");
  }
}
void FlashLogger::printSelected() {
  if (_selKind == SEL_DAY) {
    uint16_t day = _selDay;
    Serial.println("----------------------------------");
    char dateBuf[20]; formatDayID(day, dateBuf, sizeof(dateBuf));
    Serial.printf("date %s\n{\n", dateBuf);
    for (int s=0; s<MAX_SECTORS; ++s) {
      if (s == FACTORY_SECTOR) continue;
      if (!_index[s].present) continue;
      if (_index[s].dayID != day) continue;
      printSectorData(s);
      yield();
    }
    Serial.println("}\n----------------------------------");
  } else if (_selKind == SEL_SECTOR) {
    Serial.printf("\n[PRINT sector %d]\n", _selSector);
    printSectorData(_selSector);
    Serial.println("----------------------------------");
  } else {
    Serial.println("(nothing selected)");
  }
}
bool FlashLogger::handleCommand(const String& cmdIn, Stream& io) {
  String cmd = cmdIn; cmd.trim();
  if (!cmd.length()) return false;

  if (cmd.equalsIgnoreCase("help")) {
    io.println("Commands:");
    io.println("  help                                  Show this summary");
    io.println("  ls                                    List known days");
    io.println("  ls sectors [day]                      List sectors for a day");
    io.println("  cd day <date|id|#n>                   Select day");
    io.println("  cd sector <id|#n>                     Select sector");
    io.println("  print / info                          Dump selected data/info");
    io.println("  q latest <N> [token=...]              Query latest records");
    io.println("  q day <YYYY-MM-DD> [token=...]");
    io.println("  q range <YYYY-MM-DD..YYYY-MM-DD> [token=...]");
    io.println("  export <N> [token=...]                Stream from cursor (auto-saves)");
    io.println("  cursor show|clear|set|save|load       Manage cursor state");
    io.println("  fmt csv|jsonl                         Set output format");
    io.println("  set csv <cols>                        Configure CSV columns");
    io.println("  pf | stats | factory | gc             Format, stats, maintenance");
    io.println("  reset <code>                          Factory reset logs");
    io.println("  scanbad                               Scan/quarantine bad sectors");
    return true;
  }

  // reset (factory wipe + reinit)
  if (cmd.startsWith("reset")) {
    String arg = cmd.substring(5); arg.trim();
    if (!arg.length()) { io.println("⚠️  reset <12digit_code> (erases all logs)"); return true; }
    if (arg.length()!=12) { io.println("reset: code must be 12 digits"); return true; }
    for (uint8_t i=0;i<12;i++) if (arg[i]<'0'||arg[i]>'9'){ io.println("reset: numeric only"); return true; }
    if (factoryReset(arg)) { io.println("✅ reset OK, reinit..."); reinitAfterFactoryReset(); printFactoryInfo(); listDays(); }
    else io.println("❌ reset failed");
    return true;
  }

  // ls (days)
  if (cmd.equalsIgnoreCase("ls")) { listDays(); return true; }

  // ls sectors [dayID|YYYY-MM-DD]
  if (cmd.startsWith("ls sectors")) {
    uint16_t dayID = 0;
    int p = cmd.indexOf(' ', 3); p = (p>=0)?cmd.indexOf(' ', p+1):-1;
    if (p>0) {
      String a = cmd.substring(p+1); a.trim();
      if (a.length()==10 && a[4]=='-' && a[7]=='-') { if (!parseDateYYYYMMDD(a, dayID)) dayID=0; }
      else if (a.length()) dayID = (uint16_t)a.toInt();
    }
    if (!dayID) { buildSummaries(); if (!_dayCount){ io.println("(no days)"); return true; } dayID=_days[0].dayID; }
    listSectors(dayID); return true;
  }

  // cd day <YYYY-MM-DD | dayID | #index>
  if (cmd.startsWith("cd day")) {
    String a = cmd.substring(6); a.trim(); if (!a.length()){ io.println("usage: cd day <YYYY-MM-DD|dayID|#index>"); return true; }
    uint16_t dayID=0;
    if (a.startsWith("#")) { int idx=a.substring(1).toInt(); buildSummaries(); if (!dayIndexToDayID(idx, dayID)){ io.println("invalid day index"); return true; } }
    else if (a.length()==10 && a[4]=='-' && a[7]=='-') { if (!parseDateYYYYMMDD(a, dayID)){ io.println("bad date"); return true; } }
    else { dayID=(uint16_t)a.toInt(); if (!dayID){ buildSummaries(); if (!_dayCount){ io.println("(no days)"); return true; } dayID=_days[0].dayID; } }
    selectDay(dayID); printSelectedInfo(); return true;
  }

  // cd sector <sectorID | #index>
  if (cmd.startsWith("cd sector")) {
    String a = cmd.substring(9); a.trim(); if (!a.length()){ io.println("usage: cd sector <sectorID|#index>"); return true; }
    int sector = a.startsWith("#") ? (sectorIndexToSector(a.substring(1).toInt(), sector), sector) : a.toInt();
    if (sector<0){ io.println("invalid sector id"); return true; }
    if (selectSector(sector)) printSelectedInfo(); else io.println("sector not in selected day");
    return true;
  }

  // print/info
  if (cmd.equalsIgnoreCase("print")) { printSelected(); return true; }
  if (cmd.equalsIgnoreCase("info"))  { printSelectedInfo(); return true; }

  // queries / format / csv columns
  if (cmd.startsWith("q "))        { if (handleQueryCommand(cmd, io)) return true; }
  if (cmd.startsWith("fmt "))      { String a=cmd.substring(4); a.trim();
                                     if (a.equalsIgnoreCase("csv"))   { setOutputFormat(OUT_CSV);   io.println("format: CSV"); return true; }
                                     if (a.equalsIgnoreCase("jsonl")) { setOutputFormat(OUT_JSONL); io.println("format: JSONL"); return true; }
                                     io.println("fmt csv|jsonl"); return true; }
  if (cmd.startsWith("set csv "))  { String cols=cmd.substring(8); cols.trim(); setCsvColumns(cols.c_str());
                                     io.print("csv columns: "); io.println(getCsvColumns()); return true; }

  // cursor / export
  if (cmd.startsWith("cursor") || cmd.startsWith("export")) {
    if (handleCursorCommand(cmd, io)) return true;
  }

  // built-in extras
  if (cmd.equalsIgnoreCase("pf"))     { printFormattedLogs(); return true; }
  if (cmd.equalsIgnoreCase("stats"))  { auto fs=getFlashStats(3500.0f);
    io.printf("Total: %.2f MB  Used: %.2f MB  Free: %.2f MB  Used: %.1f%%  Health: %.1f%%  EstDays: %u\n",
              fs.totalMB, fs.usedMB, fs.freeMB, fs.usedPercent, fs.healthPercent, fs.estimatedDaysLeft); return true; }
  if (cmd.equalsIgnoreCase("factory")){ printFactoryInfo(); return true; }
  if (cmd.equalsIgnoreCase("gc"))     { gc(); return true; }

    // cursor save [ns key]
  if (cmd.startsWith("cursor save")) {
    String rest = cmd.substring(11); rest.trim();
    String ns="flog", key="cursor";
    if (rest.length()) {
      int sp = rest.indexOf(' ');
      if (sp<0) key = rest; else { ns = rest.substring(0,sp); key = rest.substring(sp+1); ns.trim(); key.trim(); }
    }
    bool ok = saveCursorNVS(ns.c_str(), key.c_str());
    io.println(ok ? "cursor: saved" : "cursor: save failed");
    return true;
  }
  // cursor load [ns key]
  if (cmd.startsWith("cursor load")) {
    String rest = cmd.substring(11); rest.trim();
    String ns="flog", key="cursor";
    if (rest.length()) {
      int sp = rest.indexOf(' ');
      if (sp<0) key = rest; else { ns = rest.substring(0,sp); key = rest.substring(sp+1); ns.trim(); key.trim(); }
    }
    bool ok = loadCursorNVS(ns.c_str(), key.c_str());
    io.println(ok ? "cursor: loaded" : "cursor: load failed");
    return true;
  }

  if (cmd.equalsIgnoreCase("scanbad")) { scanBadAndQuarantine(io); return true; }

  return false; // let caller handle anything else
}



//1.92

bool FlashLogger::recordMatchesTime(uint16_t recDay, uint32_t ts, const QuerySpec& q) {
  // Day range overrides ts range if set
  if (q.day_from || q.day_to) {
    if (q.day_from && recDay < q.day_from) return false;
    if (q.day_to   && recDay > q.day_to)   return false;
    return true;
  }
  if (ts < q.ts_from || ts > q.ts_to) return false;
  return true;
}
bool FlashLogger::jsonExtractKeyValue(const char* json, const char* key, String& outVal) const {
  // Find "key":
  char pat[32];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char* p = strstr(json, pat);
  if (!p) return false;
  p += strlen(pat);
  while (*p && (*p==' ' || *p=='\t' || *p==':')) p++;
  if (!*p) return false;

  // Number or string
  if (*p == '\"') {
    const char* q = ++p;
    while (*q && *q != '\"') q++;
    outVal = String("\"") + String(p, q-p) + String("\"");
    return true;
  } else {
    const char* q = p;
    while (*q && *q!='\n' && *q!='\r' && *q!=',' && *q!='}') q++;
    outVal = String(p, q-p);
    outVal.trim();
    return outVal.length() > 0;
  }
}

void FlashLogger::buildCsvLine(uint32_t ts, const char* payload, uint16_t len, const char* cols, String& out) const {
  out = "";
  // simple CSV: split cols by comma
  String colsS(cols);
  colsS.trim();
  int start = 0;
  bool first = true;
  while (start < colsS.length()) {
    int comma = colsS.indexOf(',', start);
    String col = (comma >= 0) ? colsS.substring(start, comma) : colsS.substring(start);
    col.trim();
    String val;
    if (col.equalsIgnoreCase("ts")) {
      val = String(ts);
    } else {
      // pull from JSON
      jsonExtractKeyValue(payload, col.c_str(), val);
      // remove quotes around strings for CSV cleanliness
      if (val.startsWith("\"") && val.endsWith("\"")) {
        val = val.substring(1, val.length()-1);
      }
    }
    if (!first) out += ",";
    out += val.length() ? val : "";
    first = false;
    if (comma < 0) break;
    start = comma + 1;
  }
}

void FlashLogger::buildJsonFiltered(const char* payload, uint16_t len,
                                    const char* const* keys, bool compact, String& out) {
  if (!keys || !keys[0]) {
    // no filter → pass-through (ensure newline)
    out = String(payload, len);
    if (out.length()==0 || out[out.length()-1] != '\n') out += '\n';
    return;
  }
  // build {"k1":v1,"k2":v2,...}
  out = "{";
  bool first = true;
  for (int i=0; i<8 && keys[i]; ++i) {
    String v;
    if (jsonExtractKeyValue(payload, keys[i], v)) {
      if (!first) out += ",";
      out += "\""; out += keys[i]; out += "\":";
      out += v;
      first = false;
    }
  }
  out += "}\n";
  if (!compact) {
    // (optional pretty — keeping compact for now)
  }
}

bool FlashLogger::formatPayload(uint32_t ts, const String& payload, OutFmt fmt, String& out) const {
  if (fmt == OUT_CSV) {
    buildCsvLine(ts, payload.c_str(), payload.length(), _csvCols, out);
    if (out.length()==0 || out[out.length()-1] != '\n') out += '\n';
    return true;
  }
  out = payload;
  if (out.length()==0 || out[out.length()-1] != '\n') out += '\n';
  return true;
}

bool FlashLogger::parsePredicateExpr(const String& token, FieldPredicate& out) const {
  String expr = token;
  expr.trim();
  if (!expr.length()) return false;

  static const char* OPS[] = {"<=", ">=", "!=", "<", ">", "="};
  static const PredicateOp OP_MAP[] = {PRED_LE, PRED_GE, PRED_NE, PRED_LT, PRED_GT, PRED_EQ};

  for (int i = 0; i < 6; ++i) {
    int pos = expr.indexOf(OPS[i]);
    if (pos <= 0) continue;
    String key = expr.substring(0, pos);
    String rhs = expr.substring(pos + strlen(OPS[i]));
    key.trim(); rhs.trim();
    if (!key.length() || !rhs.length()) return false;
    if (key.length() >= (int)sizeof(out.key)) return false;
    bool numeric = true;
    for (size_t j=0;j<rhs.length();++j) {
      char c = rhs[j];
      if (!(isdigit((unsigned char)c) || c=='-' || c=='+' || c=='.')) { numeric = false; break; }
    }
    if (!numeric) return false;
    key.toCharArray(out.key, sizeof(out.key));
    out.op = OP_MAP[i];
    out.value = rhs.toFloat();
    return true;
  }
  return false;
}

bool FlashLogger::addPredicateFromToken(QuerySpec& q, const String& token, Stream* err) const {
  FieldPredicate pred{};
  if (!parsePredicateExpr(token, pred)) {
    if (err) err->printf("bad predicate: %s\n", token.c_str());
    return false;
  }
  if (q.predicateCount >= QuerySpec::MAX_PREDICATES) {
    if (err) err->println("predicate limit reached");
    return false;
  }
  q.predicates[q.predicateCount++] = pred;
  return true;
}

bool FlashLogger::recordMatchesPredicates(const char* payload, uint16_t len, const QuerySpec& q) const {
  if (q.predicateCount == 0) return true;
  String value;
  constexpr float kFloatEps = 0.0001f;
  for (uint8_t i = 0; i < q.predicateCount; ++i) {
    const FieldPredicate& pred = q.predicates[i];
    if (!jsonExtractKeyValue(payload, pred.key, value)) return false;
    value.trim();
    if (!value.length()) return false;
    bool numeric = true;
    for (size_t j=0;j<value.length();++j) {
      char c = value[j];
      if (!(isdigit((unsigned char)c) || c=='-' || c=='+' || c=='.')) { numeric = false; break; }
    }
    if (!numeric) return false;
    float actual = value.toFloat();
    switch (pred.op) {
      case PRED_LT: if (!(actual < pred.value)) return false; break;
      case PRED_LE: if (!(actual <= pred.value + kFloatEps)) return false; break;
      case PRED_GT: if (!(actual > pred.value)) return false; break;
      case PRED_GE: if (!(actual + kFloatEps >= pred.value)) return false; break;
      case PRED_EQ: if (fabsf(actual - pred.value) > kFloatEps) return false; break;
      case PRED_NE: if (fabsf(actual - pred.value) <= kFloatEps) return false; break;
    }
  }
  return true;
}

bool FlashLogger::buildPageToken(int sector, uint32_t addr, uint16_t dayID, uint8_t dir, String& out) const {
  uint8_t raw[14];
  raw[0] = 'P'; raw[1] = 'T'; raw[2] = '1'; raw[3] = dir;
  raw[4] = (uint8_t)(dayID & 0xFF);
  raw[5] = (uint8_t)((dayID >> 8) & 0xFF);
  int16_t s = (int16_t)sector;
  raw[6] = (uint8_t)(s & 0xFF);
  raw[7] = (uint8_t)((s >> 8) & 0xFF);
  raw[8]  = (uint8_t)(addr & 0xFF);
  raw[9]  = (uint8_t)((addr >> 8)  & 0xFF);
  raw[10] = (uint8_t)((addr >> 16) & 0xFF);
  raw[11] = (uint8_t)((addr >> 24) & 0xFF);
  uint16_t crc = crc16(raw, 12, 0xFFFF);
  raw[12] = (uint8_t)(crc & 0xFF);
  raw[13] = (uint8_t)((crc >> 8) & 0xFF);

  static const char kHexDigits[] = "0123456789ABCDEF";
  out = "";
  out.reserve(28);
  for (int i = 0; i < 14; ++i) {
    out += kHexDigits[raw[i] >> 4];
    out += kHexDigits[raw[i] & 0x0F];
  }
  return true;
}

bool FlashLogger::parsePageToken(const String& token, int& sector, uint32_t& addr,
                                 uint16_t& dayID, uint8_t& dir) const {
  String t = token;
  t.trim();
  if (t.length() != 28) return false;
  uint8_t raw[14];
  for (int i = 0; i < 14; ++i) {
    int hi = hexNibble(t[2*i]);
    int lo = hexNibble(t[2*i + 1]);
    if (hi < 0 || lo < 0) return false;
    raw[i] = (uint8_t)((hi << 4) | lo);
  }
  if (raw[0] != 'P' || raw[1] != 'T' || raw[2] != '1') return false;
  uint16_t crc = crc16(raw, 12, 0xFFFF);
  if ((uint8_t)(crc & 0xFF) != raw[12] || (uint8_t)((crc >> 8) & 0xFF) != raw[13]) return false;
  dir = raw[3];
  dayID = (uint16_t)raw[4] | ((uint16_t)raw[5] << 8);
  sector = (int16_t)((uint16_t)raw[6] | ((uint16_t)raw[7] << 8));
  addr = (uint32_t)raw[8] | ((uint32_t)raw[9] << 8) | ((uint32_t)raw[10] << 16) | ((uint32_t)raw[11] << 24);
  return true;
}
bool FlashLogger::emitRecord(uint16_t recDay, uint32_t ts, const uint8_t* payload, uint16_t len,
                             const QuerySpec& q, RowCallback onRow, void* user) {
  if (!onRow) return false;
  static String line;
  if (q.out == OUT_JSONL) {
    buildJsonFiltered((const char*)payload, len, q.includeKeys, q.compact_json, line);
  } else { // CSV
    buildCsvLine(ts, (const char*)payload, len, _csvCols, line);
    line += "\n";
  }
  onRow(line.c_str(), user);
  return true;
}
uint32_t FlashLogger::queryLogs(const QuerySpec& q, RowCallback onRow, void* user,
                                const String* pageToken, String* nextToken) {
  if (!onRow) return 0;
  if (nextToken) *nextToken = "";

  bool resume = false;
  int resumeSector = 0;
  uint32_t resumeAddr = 0;
  if (pageToken && pageToken->length()) {
    int tokSector; uint32_t tokAddr; uint16_t tokDay; uint8_t tokDir;
    if (parsePageToken(*pageToken, tokSector, tokAddr, tokDay, tokDir) &&
        tokDir == PAGE_DIR_FWD && tokSector >= 0 && tokSector < MAX_SECTORS &&
        tokSector != FACTORY_SECTOR) {
      resume = true;
      resumeSector = tokSector;
      resumeAddr = tokAddr;
    }
  }

  uint32_t emitted = 0;
  uint32_t sample  = 0;

  if (_anchorCount == 0) buildAnchors();

  auto locateNextForward = [&](int curSector, uint32_t nextPtr, int& outSector, uint32_t& outAddr) -> bool {
    if (curSector >= 0) {
      uint32_t base = sectorBaseAddr(curSector);
      if (nextPtr < base + SECTOR_SIZE && isValidRecordAt(nextPtr)) {
        outSector = curSector;
        outAddr = nextPtr;
        return true;
      }
    }
    int start = (curSector >= 0) ? curSector + 1 : 0;
    for (int s = start; s < MAX_SECTORS; ++s) {
      if (s == FACTORY_SECTOR) continue;
      if (!_index[s].present) continue;
      uint32_t firstAddr;
      if (findFirstRecord(s, firstAddr)) {
        outSector = s;
        outAddr = firstAddr;
        return true;
      }
    }
    return false;
  };

  for (int s = 0; s < MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present)  continue;
    if (resume && s < resumeSector) continue;

    if (!sectorMaybeInRangeByAnchor(s, q.day_from, q.day_to, q.ts_from, q.ts_to)) continue;

    const uint32_t base = sectorBaseAddr(s);
    uint32_t ptr = base + sizeof(SectorHeader);
    if (resume && s == resumeSector) {
      if (isValidRecordAt(resumeAddr)) {
        ptr = resumeAddr;
      } else {
        if (!findFirstRecord(s, ptr)) { resume = false; continue; }
      }
      resume = false;
    }

    while (ptr + sizeof(RecordHeader) < base + SECTOR_SIZE) {
      RecordHeader rh; uint16_t recDay;
      if (!readRecordMeta(ptr, rh, recDay)) break;

      uint32_t nextPtr = ptr + sizeof(rh) + rh.len + 1;

      if (recordMatchesTime(recDay, rh.ts, q)) {
        static uint8_t buf[PAGE_SIZE];
        String payload; payload.reserve(rh.len + 8);
        uint32_t p = ptr + sizeof(rh);
        uint16_t remaining = rh.len;
        while (remaining) {
          uint16_t chunk = remaining > PAGE_SIZE ? PAGE_SIZE : remaining;
          readData(p, buf, chunk);
          payload += String((const char*)buf, chunk);
          p += chunk; remaining -= chunk;
          yield();
        }
        if (!recordMatchesPredicates(payload.c_str(), payload.length(), q)) {
          ptr = nextPtr;
          continue;
        }
        if (q.sample_every <= 1 || (sample++ % q.sample_every == 0)) {
          if (emitRecord(recDay, rh.ts, (const uint8_t*)payload.c_str(), rh.len, q, onRow, user)) {
            emitted++;
            if (q.max_records && emitted >= q.max_records) {
              if (nextToken) {
                int tokenSector;
                uint32_t tokenAddr;
                if (locateNextForward(s, nextPtr, tokenSector, tokenAddr)) {
                  uint16_t tokenDay = recDay;
                  if (tokenSector >= 0 && tokenSector < MAX_SECTORS && _index[tokenSector].present) {
                    tokenDay = _index[tokenSector].dayID;
                  }
                  buildPageToken(tokenSector, tokenAddr, tokenDay, PAGE_DIR_FWD, *nextToken);
                } else {
                  *nextToken = "";
                }
              }
              return emitted;
            }
          }
        }
      }

      ptr = nextPtr;
      yield();
    }
  }
  return emitted;
}

uint32_t FlashLogger::queryLatest(uint32_t N, RowCallback onRow, void* user,
                                  const String* pageToken, String* nextToken) {
  if (!onRow || N == 0) return 0;
  if (nextToken) *nextToken = "";

  bool resume = false;
  int resumeSector = -1;
  uint32_t resumeAddr = 0;
  if (pageToken && pageToken->length()) {
    int tokSector; uint32_t tokAddr; uint16_t tokDay; uint8_t tokDir;
    if (parsePageToken(*pageToken, tokSector, tokAddr, tokDay, tokDir) &&
        tokDir == PAGE_DIR_REV && tokSector >= 0 && tokSector < MAX_SECTORS &&
        tokSector != FACTORY_SECTOR) {
      resume = true;
      resumeSector = tokSector;
      resumeAddr = tokAddr;
    }
  }

  QuerySpec fmt;
  fmt.out = _outFmt;
  fmt.compact_json = true;

  uint32_t emitted = 0;

  for (int s = MAX_SECTORS - 1; s >= 0 && emitted < N; --s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present) continue;
    if (resume && s > resumeSector) continue;

    uint32_t addr;
    if (resume && s == resumeSector) {
      if (isValidRecordAt(resumeAddr)) {
        addr = resumeAddr;
      } else {
        if (!findLastRecord(s, addr)) { resume = false; continue; }
      }
      resume = false;
    } else {
      if (!findLastRecord(s, addr)) continue;
    }

    while (emitted < N) {
      RecordHeader rh; uint16_t recDay;
      if (!readRecordMeta(addr, rh, recDay)) break;

      static uint8_t buf[PAGE_SIZE];
      String payload; payload.reserve(rh.len + 8);
      uint32_t p = addr + sizeof(rh);
      uint16_t remaining = rh.len;
      while (remaining) {
        uint16_t chunk = remaining > PAGE_SIZE ? PAGE_SIZE : remaining;
        readData(p, buf, chunk);
        payload += String((const char*)buf, chunk);
        p += chunk;
        remaining -= chunk;
        yield();
      }

      if (emitRecord(recDay, rh.ts, (const uint8_t*)payload.c_str(), rh.len, fmt, onRow, user)) {
        ++emitted;
        if (emitted >= N) {
          if (nextToken) {
            int tokenSector = -1;
            uint32_t tokenAddr = 0;
            bool hasMore = false;
            uint32_t prevAddr;
            if (findPrevRecordAddr(s, addr, prevAddr)) {
              tokenSector = s;
              tokenAddr = prevAddr;
              hasMore = true;
            } else {
              for (int s2 = s - 1; s2 >= 0; --s2) {
                if (s2 == FACTORY_SECTOR) continue;
                if (!_index[s2].present) continue;
                uint32_t lastAddr;
                if (findLastRecord(s2, lastAddr)) {
                  tokenSector = s2;
                  tokenAddr = lastAddr;
                  hasMore = true;
                  break;
                }
              }
            }
            if (hasMore) {
              uint16_t tokenDay = recDay;
              if (tokenSector >= 0 && tokenSector < MAX_SECTORS && _index[tokenSector].present) {
                tokenDay = _index[tokenSector].dayID;
              }
              buildPageToken(tokenSector, tokenAddr, tokenDay, PAGE_DIR_REV, *nextToken);
            } else {
              *nextToken = "";
            }
          }
          return emitted;
        }
      }

      uint32_t prevAddr;
      if (!findPrevRecordAddr(s, addr, prevAddr)) break;
      addr = prevAddr;
      yield();
    }
  }
  return emitted;
}

uint32_t FlashLogger::queryRange(uint32_t ts_from, uint32_t ts_to, RowCallback onRow, void* user) {
  QuerySpec q; q.ts_from = ts_from; q.ts_to = ts_to; q.out = _outFmt;
  return queryLogs(q, onRow, user);
}

uint32_t FlashLogger::queryBattery(RowCallback onRow, void* user) {
  QuerySpec q; q.out = _outFmt;
  q.includeKeys[0] = "bat"; q.includeKeys[1] = nullptr;
  return queryLogs(q, onRow, user);
}

bool FlashLogger::handleQueryCommand(const String& cmd, Stream& io) {
  // Patterns:
  // q latest <N> [keys...]
  // q day <YYYY-MM-DD> [keys...]
  // q range <YYYY-MM-DD>..<YYYY-MM-DD> [keys...]
  // Optional keys → filter fields in JSON; CSV ignores keys list and uses setCsvColumns()

  QuerySpec q; q.out = _outFmt; q.compact_json = true;

  // extract arguments
  String rest = cmd.substring(2); rest.trim(); // after "q "
  if (rest.startsWith("latest")) {
    rest.remove(0, 6); rest.trim();
    String tokenIn;
    int tokIdx = rest.indexOf("token=");
    if (tokIdx >= 0) {
      tokenIn = rest.substring(tokIdx + 6);
      rest = rest.substring(0, tokIdx);
      tokenIn.trim();
      rest.trim();
    }
    int sp = rest.indexOf(' ');
    String nStr = (sp>=0) ? rest.substring(0, sp) : rest;
    uint32_t N = nStr.toInt();
    // keys (kept for compatibility even though latest currently ignores them)
    int k = 0;
    if (sp >= 0) {
      String tail = rest.substring(sp+1); tail.trim();
      while (tail.length()) {
        int sp2 = tail.indexOf(' ');
        String tok = (sp2>=0) ? tail.substring(0, sp2) : tail;
        tok.trim();
        bool treated = false;
        if (tok.indexOf('<') >=0 || tok.indexOf('>')>=0 || tok.indexOf('=')>=0) {
          treated = addPredicateFromToken(q, tok, &io);
        }
        if (!treated && tok.length()) {
          if (k < 7) q.includeKeys[k++] = strdup(tok.c_str());
        }
        if (sp2 < 0) break;
        tail = tail.substring(sp2+1);
        tail.trim();
      }
      if (k < 8) q.includeKeys[k] = nullptr;
    }
    const String* tokPtr = tokenIn.length() ? &tokenIn : nullptr;
    String nextToken;
    uint32_t outCount = queryLatest(N ? N : 100,
                                    [](const char* line, void* u){ ((Stream*)u)->print(line); },
                                    &io,
                                    tokPtr,
                                    &nextToken);
    if (nextToken.length()) {
      io.printf("next: %s\n", nextToken.c_str());
      saveCursorNVS();
    }
    io.printf("(%lu rows)\n", (unsigned long)outCount);
    return true;
  }

  if (rest.startsWith("day ")) {
    String args = rest.substring(4); args.trim();
    String tokenIn;
    int tokIdx = args.indexOf("token=");
    if (tokIdx >= 0) {
      tokenIn = args.substring(tokIdx + 6);
      args = args.substring(0, tokIdx);
      tokenIn.trim();
      args.trim();
    }
    String date = args; date.trim();
    // split date & optional keys
    String keys; int sp = date.indexOf(' ');
    if (sp>=0) { keys = date.substring(sp+1); date = date.substring(0, sp); date.trim(); keys.trim(); }
    uint16_t dFrom=0; if (!parseDateYYYYMMDD(date, dFrom)) { io.println("bad date (YYYY-MM-DD)"); return true; }
    q.day_from = dFrom; q.day_to = dFrom;

    int k=0;
    while (keys.length()) {
      int sp2 = keys.indexOf(' ');
      String tok = (sp2>=0) ? keys.substring(0, sp2) : keys;
      tok.trim();
      bool treated = false;
      if (tok.indexOf('<')>=0 || tok.indexOf('>')>=0 || tok.indexOf('=')>=0) {
        treated = addPredicateFromToken(q, tok, &io);
      }
      if (!treated && tok.length()) {
        if (k<7) q.includeKeys[k++] = strdup(tok.c_str());
      }
      if (sp2<0) break; keys = keys.substring(sp2+1); keys.trim();
    }
    if (k<8) q.includeKeys[k] = nullptr;

    const String* tokPtr = tokenIn.length() ? &tokenIn : nullptr;
    String nextToken;
    uint32_t outCount = queryLogs(q, [](const char* line, void* u){ ((Stream*)u)->print(line); }, &io, tokPtr, &nextToken);
    if (nextToken.length()) {
      io.printf("next: %s\n", nextToken.c_str());
      saveCursorNVS();
    }
    io.printf("(%lu rows)\n", (unsigned long)outCount);
    return true;
  }

  if (rest.startsWith("range ")) {
    String args = rest.substring(6); args.trim();
    String tokenIn;
    int tokIdx = args.indexOf("token=");
    if (tokIdx >= 0) {
      tokenIn = args.substring(tokIdx + 6);
      args = args.substring(0, tokIdx);
      tokenIn.trim();
      args.trim();
    }
    String rr = args; rr.trim();
    // "YYYY-MM-DD..YYYY-MM-DD" [keys...]
    String keys; int sp = rr.indexOf(' ');
    if (sp>=0) { keys = rr.substring(sp+1); rr = rr.substring(0, sp); rr.trim(); keys.trim(); }
    int dots = rr.indexOf("..");
    if (dots < 0) { io.println("range format: YYYY-MM-DD..YYYY-MM-DD"); return true; }
    String d1 = rr.substring(0, dots);
    String d2 = rr.substring(dots+2);
    uint16_t D1=0, D2=0;
    if (!parseDateYYYYMMDD(d1, D1) || !parseDateYYYYMMDD(d2, D2)) { io.println("bad date(s)"); return true; }
    q.day_from = min(D1, D2); q.day_to = max(D1, D2);

    int k=0;
    while (keys.length()) {
      int sp2 = keys.indexOf(' ');
      String tok = (sp2>=0) ? keys.substring(0, sp2) : keys;
      tok.trim();
      bool treated = false;
      if (tok.indexOf('<')>=0 || tok.indexOf('>')>=0 || tok.indexOf('=')>=0) {
        treated = addPredicateFromToken(q, tok, &io);
      }
      if (!treated && tok.length()) {
        if (k<7) q.includeKeys[k++] = strdup(tok.c_str());
      }
      if (sp2<0) break; keys = keys.substring(sp2+1); keys.trim();
    }
    if (k<8) q.includeKeys[k] = nullptr;

    const String* tokPtr = tokenIn.length() ? &tokenIn : nullptr;
    String nextToken;
    uint32_t outCount = queryLogs(q, [](const char* line, void* u){ ((Stream*)u)->print(line); }, &io, tokPtr, &nextToken);
    if (nextToken.length()) {
      io.printf("next: %s\n", nextToken.c_str());
      saveCursorNVS();
    }
    io.printf("(%lu rows)\n", (unsigned long)outCount);
    return true;
  }

  io.println("q latest <N> [keys...]");
  io.println("q day <YYYY-MM-DD> [keys...]");
  io.println("q range <YYYY-MM-DD>..<YYYY-MM-DD> [keys...]");
  return true;
}

bool FlashLogger::readRecordMeta(uint32_t addr, RecordHeader& rh, uint16_t& recDay) const {
  // determine sector & day from address
  int sector = addr / SECTOR_SIZE;
  if (sector < 0 || sector >= MAX_SECTORS || sector == FACTORY_SECTOR) return false;
  SectorHeader sh;
  if (!((FlashLogger*)this)->readSectorHeader(sector, sh)) return false;
  recDay = sh.dayID;

  // bounds
  uint32_t base = sectorBaseAddr(sector);
  if (addr < base + sizeof(SectorHeader) || addr + sizeof(RecordHeader) >= base + SECTOR_SIZE) return false;

  ((FlashLogger*)this)->readData(addr, (uint8_t*)&rh, sizeof(rh));
  if (rh.len == 0xFFFF || rh.len == 0x0000) return false;
  if (addr + sizeof(rh) + rh.len + 1 > base + SECTOR_SIZE) return false;

  // commit byte quick-check
  uint8_t cm=0; ((FlashLogger*)this)->readData(addr + sizeof(rh) + rh.len, &cm, 1);
  if (cm != REC_COMMIT) return false;

  return true;
}

bool FlashLogger::isValidRecordAt(uint32_t addr) const {
  RecordHeader rh; uint16_t d;
  return readRecordMeta(addr, rh, d);
}

bool FlashLogger::findFirstRecord(int sector, uint32_t& outAddr) const{
  if (sector < 0 || sector >= MAX_SECTORS || sector == FACTORY_SECTOR) return false;
  if (!_index[sector].present) return false;
  uint32_t base = sectorBaseAddr(sector);
  uint32_t ptr  = base + sizeof(SectorHeader);

  while (ptr + sizeof(RecordHeader) < base + SECTOR_SIZE) {
    if (isValidRecordAt(ptr)) { outAddr = ptr; return true; }
    // if header not valid, break (we are at first gap)
    RecordHeader rh; uint16_t d;
    if (!readRecordMeta(ptr, rh, d)) break;
    ptr += sizeof(rh) + rh.len + 1;
    yield();
  }
  return false;
}

bool FlashLogger::findNextRecordAddr(int sector, uint32_t curAddr, uint32_t& nextAddr) const{
  if (sector < 0 || sector >= MAX_SECTORS || sector == FACTORY_SECTOR) return false;
  uint32_t base = sectorBaseAddr(sector);
  if (curAddr < base + sizeof(SectorHeader)) return false;

  RecordHeader rh; uint16_t d;
  if (!readRecordMeta(curAddr, rh, d)) return false;

  uint32_t ptr = curAddr + sizeof(rh) + rh.len + 1;
  if (ptr + sizeof(RecordHeader) >= base + SECTOR_SIZE) return false;

  if (isValidRecordAt(ptr)) { nextAddr = ptr; return true; }
  // allow fast break if next header invalid
  return false;
}

bool FlashLogger::findLastRecord(int sector, uint32_t& outAddr) const{
  if (sector < 0 || sector >= MAX_SECTORS || sector == FACTORY_SECTOR) return false;
  if (!_index[sector].present) return false;
  uint32_t base = sectorBaseAddr(sector);
  uint32_t ptr  = base + sizeof(SectorHeader);
  uint32_t last = 0;

  while (ptr + sizeof(RecordHeader) < base + SECTOR_SIZE) {
    RecordHeader rh; uint16_t recDay;
    if (!readRecordMeta(ptr, rh, recDay)) break;
    last = ptr;
    uint32_t next = ptr + sizeof(rh) + rh.len + 1;
    if (next >= base + SECTOR_SIZE) break;
    ptr = next;
    yield();
  }
  if (!last) return false;
  outAddr = last;
  return true;
}

bool FlashLogger::findPrevRecordAddr(int sector, uint32_t curAddr, uint32_t& prevAddr) const{
  if (sector < 0 || sector >= MAX_SECTORS || sector == FACTORY_SECTOR) return false;
  if (!_index[sector].present) return false;
  uint32_t base = sectorBaseAddr(sector);
  if (curAddr < base + sizeof(SectorHeader)) return false;

  uint32_t ptr   = base + sizeof(SectorHeader);
  uint32_t prev  = 0;

  while (ptr + sizeof(RecordHeader) < base + SECTOR_SIZE) {
    RecordHeader rh; uint16_t recDay;
    if (!readRecordMeta(ptr, rh, recDay)) break;
    if (ptr == curAddr) {
      if (prev) { prevAddr = prev; return true; }
      return false;
    }
    prev = ptr;
    uint32_t next = ptr + sizeof(rh) + rh.len + 1;
    if (next >= base + SECTOR_SIZE) break;
    ptr = next;
    yield();
  }
  return false;
}

bool FlashLogger::earliestCursor(SyncCursor& out) const {
  // scan sectors in order, pick first with a valid record
  for (int s=0; s<MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present) continue;
    uint32_t addr;
    if (((FlashLogger*)this)->findFirstRecord(s, addr)) {
      out.dayID  = _index[s].dayID;
      out.sector = s;
      out.addr   = addr;
      out.seq_next = 0; // unknown; reader doesn’t require it
      return true;
    }
    yield();
  }
  return false;
}

bool FlashLogger::advanceToNextValid(SyncCursor& c) const {
  if (c.sector < 0) return false;
  uint32_t nxt;
  if (findNextRecordAddr(c.sector, c.addr, nxt)) {
    c.addr = nxt;
    return true;
  }
  // move to first record of next sector (same day or later)
  for (int s=c.sector+1; s<MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present) continue;
    uint32_t addr;
    if (((FlashLogger*)this)->findFirstRecord(s, addr)) {
      c.dayID  = _index[s].dayID;
      c.sector = s;
      c.addr   = addr;
      return true;
    }
    yield();
  }
  return false;
}

bool FlashLogger::getCursor(SyncCursor& out) const {
  // “bookmark now”: point to NEXT record that would be written
  out.dayID  = _currentDay;
  out.sector = _currentSector;
  out.addr   = _index[_currentSector].writePtr; // points at empty space (next header)
  out.seq_next = _seqCounter;
  return true;
}

bool FlashLogger::setCursor(const SyncCursor& in) {
  // Validate: allow two styles
  // 1) A real record header address → use it
  // 2) A sector’s first record if addr==0 → resolve automatically
  SyncCursor c = in;

  if (c.sector < 0 || c.sector >= MAX_SECTORS || c.sector == FACTORY_SECTOR) return false;
  if (!_index[c.sector].present) return false;

  if (c.addr == 0) {
    if (!findFirstRecord(c.sector, c.addr)) return false;
  } else if (!isValidRecordAt(c.addr)) {
    // If it points to writePtr (future), bump to next existing sector with data
    uint32_t wp = _index[c.sector].writePtr;
    if (c.addr == wp) {
      if (!advanceToNextValid(c)) return false;
    } else {
      return false;
    }
  }
  _readCursor = c;
  return true;
}

void FlashLogger::clearCursor() {
  SyncCursor c{};
  if (earliestCursor(c)) _readCursor = c;
  else _readCursor = {0,-1,0,0};
}

uint32_t FlashLogger::exportSince(const SyncCursor& from, uint32_t max_rows,
                                  RowCallback onRow, void* user, String* nextToken) {
  return exportSinceInternal(from, max_rows, onRow, user, nullptr, nullptr, nullptr, nextToken);
}

uint32_t FlashLogger::exportSinceWithMeta(const SyncCursor& from, uint32_t max_rows,
                                          bool (*onRecord)(const RecordHeader&, const String&, void*),
                                          void* user, const QuerySpec* filter, String* nextToken) {
  return exportSinceInternal(from, max_rows, nullptr, nullptr, onRecord, user, filter, nextToken);
}

uint32_t FlashLogger::exportSinceInternal(const SyncCursor& from, uint32_t max_rows,
                                          RowCallback onRow, void* rowUser,
                                          bool (*onRecord)(const RecordHeader&, const String&, void*),
                                          void* recordUser, const QuerySpec* filter, String* nextToken) {
  if (!onRow && !onRecord) return 0;
  if (nextToken) *nextToken = "";

  SyncCursor cur = from;
  if (cur.sector < 0 || !isValidRecordAt(cur.addr)) {
    if (!setCursor(from)) return 0;
    cur = _readCursor;
  }

  uint32_t emitted = 0;

  while (true) {
    RecordHeader rh; uint16_t recDay;
    if (!readRecordMeta(cur.addr, rh, recDay)) {
      if (!advanceToNextValid(cur)) break;
      continue;
    }

    static uint8_t buf[PAGE_SIZE];
    String payload;
    uint32_t p = cur.addr + sizeof(rh);
    uint16_t remaining = rh.len;
    while (remaining) {
      uint16_t chunk = min<uint16_t>(remaining, PAGE_SIZE);
      readData(p, buf, chunk);
      payload += String((const char*)buf, chunk);
      p += chunk; remaining -= chunk; yield();
    }

    if (filter && !recordMatchesPredicates(payload.c_str(), payload.length(), *filter)) {
      if (!advanceToNextValid(cur)) break;
      continue;
    }

    if (onRow) {
      QuerySpec q; q.out = _outFmt; q.compact_json = true;
      emitRecord(recDay, rh.ts, (const uint8_t*)payload.c_str(), rh.len, q, onRow, rowUser);
    }

    if (onRecord) {
      if (!onRecord(rh, payload, recordUser)) break;
    }

    emitted++;
    if (max_rows && emitted >= max_rows) {
      if (nextToken) {
        SyncCursor next = cur;
        if (advanceToNextValid(next) && next.sector >= 0 && next.sector != FACTORY_SECTOR) {
          uint16_t tokenDay = 0;
          RecordHeader tmp;
          if (!readRecordMeta(next.addr, tmp, tokenDay)) {
            if (next.sector >= 0 && next.sector < MAX_SECTORS && _index[next.sector].present) {
              tokenDay = _index[next.sector].dayID;
            }
          }
          buildPageToken(next.sector, next.addr, tokenDay, PAGE_DIR_FWD, *nextToken);
        } else {
          nextToken->clear();
        }
      }
      _readCursor = cur;
      break;
    }

    if (!advanceToNextValid(cur)) break;
  }

  return emitted;
}

void FlashLogger::markDaysPushedUntil(uint16_t dayID_inclusive) {
  for (int s=0; s<MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present) continue;
    if (_index[s].dayID <= dayID_inclusive) {
      _index[s].pushed = true;
      SectorHeader hdr;
      if (readSectorHeader(s, hdr)) {
        if (hdr.pushed == 0) {
          hdr.pushed = 1;
          pageProgram(sectorBaseAddr(s), (const uint8_t*)&hdr, sizeof(SectorHeader));
          verifyWrite(sectorBaseAddr(s), (const uint8_t*)&hdr, sizeof(SectorHeader));
        }
      }
    }
    yield();
  }
  // io_flush(); // optional: if you have any buffered IO, otherwise delete
}

bool FlashLogger::handleCursorCommand(const String& cmd, Stream& io) {
  // cursor show | clear | set <dayID> <sector> <addr> <seq>
  // export <max_rows>
  String s = cmd; s.trim();

  if (s.equalsIgnoreCase("cursor show")) {
    io.printf("cursor: day=%u sector=%d addr=0x%06lX seq=%lu\n",
              _readCursor.dayID, _readCursor.sector, (unsigned long)_readCursor.addr,
              (unsigned long)_readCursor.seq_next);
    return true;
  }
  if (s.equalsIgnoreCase("cursor clear")) {
    clearCursor();
    io.println("cursor: reset to earliest");
    return true;
  }
  if (s.startsWith("cursor set")) {
    // cursor set <dayID> <sector> <addr> <seq>
    String rest = s.substring(10); rest.trim();
    int sp1 = rest.indexOf(' '); if (sp1<0) { io.println("usage: cursor set <dayID> <sector> <addr> <seq>"); return true; }
    int sp2 = rest.indexOf(' ', sp1+1); if (sp2<0) { io.println("usage: cursor set <dayID> <sector> <addr> <seq>"); return true; }
    int sp3 = rest.indexOf(' ', sp2+1); if (sp3<0) { io.println("usage: cursor set <dayID> <sector> <addr> <seq>"); return true; }
    uint16_t day = (uint16_t)rest.substring(0, sp1).toInt();
    int      sec = rest.substring(sp1+1, sp2).toInt();
    uint32_t adr = (uint32_t)strtoul(rest.substring(sp2+1, sp3).c_str(), nullptr, 0); // allow 0x...
    uint32_t seq = (uint32_t)rest.substring(sp3+1).toInt();

    SyncCursor c{day, sec, adr, seq};
    if (setCursor(c)) io.println("cursor: set OK");
    else io.println("cursor: set FAILED");
    return true;
  }
  if (s.startsWith("export")) {
    // export <max_rows>
    String rest = s.substring(6); rest.trim();
    String tokenIn;
    int tokIdx = rest.indexOf("token=");
    if (tokIdx >= 0) {
      tokenIn = rest.substring(tokIdx + 6);
      rest = rest.substring(0, tokIdx);
      tokenIn.trim();
      rest.trim();
    }
    uint32_t N = rest.length() ? (uint32_t)rest.toInt() : 0;

    SyncCursor start = _readCursor;
    if (tokenIn.length()) {
      int tokSector; uint32_t tokAddr; uint16_t tokDay; uint8_t tokDir;
      if (parsePageToken(tokenIn, tokSector, tokAddr, tokDay, tokDir) &&
          tokDir == PAGE_DIR_FWD) {
        start.dayID  = tokDay;
        start.sector = tokSector;
        start.addr   = tokAddr;
      } else {
        io.println("export: invalid token");
        return true;
      }
    }

    String nextToken;
    uint32_t rows = exportSince(start, N, [](const char* line, void* u){ ((Stream*)u)->print(line); }, &io, &nextToken);
    if (rows && nextToken.length()) {
      // persist cursor snapshot in NVS when we have more data to stream later
      saveCursorNVS();
    }
    if (nextToken.length()) io.printf("next: %s\n", nextToken.c_str());
    io.printf("(%lu rows)\n", (unsigned long)rows);
    return true;
  }

  return false;
}

void FlashLogger::reinitAfterFactoryReset() {
  // rebuild indexes and pick a fresh sector for today
  for (int i = 0; i < MAX_SECTORS; ++i) {
    _index[i] = {false, 0, false, 0};
  }
  scanAllSectorsBuildIndex();

  uint32_t storedUnix = 0;
  if (!loadLastTimestampNVS(storedUnix)) storedUnix = 0;
  _lastGoodUnix = storedUnix;
  if (_rtc) {
    uint32_t bootUnix = _rtc->now().unixtime();
    if (isRtcTimestampValid(bootUnix)) {
      _rtcHealthy = true;
      _rtcWarningShown = false;
      _lastGoodUnix = bootUnix;
      saveLastTimestampNVS(bootUnix);
    } else {
      _rtcHealthy = false;
      _rtcWarningShown = false;
      Serial.println("FlashLogger: RTC invalid; logging paused until time restored.");
    }
  } else {
    _rtcHealthy = false;
    _rtcWarningShown = false;
    Serial.println("FlashLogger: RTC not available; logging paused.");
  }

  DateTime now = _rtc ? _rtc->now() : DateTime((uint32_t)_lastGoodUnix);
  _currentDay = dayIDFromDateTime(now);
  selectOrCreateTodaySector();
  if (_currentSector >= 0) {
    findLastWritePositionInSector(_currentSector);
    _writeAddr = _index[_currentSector].writePtr;
  } else {
    Serial.println("FlashLogger: no sector available after reset!");
  }

  // clear navigation caches/selection
  _dayCount = 0; _sectCount = 0;
  _selKind = SEL_NONE; _selDay = 0; _selSector = -1;
  _seqCounter = 0;
  _todayBytes = 0;
  _lowSpace = false;
}

bool FlashLogger::loadLastTimestampNVS(uint32_t& outTs) {
  Preferences p;
  if (!p.begin("flog", true)) return false;
  outTs = p.getUInt("rtc_last", 0);
  p.end();
  return outTs != 0;
}

void FlashLogger::saveLastTimestampNVS(uint32_t ts) {
  Preferences p;
  if (!p.begin("flog", false)) return;
  p.putUInt("rtc_last", ts);
  p.end();
}

bool FlashLogger::isRtcTimestampValid(uint32_t unixTs) const {
  constexpr uint32_t kMinAcceptable = 946684800UL; // 2000-01-01T00:00:00Z
  if (unixTs < kMinAcceptable) return false;
  if (_lastGoodUnix && unixTs + 1 < _lastGoodUnix) return false;
  return true;
}

bool FlashLogger::saveConfigToNVS(const FlashLoggerConfig& cfg, const char* ns) {
  Preferences p;
  if (!p.begin(ns ? ns : "flcfg", false)) return false;
  p.putBool("valid", true);
  p.putInt("spi_cs", cfg.spi_cs_pin);
  p.putInt("spi_sck", cfg.spi_sck_pin);
  p.putInt("spi_mosi", cfg.spi_mosi_pin);
  p.putInt("spi_miso", cfg.spi_miso_pin);
  p.putUInt("spi_clk", cfg.spi_clock_hz);
  p.putUShort("retention", cfg.retentionDays);
  p.putUInt("daily_hint", cfg.dailyBytesHint);
  p.putUShort("max_sectors", cfg.maxSectorsPerDay);
  p.putUInt("total_bytes", cfg.totalSizeBytes);
  p.putUInt("sector_size", cfg.sectorSize);
  p.putInt("date_style", (int)cfg.dateStyle);
  p.putInt("out_fmt", (int)cfg.defaultOut);
  p.putBool("shell", cfg.enableShell);
  p.putBool("persist", cfg.persistConfig);
  if (cfg.model)      p.putString("model", cfg.model);
  if (cfg.flashModel) p.putString("flashModel", cfg.flashModel);
  if (cfg.deviceId)   p.putString("deviceId", cfg.deviceId);
  if (cfg.resetCode12)p.putString("resetCode", cfg.resetCode12);
  if (cfg.csvColumns) p.putString("csv", cfg.csvColumns);
  p.end();
  return true;
}

bool FlashLogger::loadConfigFromPrefs(FlashLoggerConfig& cfg, Preferences& p) {
  if (!p.getBool("valid", false)) return false;
  cfg.spi_cs_pin       = p.getInt("spi_cs", cfg.spi_cs_pin);
  cfg.spi_sck_pin      = p.getInt("spi_sck", cfg.spi_sck_pin);
  cfg.spi_mosi_pin     = p.getInt("spi_mosi", cfg.spi_mosi_pin);
  cfg.spi_miso_pin     = p.getInt("spi_miso", cfg.spi_miso_pin);
  cfg.spi_clock_hz     = p.getUInt("spi_clk", cfg.spi_clock_hz);
  cfg.retentionDays    = p.getUShort("retention", cfg.retentionDays);
  cfg.dailyBytesHint   = p.getUInt("daily_hint", cfg.dailyBytesHint);
  cfg.maxSectorsPerDay = p.getUShort("max_sectors", cfg.maxSectorsPerDay);
  cfg.totalSizeBytes   = p.getUInt("total_bytes", cfg.totalSizeBytes);
  cfg.sectorSize       = p.getUInt("sector_size", cfg.sectorSize);
  cfg.dateStyle        = (DateStyle)p.getInt("date_style", (int)cfg.dateStyle);
  cfg.defaultOut       = (OutFmt)p.getInt("out_fmt", (int)cfg.defaultOut);
  cfg.enableShell      = p.getBool("shell", cfg.enableShell);
  cfg.persistConfig    = p.getBool("persist", cfg.persistConfig);

  _loadedModel        = p.getString("model", cfg.model ? cfg.model : "");
  _loadedFlashModel   = p.getString("flashModel", cfg.flashModel ? cfg.flashModel : "");
  _loadedDeviceId     = p.getString("deviceId", cfg.deviceId ? cfg.deviceId : "");
  _loadedResetCode    = p.getString("resetCode", cfg.resetCode12 ? cfg.resetCode12 : "");
  _loadedCsvColumns   = p.getString("csv", cfg.csvColumns ? cfg.csvColumns : "");

  if (_loadedModel.length()) cfg.model = _loadedModel.c_str();
  if (_loadedFlashModel.length()) cfg.flashModel = _loadedFlashModel.c_str();
  if (_loadedDeviceId.length()) cfg.deviceId = _loadedDeviceId.c_str();
  if (_loadedResetCode.length()) cfg.resetCode12 = _loadedResetCode.c_str();
  if (_loadedCsvColumns.length()) cfg.csvColumns = _loadedCsvColumns.c_str();

  return true;
}

bool FlashLogger::loadConfigFromNVS(FlashLoggerConfig& cfg, const char* ns) {
  Preferences p;
  if (!p.begin(ns ? ns : "flcfg", true)) return false;
  bool ok = loadConfigFromPrefs(cfg, p);
  p.end();
  return ok;
}





bool FlashLogger::firstRecordInSector(int sector, uint32_t& firstAddr, uint32_t& firstTs, uint32_t& lastTs) {
  if (sector < 0 || sector >= MAX_SECTORS || sector == FACTORY_SECTOR) return false;
  if (!_index[sector].present) return false;

  uint32_t base = sectorBaseAddr(sector);
  uint32_t ptr  = base + sizeof(SectorHeader);
  bool found = false;
  firstTs = 0; lastTs = 0;

  while (ptr + sizeof(RecordHeader) < base + SECTOR_SIZE) {
    RecordHeader rh; uint16_t d;
    if (!readRecordMeta(ptr, rh, d)) break;
    if (!found) {
      firstAddr = ptr;
      firstTs   = rh.ts;
      found = true;
    }
    lastTs = rh.ts;
    ptr += sizeof(rh) + rh.len + 1;
  }
  return found;
}

void FlashLogger::buildAnchors(bool persist) {
  _anchorCount = 0;
  for (int s=0; s<MAX_SECTORS && _anchorCount<MAX_ANCHORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    if (!_index[s].present) continue;
    uint32_t fa, fts, lts;
    if (firstRecordInSector(s, fa, fts, lts)) {
      Anchor& a = _anchors[_anchorCount++];
      a.sector   = s;
      a.dayID    = _index[s].dayID;
      a.firstTs  = fts;
      a.firstAddr= fa;
      a.lastTs   = lts;
    }
    yield();
  }
  if (persist) saveAnchorsToNVS();
}

bool FlashLogger::loadAnchorsFromNVS() {
  _anchorCount = 0;
  Preferences p;
  if (!p.begin("flanchors", true)) return false;
  uint32_t magic = p.getUInt("magic", 0);
  if (magic != ANCHOR_MAGIC) { p.end(); return false; }
  uint32_t gen = p.getUInt("generation", 0);
  if (gen != _generation) { p.end(); return false; }
  uint16_t count = p.getUShort("count", 0);
  if (count > MAX_ANCHORS) count = MAX_ANCHORS;
  size_t bytes = count * sizeof(Anchor);
  if (bytes) {
    size_t got = p.getBytes("data", _anchors, bytes);
    p.end();
    if (got != bytes) return false;
  } else {
    p.end();
  }
  for (int i=0; i<count; ++i) {
    int sec = _anchors[i].sector;
    if (sec < 0 || sec >= MAX_SECTORS) return false;
    if (!_index[sec].present) return false;
    if (_index[sec].dayID != _anchors[i].dayID) return false;
  }
  _anchorCount = count;
  return true;
}

void FlashLogger::saveAnchorsToNVS() const {
  Preferences p;
  if (!p.begin("flanchors", false)) return;
  p.putUInt("magic", ANCHOR_MAGIC);
  p.putUInt("generation", _generation);
  p.putUShort("count", _anchorCount);
  if (_anchorCount) {
    p.putBytes("data", _anchors, _anchorCount * sizeof(Anchor));
  }
  p.end();
}

bool FlashLogger::sectorMaybeInRangeByAnchor(int sector, uint16_t dayFrom, uint16_t dayTo,
                                             uint32_t ts_from, uint32_t ts_to) const {
  // if day range used, decide by day
  if (dayFrom || dayTo) {
    uint16_t d = _index[sector].dayID;
    if (dayFrom && d < dayFrom) return false;
    if (dayTo   && d > dayTo)   return false;
    return true;
  }
  // else use timestamp window from anchors
  for (int i=0;i<_anchorCount;++i) {
    if (_anchors[i].sector == sector) {
      // overlap check
      return !(_anchors[i].lastTs < ts_from || _anchors[i].firstTs > ts_to);
    }
  }
  // unknown → fall back to true
  return true;
}

void FlashLogger::scanBadAndQuarantine(Stream& io) {
  io.println("scanbad: scanning sectors...");
  int quarantined = 0;
  for (int s=0; s<MAX_SECTORS; ++s) {
    if (s == FACTORY_SECTOR) continue;
    uint32_t base = sectorBaseAddr(s);

    // Heuristic: try to read header; if magic is corrupt or repeated write/erase fails verification,
    // quarantine the sector. We DO NOT erase during scan—non-destructive.
    SectorHeader sh;
    if (!readSectorHeader(s, sh)) {
      io.printf("  sector %d: unreadable header → quarantine\n", s);
      quarantineSector(s); quarantined++; continue;
    }

    // Optional lightweight write/verify probe at end of free area (skip if present day)
    // We keep it passive by default to avoid wearing the flash.
    // (Leave hooks here if you want an active probe mode later.)
    yield();
  }
  io.printf("scanbad: done. quarantined=%d\n", quarantined);
}

void FlashLogger::rescanAndRefresh(bool rebuildSummaries, bool keepSelection) {
  // Remember selection if requested
  SelKind  prevKind    = _selKind;
  uint16_t prevDay     = _selDay;
  int      prevSector  = _selSector;

  // Clear fast state
  for (int i=0; i<MAX_SECTORS; ++i) {
    _index[i] = {false, 0, false, 0};
  }

  // Re-scan the world
  scanAllSectorsBuildIndex();
  if (!loadAnchorsFromNVS()) buildAnchors(true);

  if (rebuildSummaries) {
    buildSummaries();                // also recomputes _days/_sects
    // buildSummaries() can also call buildAnchors()—that’s fine (cheap)
  }

  // Re-pick today’s sector/write head
  selectOrCreateTodaySector();
  if (_currentSector >= 0) {
    findLastWritePositionInSector(_currentSector);
    _writeAddr = _index[_currentSector].writePtr;
  }

  // Optionally restore selection (if it still exists)
  if (keepSelection) {
    _selKind = SEL_NONE; _selDay = 0; _selSector = -1;
    if (prevKind == SEL_DAY) {
      if (selectDay(prevDay)) { /* ok */ }
    } else if (prevKind == SEL_SECTOR) {
      if (_index[prevSector].present) {
        selectSector(prevSector);
      }
    }
  }

  // If you also use the “dirty anchors” flag:
  // _anchorsDirty = false;
}

bool FlashLogger::saveCursorNVS(const char* ns, const char* key) {
  Preferences p; if (!p.begin(ns, false)) return false;

  String k_day = String(key) + "_day";
  String k_sec = String(key) + "_sec";
  String k_adr = String(key) + "_adr";
  String k_seq = String(key) + "_seq";

  bool ok = true;
  ok &= (p.putUShort(k_day.c_str(), _readCursor.dayID)   > 0);
  ok &= (p.putInt   (k_sec.c_str(), _readCursor.sector)  > 0);
  ok &= (p.putUInt  (k_adr.c_str(), _readCursor.addr)    > 0);
  ok &= (p.putUInt  (k_seq.c_str(), _readCursor.seq_next)> 0);
  p.end();
  return ok;
}

bool FlashLogger::loadCursorNVS(SyncCursor& out, const char* ns, const char* key) {
  Preferences p; if (!p.begin(ns, true)) return false;

  String k_day = String(key) + "_day";
  String k_sec = String(key) + "_sec";
  String k_adr = String(key) + "_adr";
  String k_seq = String(key) + "_seq";

  out.dayID    = p.getUShort(k_day.c_str(), 0);
  out.sector   = p.getInt   (k_sec.c_str(), -1);
  out.addr     = p.getUInt  (k_adr.c_str(), 0);
  out.seq_next = p.getUInt  (k_seq.c_str(), 0);
  p.end();

  return (out.sector >= 0) && isValidRecordAt(out.addr);
}
