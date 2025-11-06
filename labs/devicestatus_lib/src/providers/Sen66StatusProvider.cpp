#include "../../include/device_status/providers/Sen66StatusProvider.h"

#include <SensirionErrors.h>

namespace device_status {

namespace {
constexpr uint8_t kStatusWarnBit = 21;
constexpr uint8_t kStatusErrBits[] = {11, 9, 7, 6, 4};



void errorToDetail(int16_t err, char* out, size_t len) {
  char msg[96];
  errorToString(static_cast<uint16_t>(err), msg, sizeof(msg));
  snprintf(out, len, "sensor error: %s", msg);
}
}  // namespace

Sen66StatusProvider::Sen66StatusProvider(SensirionI2cSen66& driver,
                                         TwoWire& wire,
                                         uint8_t address)
    : _driver(driver), _wire(wire), _address(address) {
  setDetails("not initialised");
}

bool Sen66StatusProvider::begin() {
  _driver.begin(_wire, _address);
  int16_t err = _driver.deviceReset();
  if (err != 0) {
    errorToDetail(err, _details, sizeof(_details));
    return false;
  }
  delay(200);

  err = _driver.startContinuousMeasurement();
  if (err != 0) {
    errorToDetail(err, _details, sizeof(_details));
    return false;
  }

  setDetails("measurement started");
  _started = true;
  return true;
}

DeviceStatusCode Sen66StatusProvider::refresh(DeviceStatusReport& report) {
  if (!_started) {
    setDetails("not started");
    report.sen66 = STATUS_INIT_FAIL;
    report.sen66Data.dataValid = false;
    return report.sen66;
  }

  bool ready = false;
  uint8_t padding = 0;
  int16_t err = _driver.getDataReady(padding, ready);
  if (err != 0) {
    errorToDetail(err, _details, sizeof(_details));
    report.sen66 = STATUS_ERROR;
    report.sen66Data.dataValid = false;
    return report.sen66;
  }
  if (!ready) {
    setDetails("waiting data-ready");
    return report.sen66;
  }

  float pm1, pm25, pm4, pm10, rh, tc, vocIdx, noxIdx;
  uint16_t co2ppm;
  err = _driver.readMeasuredValues(pm1, pm25, pm4, pm10, rh, tc, vocIdx, noxIdx, co2ppm);
  if (err != 0) {
    errorToDetail(err, _details, sizeof(_details));
    report.sen66 = STATUS_ERROR;
    report.sen66Data.dataValid = false;
    return report.sen66;
  }

  SEN66DeviceStatus ds{};
  err = _driver.readAndClearDeviceStatus(ds);
  if (err != 0) {
    errorToDetail(err, _details, sizeof(_details));
    report.sen66 = STATUS_ERROR;
  } else {
    report.sen66 = evaluateStatus(ds.value);
    report.sen66Data.rawStatus = ds.value;
    setDetails(report.sen66 == STATUS_OK ? "ok" : "status flagged");
  }

  report.sen66Data.pm1 = pm1;
  report.sen66Data.pm25 = pm25;
  report.sen66Data.pm4 = pm4;
  report.sen66Data.pm10 = pm10;
  report.sen66Data.humidity = rh;
  report.sen66Data.temperatureC = tc;
  report.sen66Data.vocIndex = vocIdx;
  report.sen66Data.noxIndex = noxIdx;
  report.sen66Data.co2ppm = co2ppm;
  report.sen66Data.dataValid = true;
  return report.sen66;
}

DeviceStatusCode Sen66StatusProvider::evaluateStatus(uint32_t raw) {
  bool warn = raw & bit(kStatusWarnBit);
  bool err = false;
  for (uint8_t b : kStatusErrBits) {
    if (raw & bit(b)) {
      err = true;
      break;
    }
  }
  if (err) return STATUS_ERROR;
  if (warn) return STATUS_WARN;
  return STATUS_OK;
}

void Sen66StatusProvider::setDetails(const char* msg) {
  snprintf(_details, sizeof(_details), "%s", msg);
}

}  // namespace device_status
