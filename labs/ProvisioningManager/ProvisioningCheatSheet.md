# ProvisioningManager Cheat Sheet

**Constructor**
- `ProvisioningManager(const ProvisioningConfig& cfg)` — pass a fully populated config struct (see below).

**Setup**
```cpp
ProvisioningManager mgr(cfg);

void setup() {
  Serial.begin(115200);
  mgr.begin();
}

void loop() {
  mgr.loop();
}
```

> Demo sketch: `apps/ProvisioningManagerDemo/ProvisioningManagerDemo.ino` shows this wiring and includes the `.cpp` directly so it compiles without moving files. For production, copy `ProvisioningManager.{h,cpp}` into your sketch (or create a proper library).

**ProvisioningConfig fields**
- `apSsid`, `apPassword` — captive portal credentials (password must be ≥ 8 chars).
- `localIP`, `gateway`, `subnet` — AP IP configuration (defaults to `192.168.4.1/24`).
- `ledPin` — digital pin used for status LED (use `-1` to disable LED control).
- `connectTimeoutMs` — station connect timeout per attempt.
- `staHeartbeatIntervalMs`, `apHeartbeatIntervalMs` — LED blink periods in ms.
- `reconnectBackoffMs` — delay between STA reconnect attempts.
- `portalShutdownDelayMs` — delay before tearing down AP after a successful save.
- `broadcastPort` — UDP port for the provisioning completion broadcast.

**Runtime helpers**
- `bool mgr.isProvisioning()` — `true` while AP/captive portal active.
- `const String& mgr.lastProvisionedNetwork()` — last SSID that succeeded or failed.
- `void mgr.resetStoredCredentials()` — clear NVS credentials (same as serial `reset`).
- `void mgr.handleSerialCommands()` — already invoked inside `loop()`, re-call if you use your own loop structure.

**Serial commands**
- `reset` — clears stored credentials, stops STA, and relaunches provisioning.

**HTTP endpoints (served while AP active)**
- `GET /` — captive portal UI.
- `GET /scan` — returns JSON array of nearby networks.
- `POST /save` — body `ssid=<ssid>&password=<pass>` connects, saves credentials, and schedules STA mode.
- `GET /status` — returns `{mode, connected, state, message, ssid, ip?}` for polling by a companion app.

**UDP broadcast**
- Sent once per successful provisioning or reconnect (when STA link established).
- Destination: subnet broadcast on `broadcastPort`.
- Payload: `{"event":"provisioning_complete","ssid":"...", "ip":"...", "device":"optional hostname"}`.

**Error handling**
- Failed `/save` requests leave the AP running; UI message comes from the JSON response.
- If STA connection drops later, the manager retries based on `reconnectBackoffMs`; on repeated failure it re-enters provisioning.
