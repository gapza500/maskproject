#include <WiFi.h>
#include <PubSubClient.h>
#include "../UploadHelpers.h"

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);
static const char* kTopic = "devices/device-001/logs";

static bool mqttSender(const char* payload, size_t len, const char* key, void* user) {
  (void)user;
  char topic[128];
  snprintf(topic, sizeof(topic), "%s/%s", kTopic, key);
  return mqttClient.publish(topic, payload, len, true /*retain*/);
}

void uploadLogsOverMqtt(FlashLogger& logger) {
  if (!mqttClient.connected()) return;
  SyncCursor cursor;
  if (!logger.getCursor(cursor)) return;
  FlashLoggerUploadPolicy policy;
  policy.maxAttempts = 3;
  policy.initialBackoffMs = 1000;
  policy.backoffMultiplier = 1.5f;
  String nextToken;
  flashlogger_upload_csv(logger, cursor, 25, mqttSender, nullptr, policy, &nextToken);
}
