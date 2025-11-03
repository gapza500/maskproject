#ifndef PROVISIONING_MANAGER_H
#define PROVISIONING_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>

struct ProvisioningConfig {
    const char* apSsid;
    const char* apPassword;
    IPAddress localIP;
    IPAddress gateway;
    IPAddress subnet;
    int ledPin;
    uint32_t connectTimeoutMs;
    uint32_t staHeartbeatIntervalMs;
    uint32_t apHeartbeatIntervalMs;
    uint32_t reconnectBackoffMs;
    uint32_t portalShutdownDelayMs;
    uint16_t broadcastPort;
};

class ProvisioningManager {
public:
    explicit ProvisioningManager(const ProvisioningConfig& config);

    void begin();
    void loop();
    void handleSerialCommands();
    void resetStoredCredentials();
    bool isProvisioning() const;
    const String& lastProvisionedNetwork() const;
    const ProvisioningConfig& getConfig() const;

private:
    enum class ProvisioningState : uint8_t { Idle, Connecting, Success, Failure };

    void initProvisioning(bool resetStatus = true);
    void initSTA(const String& ssid, const String& password);
    void setStatusLED(int state);
    void saveCredentials(const String& ssid, const String& password);
    bool loadCredentials(String& ssid, String& password);
    void ensureServerHandlers();
    void enterOperationalMode();
    bool connectToNetwork(const String& ssid, const String& password, wifi_mode_t mode);
    void notifyProvisioningSuccess();
    void handleRoot();
    void handleScan();
    void handleSave();
    void handleStatus();
    void handleNotFound();
    const char* provisioningStateToString(ProvisioningState state) const;

    ProvisioningConfig config;
    WebServer server;
    DNSServer dnsServer;
    Preferences preferences;
    WiFiUDP provisioningUdp;

    bool isProvisioningMode;
    bool serverHandlersRegistered;
    bool pendingPortalShutdown;
    bool udpInitialized;
    unsigned long lastReconnectAttempt;
    unsigned long portalShutdownAt;
    String lastProvisionedSsid;
    ProvisioningState provisioningState;
    String provisioningMessage;

    static constexpr const char* PREF_NAMESPACE = "wifi_config";
    static constexpr const char* PREF_KEY_SSID = "ssid";
    static constexpr const char* PREF_KEY_PASS = "pass";
};

#endif // PROVISIONING_MANAGER_H
