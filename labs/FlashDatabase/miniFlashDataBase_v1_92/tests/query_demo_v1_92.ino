#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include "FlashLogger.h"

RTC_DS3231 rtc;
FlashLogger logger(4);

static void printLine(const char* line, void* user) {
  ((Stream*)user)->print(line);
}

void runJsonQuery() {
  Serial.println(F("\n[test] JSONL latest (include temp only)"));
  QuerySpec q;
  q.out = OUT_JSONL;
  q.max_records = 3;
  q.includeKeys[0] = "ts";
  q.includeKeys[1] = "temp";
  q.includeKeys[2] = nullptr;
  logger.queryLogs(q, printLine, &Serial, nullptr, nullptr);
}

void runCsvQuery() {
  Serial.println(F("\n[test] CSV range (sample every 2)"));
  QuerySpec q;
  q.out = OUT_CSV;
  q.sample_every = 2;
  q.ts_from = rtc.now().unixtime() - 3600;
  q.ts_to   = rtc.now().unixtime() + 3600;
  logger.setCsvColumns("ts,temp,bat");
  logger.queryLogs(q, printLine, &Serial, nullptr, nullptr);
}

void runShellExample() {
  Serial.println(F("\n[test] shell command: q latest 5"));
  logger.handleCommand("fmt jsonl", Serial);
  logger.handleCommand("q latest 5", Serial);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nv1.92 query demo");

  Wire.begin(19, 20);
  rtc.begin();

  logger.begin(&rtc);
  logger.setDateStyle(2);

  uint32_t base = rtc.now().unixtime();
  logger.append(String("{\"ts\":" + String(base)     + ",\"temp\":25.5,\"hum\":60,\"bat\":88}"));
  logger.append(String("{\"ts\":" + String(base+60) + ",\"temp\":25.8,\"hum\":61,\"bat\":87}"));
  logger.append(String("{\"ts\":" + String(base+120)+ ",\"temp\":26.0,\"hum\":62,\"bat\":86}"));
  logger.append(String("{\"ts\":" + String(base+180)+ ",\"temp\":26.2,\"hum\":63,\"bat\":85}"));

  runJsonQuery();
  runCsvQuery();
  runShellExample();
}

void loop() {}
