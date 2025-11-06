#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "../../../labs/devicestatus_lib/include/device_status/DeviceStatusCodes.h"
#include "../../../labs/devicestatus_lib/include/device_status/Report.h"
#include "../../../labs/FlashDatabase/miniFlashDataBase_v2_0/FlashLogger.h"

namespace comms {
namespace local_api {

struct Config {
  uint16_t port = 8080;
  bool enabled = true;
};

class Service {
 public:
  explicit Service(const Config& cfg) : _cfg(cfg), _server(cfg.port) {}

  void setLogger(FlashLogger* logger) { _logger = logger; }

  void begin() {
    if (!_cfg.enabled || _started) return;
    _server.on("/status", [this]() { handleStatus(); });
    _server.on("/logs/latest", [this]() { handleLogsLatest(); });
    _server.onNotFound([this]() { handleNotFound(); });
    _server.begin();
    _started = true;
  }

  void loop(const device_status::DeviceStatusReport& report) {
    _report = &report;
    if (_started) {
      _server.handleClient();
    }
  }

 private:
  void handleStatus() {
    if (!_report) {
      _server.send(503, "application/json", "{\"error\":\"no status\"}");
      return;
    }
    String json = "{";
    json += "\"sen66\":\"";
    json += device_status::statusToString(_report->sen66);
    json += "\",\"battery_pct\":";
    json += String(_report->batteryData.percent, 1);
    json += ",\"battery_v\":";
    json += String(_report->batteryData.voltage, 3);
    json += ",\"rtc_ok\":";
    json += (_report->rtcData.running && !_report->rtcData.lostPower) ? "true" : "false";
    json += ",\"flash_health\":";
    json += String(_report->flashData.healthPercent, 1);
    json += ",\"timestamp\":";
    json += String(_report->rtcData.unixTime);
    json += "}";
    _server.send(200, "application/json", json);
  }

  void handleLogsLatest() {
    if (!_logger) {
      _server.send(503, "text/plain", "logger unavailable");
      return;
    }
    uint32_t limit = 10;
    if (_server.hasArg("limit")) {
      limit = (uint32_t)_server.arg("limit").toInt();
      if (limit == 0) limit = 1;
      if (limit > 200) limit = 200;
    }
    auto prevFmt = _logger->outputFormat();
    _logger->setOutputFormat(OUT_JSONL);
    struct Buffer { String body; uint32_t count = 0; } buf;
    auto collector = [](const char* line, void* user) {
      auto* b = static_cast<Buffer*>(user);
      if (b->count) b->body += '\n';
      b->body += line;
      b->count++;
    };
    _logger->queryLatest(limit, collector, &buf);
    _logger->setOutputFormat(prevFmt);
    if (!buf.count) {
      _server.send(204, "text/plain", "");
      return;
    }
    _server.send(200, "application/x-ndjson", buf.body);
  }

  void handleNotFound() {
    _server.send(404, "text/plain", "Not Found");
  }

  Config _cfg;
  WebServer _server;
  bool _started = false;
  const device_status::DeviceStatusReport* _report = nullptr;
  FlashLogger* _logger = nullptr;
};

}  // namespace local_api
}  // namespace comms
