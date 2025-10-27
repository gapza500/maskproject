#include <WiFi.h>
#include <HTTPClient.h>
#include "../UploadHelpers.h"

static const char* kServerUrl = "https://example.com/api/logs";
static const char* kDeviceKey = "device-001";

static bool httpSender(const char* payload, size_t len, const char* key, void* user) {
  (void)user;
  HTTPClient http;
  http.begin(kServerUrl);
  http.addHeader("Content-Type", "application/json" );
  http.addHeader("X-Idempotency-Key", key);
  http.addHeader("X-Device-Key", kDeviceKey);
  int status = http.POST((uint8_t*)payload, len);
  http.end();
  return status >= 200 && status < 300;
}

void uploadLogsOverHttp(FlashLogger& logger) {
  SyncCursor cursor;
  if (!logger.getCursor(cursor)) return;
  FlashLoggerUploadPolicy policy;
  policy.maxAttempts = 5;
  policy.initialBackoffMs = 500;
  policy.backoffMultiplier = 2.0f;
  String nextToken;
  flashlogger_upload_ndjson(logger, cursor, 50, httpSender, nullptr, policy, &nextToken);
}
