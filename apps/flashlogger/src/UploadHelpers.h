#pragma once

#include <Arduino.h>
#include "FlashLogger.h"

struct FlashLoggerUploadPolicy {
  uint8_t maxAttempts = 3;
  uint32_t initialBackoffMs = 500;
  float backoffMultiplier = 2.0f;
};

typedef bool (*FlashLoggerSendCallback)(const char* payload, size_t len,
                                        const char* idempotencyKey, void* user);

bool flashlogger_upload_ndjson(FlashLogger& logger,
                               const SyncCursor& cursor,
                               uint32_t maxRows,
                               FlashLoggerSendCallback sender,
                               void* user,
                               const FlashLoggerUploadPolicy& policy,
                               String* nextToken = nullptr);

bool flashlogger_upload_csv(FlashLogger& logger,
                            const SyncCursor& cursor,
                            uint32_t maxRows,
                            FlashLoggerSendCallback sender,
                            void* user,
                            const FlashLoggerUploadPolicy& policy,
                            String* nextToken = nullptr);
