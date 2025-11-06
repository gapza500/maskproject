#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "../../../labs/FlashDatabase/miniFlashDataBase_v2_0/FlashLogger.h"
#include "../../../labs/FlashDatabase/miniFlashDataBase_v2_0/UploadHelpers.h"

namespace comms {
namespace cloud {

struct Config {
  const char* url = nullptr;
  const char* authHeader = nullptr;
  const char* authToken = nullptr;
  uint32_t publishIntervalMs = 60'000;
  uint32_t batchSize = 64;
  FlashLoggerUploadPolicy policy{};
  const char* prefsNamespace = "cloud";
  bool enabled = false;
};

struct State {
  uint32_t lastPublishMs = 0;
  SyncCursor cursor{0, -1, 0, 0};
  bool cursorLoaded = false;
};

namespace detail {
struct SenderCtx {
  const Config* cfg;
};

inline bool httpSender(const char* payload,
                       size_t len,
                       const char* key,
                       void* user) {
  auto* ctx = static_cast<SenderCtx*>(user);
  if (!ctx || !ctx->cfg || !ctx->cfg->url) return false;

  HTTPClient http;
  if (!http.begin(ctx->cfg->url)) return false;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Idempotency-Key", key);
  if (ctx->cfg->authHeader && ctx->cfg->authToken) {
    http.addHeader(ctx->cfg->authHeader, ctx->cfg->authToken);
  }
  int status = http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(payload)), len);
  http.end();
  return status >= 200 && status < 300;
}

inline FlashLoggerUploadPolicy normalise(FlashLoggerUploadPolicy pol) {
  if (pol.maxAttempts == 0) pol.maxAttempts = 3;
  if (pol.initialBackoffMs == 0 && pol.maxAttempts > 1) pol.initialBackoffMs = 500;
  if (pol.backoffMultiplier < 1.0f) pol.backoffMultiplier = 2.0f;
  return pol;
}

inline bool ensureCursorLoaded(State& state, const Config& cfg, FlashLogger& logger) {
  if (state.cursorLoaded) return true;
  if (!logger.loadCursorNVS(state.cursor, cfg.prefsNamespace, "cursor")) {
    logger.clearCursor();
    logger.saveCursorNVS(cfg.prefsNamespace, "cursor");
    if (!logger.loadCursorNVS(state.cursor, cfg.prefsNamespace, "cursor")) {
      return false;
    }
  }
  if (!logger.setCursor(state.cursor)) {
    logger.clearCursor();
    logger.saveCursorNVS(cfg.prefsNamespace, "cursor");
    if (!logger.loadCursorNVS(state.cursor, cfg.prefsNamespace, "cursor")) {
      return false;
    }
    logger.setCursor(state.cursor);
  }
  state.cursorLoaded = true;
  return true;
}

}  // namespace detail

inline void init(const Config& cfg, State& state, FlashLogger& logger) {
  state.lastPublishMs = 0;
  state.cursorLoaded = false;
  state.cursor = SyncCursor{0, -1, 0, 0};
  detail::ensureCursorLoaded(state, cfg, logger);
}

inline void publishIfDue(const Config& cfg,
                         State& state,
                         FlashLogger& logger,
                         uint32_t nowMs) {
  if (!cfg.enabled || !cfg.url) return;
  if (cfg.publishIntervalMs && (nowMs - state.lastPublishMs) < cfg.publishIntervalMs) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (!detail::ensureCursorLoaded(state, cfg, logger)) return;

  FlashLoggerUploadPolicy pol = detail::normalise(cfg.policy);
  detail::SenderCtx ctx{&cfg};

  logger.setCursor(state.cursor);

  bool sent = flashlogger_upload_ndjson(logger,
                                        state.cursor,
                                        cfg.batchSize,
                                        detail::httpSender,
                                        &ctx,
                                        pol,
                                        nullptr);
  if (sent) {
    SyncCursor updated;
    logger.getCursor(updated);
    state.cursor = updated;
    logger.saveCursorNVS(cfg.prefsNamespace, "cursor");
  }
  state.lastPublishMs = nowMs;
}

inline void teardown(State& state) {
  state.cursorLoaded = false;
}

}  // namespace cloud
}  // namespace comms
