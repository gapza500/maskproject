#include "UploadHelpers.h"

namespace {
struct UploadContext {
  FlashLogger& logger;
  FlashLoggerSendCallback sender;
  void* user;
  FlashLoggerUploadPolicy policy;
  OutFmt fmt;
};

bool sendWithRetry(UploadContext& ctx, const RecordHeader& rh, const String& rawPayload) {
  String formatted;
  ctx.logger.formatPayload(rh.ts, rawPayload, ctx.fmt, formatted);
  String seqKey = String((unsigned long)rh.seq);

  uint32_t backoff = ctx.policy.initialBackoffMs;
  for (uint8_t attempt = 1; attempt <= ctx.policy.maxAttempts; ++attempt) {
    if (!ctx.sender || ctx.sender(formatted.c_str(), formatted.length(), seqKey.c_str(), ctx.user)) {
      return true;
    }
    if (attempt < ctx.policy.maxAttempts) {
      if (backoff) delay(backoff);
      backoff = (uint32_t)(backoff * ctx.policy.backoffMultiplier);
    }
  }
  return false;
}

bool exportCallback(const RecordHeader& rh, const String& payload, void* user) {
  auto* ctx = static_cast<UploadContext*>(user);
  return sendWithRetry(*ctx, rh, payload);
}
} // namespace

static bool uploadInternal(FlashLogger& logger,
                           const SyncCursor& cursor,
                           uint32_t maxRows,
                           FlashLoggerSendCallback sender,
                           void* user,
                           const FlashLoggerUploadPolicy& policy,
                           OutFmt fmt,
                           String* nextToken) {
  FlashLoggerUploadPolicy pol = policy;
  if (pol.maxAttempts == 0) pol.maxAttempts = 1;
  if (pol.initialBackoffMs == 0 && pol.maxAttempts > 1) pol.initialBackoffMs = 100;
  if (pol.backoffMultiplier < 1.0f) pol.backoffMultiplier = 1.0f;

  UploadContext ctx{logger, sender, user, pol, fmt};
  uint32_t sent = logger.exportSinceWithMeta(cursor, maxRows, exportCallback, &ctx, nextToken);
  return sent > 0;
}

bool flashlogger_upload_ndjson(FlashLogger& logger,
                               const SyncCursor& cursor,
                               uint32_t maxRows,
                               FlashLoggerSendCallback sender,
                               void* user,
                               const FlashLoggerUploadPolicy& policy,
                               String* nextToken) {
  return uploadInternal(logger, cursor, maxRows, sender, user, policy, OUT_JSONL, nextToken);
}

bool flashlogger_upload_csv(FlashLogger& logger,
                            const SyncCursor& cursor,
                            uint32_t maxRows,
                            FlashLoggerSendCallback sender,
                            void* user,
                            const FlashLoggerUploadPolicy& policy,
                            String* nextToken) {
  return uploadInternal(logger, cursor, maxRows, sender, user, policy, OUT_CSV, nextToken);
}
