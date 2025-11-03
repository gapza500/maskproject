#include "ProvisioningManager.h"

#include <functional>

using std::placeholders::_1;

ProvisioningManager::ProvisioningManager(const ProvisioningConfig& cfg)
    : config(cfg),
      server(80),
      dnsServer(),
      preferences(),
      provisioningUdp(),
      isProvisioningMode(true),
      serverHandlersRegistered(false),
      pendingPortalShutdown(false),
      udpInitialized(false),
      lastReconnectAttempt(0),
      portalShutdownAt(0),
      lastProvisionedSsid(),
      provisioningState(ProvisioningState::Idle),
      provisioningMessage("Ready to configure.") {}

void ProvisioningManager::begin() {
    if (config.ledPin >= 0) {
        pinMode(config.ledPin, OUTPUT);
    }
    setStatusLED(HIGH);

    String savedSsid, savedPassword;

    if (loadCredentials(savedSsid, savedPassword)) {
        Serial.println("Found saved credentials. Attempting connection.");
        initSTA(savedSsid, savedPassword);
    } else {
        Serial.println("No saved credentials found. Starting provisioning AP.");
        initProvisioning();
    }
}

void ProvisioningManager::loop() {
    handleSerialCommands();

    if (pendingPortalShutdown && millis() >= portalShutdownAt) {
        Serial.println("Provisioning success acknowledged. Shutting down AP.");
        enterOperationalMode();
    }

    if (isProvisioningMode) {
        dnsServer.processNextRequest();
        server.handleClient();

        static bool apLedState = false;
        static unsigned long lastApToggle = 0;
        const unsigned long now = millis();
        if (now - lastApToggle >= config.apHeartbeatIntervalMs) {
            lastApToggle = now;
            apLedState = !apLedState;
            setStatusLED(apLedState ? HIGH : LOW);
        }
    } else {
        const unsigned long now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            if (now - lastReconnectAttempt >= config.reconnectBackoffMs) {
                lastReconnectAttempt = now;
                Serial.println("Connection lost. Attempting reconnect...");
                String savedSsid, savedPassword;
                if (loadCredentials(savedSsid, savedPassword)) {
                    if (connectToNetwork(savedSsid, savedPassword, WIFI_STA)) {
                        Serial.println("Reconnect successful.");
                        lastProvisionedSsid = savedSsid;
                        enterOperationalMode();
                    } else {
                        Serial.println("Reconnect failed. Returning to provisioning mode.");
                        initProvisioning();
                    }
                } else {
                    Serial.println("No stored credentials. Returning to provisioning mode.");
                    initProvisioning();
                }
            }
        } else {
            static bool staLedState = false;
            static unsigned long lastStaToggle = 0;
            if (now - lastStaToggle >= config.staHeartbeatIntervalMs) {
                lastStaToggle = now;
                staLedState = !staLedState;
                setStatusLED(staLedState ? HIGH : LOW);
            }
        }
    }

    delay(10);
}

void ProvisioningManager::handleSerialCommands() {
    if (Serial.available() <= 0) {
        return;
    }

    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.equalsIgnoreCase("reset")) {
        Serial.println("\n--- SERIAL COMMAND RECEIVED: RESET ---");
        resetStoredCredentials();
        Serial.println("Stored Wi-Fi credentials cleared.");

        WiFi.disconnect(true);

        if (isProvisioningMode) {
            server.stop();
            dnsServer.stop();
            WiFi.softAPdisconnect(true);
        }

        Serial.println("Returning to Provisioning Mode.");
        delay(500);
        initProvisioning();
    } else {
        Serial.print("Unknown serial command received: ");
        Serial.println(command);
    }
}

void ProvisioningManager::resetStoredCredentials() {
    preferences.begin(PREF_NAMESPACE, false);
    preferences.clear();
    preferences.end();
}

bool ProvisioningManager::isProvisioning() const {
    return isProvisioningMode;
}

const String& ProvisioningManager::lastProvisionedNetwork() const {
    return lastProvisionedSsid;
}

const ProvisioningConfig& ProvisioningManager::getConfig() const {
    return config;
}

void ProvisioningManager::initProvisioning(bool resetStatus) {
    isProvisioningMode = true;
    Serial.println("\n--- Entering Provisioning Mode (AP) ---");

    setStatusLED(HIGH);
    if (resetStatus) {
        provisioningState = ProvisioningState::Idle;
        provisioningMessage = "Ready to configure.";
        lastProvisionedSsid = "";
    }
    pendingPortalShutdown = false;
    dnsServer.stop();
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAPConfig(config.localIP, config.gateway, config.subnet)) {
        Serial.println("AP Configuration failed!");
        return;
    }
    if (!WiFi.softAP(config.apSsid, config.apPassword)) {
        Serial.println("SoftAP creation failed!");
        return;
    }

    Serial.print("Access Point: ");
    Serial.println(config.apSsid);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    dnsServer.start(53, "*", config.localIP);
    Serial.println("DNS Server started.");

    ensureServerHandlers();
    server.begin();
    Serial.println("HTTP Server started.");
}

void ProvisioningManager::initSTA(const String& ssid, const String& password) {
    Serial.println("\n--- Entering Operational Mode (STA) ---");

    if (connectToNetwork(ssid, password, WIFI_STA)) {
        lastProvisionedSsid = ssid;
        enterOperationalMode();
    } else {
        Serial.println("Connection failed. Returning to provisioning mode.");
        initProvisioning();
    }
}

void ProvisioningManager::setStatusLED(int state) {
    if (config.ledPin >= 0) {
        digitalWrite(config.ledPin, state);
    }
}

void ProvisioningManager::saveCredentials(const String& ssid, const String& password) {
    preferences.begin(PREF_NAMESPACE, false);
    preferences.putString(PREF_KEY_SSID, ssid);
    preferences.putString(PREF_KEY_PASS, password);
    preferences.end();
    Serial.println("Credentials saved to NVS.");
}

bool ProvisioningManager::loadCredentials(String& ssid, String& password) {
    preferences.begin(PREF_NAMESPACE, true);
    ssid = preferences.getString(PREF_KEY_SSID, "");
    password = preferences.getString(PREF_KEY_PASS, "");
    preferences.end();
    return ssid.length() > 0;
}

void ProvisioningManager::ensureServerHandlers() {
    if (serverHandlersRegistered) {
        return;
    }

    server.on("/", HTTP_GET, std::bind(&ProvisioningManager::handleRoot, this));
    server.on("/scan", HTTP_GET, std::bind(&ProvisioningManager::handleScan, this));
    server.on("/save", HTTP_POST, std::bind(&ProvisioningManager::handleSave, this));
    server.on("/status", HTTP_GET, std::bind(&ProvisioningManager::handleStatus, this));
    server.onNotFound(std::bind(&ProvisioningManager::handleNotFound, this));

    serverHandlersRegistered = true;
}

void ProvisioningManager::enterOperationalMode() {
    isProvisioningMode = false;
    dnsServer.stop();
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    lastReconnectAttempt = millis();
    pendingPortalShutdown = false;
    provisioningState = ProvisioningState::Success;
    provisioningMessage = lastProvisionedSsid.length()
        ? "Provisioning successful for " + lastProvisionedSsid
        : "Provisioning successful.";
    setStatusLED(HIGH);
    notifyProvisioningSuccess();
}

bool ProvisioningManager::connectToNetwork(const String& ssid, const String& password, wifi_mode_t mode) {
    WiFi.mode(mode);
    WiFi.begin(ssid.c_str(), password.c_str());

    Serial.print("Connecting to Wi-Fi '");
    Serial.print(ssid);
    Serial.print("'");

    const unsigned long start = millis();
    bool ledState = false;

    while (WiFi.status() != WL_CONNECTED && (millis() - start) < config.connectTimeoutMs) {
        delay(100);
        Serial.print(".");
        ledState = !ledState;
        setStatusLED(ledState ? HIGH : LOW);
        handleSerialCommands();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected successfully!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        return true;
    }

    Serial.println("\nConnection failed or timed out.");
    WiFi.disconnect(true);
    return false;
}

void ProvisioningManager::notifyProvisioningSuccess() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Skipping provisioning success broadcast: Wi-Fi not connected.");
        return;
    }

    if (!udpInitialized) {
        if (provisioningUdp.begin(0)) {
            udpInitialized = true;
        } else {
            Serial.println("Failed to initialize UDP socket for provisioning notifications.");
            return;
        }
    }

    uint32_t rawIP = static_cast<uint32_t>(WiFi.localIP());
    uint32_t rawMask = static_cast<uint32_t>(WiFi.subnetMask());
    IPAddress broadcast(rawIP | ~rawMask);

    StaticJsonDocument<256> doc;
    doc["event"] = "provisioning_complete";
    doc["ssid"] = lastProvisionedSsid.length() ? lastProvisionedSsid : WiFi.SSID();
    doc["ip"] = WiFi.localIP().toString();
    const char* hostname = WiFi.getHostname();
    if (hostname && *hostname) {
        doc["device"] = hostname;
    }

    String payload;
    serializeJson(doc, payload);

    provisioningUdp.beginPacket(broadcast, config.broadcastPort);
    provisioningUdp.write(reinterpret_cast<const uint8_t*>(payload.c_str()), payload.length());
    provisioningUdp.endPacket();

    Serial.print("Broadcasted provisioning complete message to ");
    Serial.print(broadcast);
    Serial.print(":");
    Serial.println(config.broadcastPort);
}

void ProvisioningManager::handleRoot() {
    Serial.println("Serving root page.");
    String html = R"raw(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-C6 Wi-Fi Setup</title>
    <style>
        body { font-family: sans-serif; background-color: #f4f7f6; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
        .container { background-color: white; padding: 25px; border-radius: 12px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); width: 90%; max-width: 400px; }
        h1 { color: #2c3e50; font-size: 1.5rem; text-align: center; margin-bottom: 20px; }
        label { display: block; margin-top: 15px; margin-bottom: 5px; font-weight: bold; color: #34495e; }
        select, input[type="password"] { width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 6px; box-sizing: border-box; }
        button { background-color: #3498db; color: white; padding: 12px 20px; border: none; border-radius: 6px; cursor: pointer; width: 100%; font-size: 1rem; transition: background-color 0.3s; margin-top: 10px; }
        button:hover { background-color: #2980b9; }
        #status { text-align: center; margin-top: 20px; padding: 10px; border-radius: 6px; background-color: #ecf0f1; color: #2c3e50; }
        .scan-button { background-color: #2ecc71; }
        .scan-button:hover { background-color: #27ae60; }
        .logo { text-align: center; font-size: 2rem; color: #3498db; margin-bottom: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">&#9974;</div>
        <h1>Wi-Fi Provisioning Portal</h1>
        <form id="wifiForm">
            <label for="ssid">Choose Network (SSID):</label>
            <select id="ssid" name="ssid" required>
                <option value="" disabled selected>Scanning...</option>
            </select>
            
            <button type="button" class="scan-button" onclick="scanNetworks()">&#x21bb; Scan Networks</button>

            <label for="password">Password:</label>
            <input type="password" id="password" name="password" placeholder="Enter network password" required>

            <button type="submit" id="saveBtn">Connect & Save</button>
        </form>
        <div id="status">Ready to configure.</div>
    </div>

    <script>
        const form = document.getElementById('wifiForm');
        const ssidSelect = document.getElementById('ssid');
        const statusDiv = document.getElementById('status');
        const saveBtn = document.getElementById('saveBtn');

        function updateStatus(message) {
            statusDiv.innerHTML = message;
        }

        function scanNetworks() {
            updateStatus('Scanning for networks...');
            saveBtn.disabled = true;
            
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    ssidSelect.innerHTML = '';
                    if (data.length === 0) {
                        const option = document.createElement('option');
                        option.value = '';
                        option.text = 'No networks found';
                        option.disabled = true;
                        option.selected = true;
                        ssidSelect.appendChild(option);
                        updateStatus('No Wi-Fi networks found. Try scanning again.');
                    } else {
                        const defaultOption = document.createElement('option');
                        defaultOption.value = '';
                        defaultOption.text = 'Select an SSID...';
                        defaultOption.disabled = true;
                        defaultOption.selected = true;
                        ssidSelect.appendChild(defaultOption);

                        data.forEach(network => {
                            const option = document.createElement('option');
                            option.value = network.ssid;
                            option.text = `${network.ssid} (${network.rssi} dBm) [${network.auth ? 'Secured' : 'Open'}]`;
                            ssidSelect.appendChild(option);
                        });
                        updateStatus(`Found ${data.length} networks. Select your network.`);
                    }
                    saveBtn.disabled = false;
                })
                .catch(error => {
                    console.error('Scan Error:', error);
                    updateStatus('Network request failed. Check device connectivity or serial console for details.');
                    saveBtn.disabled = false;
                });
        }

        form.addEventListener('submit', function(e) {
            e.preventDefault();
            const selectedSsid = ssidSelect.value;
            const password = document.getElementById('password').value;

            if (!selectedSsid || !password) {
                 updateStatus('Please select an SSID and enter the password.');
                 return;
            }

            updateStatus('Attempting to connect and save credentials...');
            saveBtn.disabled = true;

            const formData = new URLSearchParams();
            formData.append('ssid', selectedSsid);
            formData.append('password', password);

            fetch('/save', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded'
                },
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'success') {
                    updateStatus('Configuration successful! The device is now rebooting/switching to your network.');
                } else {
                    updateStatus(`Connection failed: ${data.message}. Please check credentials and try again.`);
                    saveBtn.disabled = false;
                }
            })
            .catch(error => {
                console.error('Save Error:', error);
                updateStatus('A network error occurred while saving. Try again.');
                saveBtn.disabled = false;
            });
        });

        window.onload = scanNetworks;

    </script>
</body>
</html>
)raw";
    server.send(200, "text/html", html);
}

void ProvisioningManager::handleScan() {
    Serial.println("Scanning networks...");
    
    int n = WiFi.scanNetworks(false, true, false, 5000);

    StaticJsonDocument<3072> doc;
    JsonArray networks = doc.to<JsonArray>();

    if (n > 0) {
        for (int i = 0; i < n; ++i) {
            if (WiFi.SSID(i).length() > 0) {
                JsonObject network = networks.createNestedObject();
                if (network.isNull()) {
                    Serial.println("Scan result truncated due to JSON buffer limits.");
                    break;
                }
                network["ssid"] = WiFi.SSID(i);
                network["rssi"] = WiFi.RSSI(i);
                network["auth"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
            }
        }
    }
    
    WiFi.scanDelete();

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    
    server.send(200, "application/json", jsonResponse);
    Serial.print("Scan complete, found ");
    Serial.print(networks.size());
    Serial.println(" unique SSIDs.");
    if (doc.overflowed()) {
        Serial.println("Warning: Serialized scan response exceeded buffer capacity.");
    }
}

void ProvisioningManager::handleSave() {
    DynamicJsonDocument responseDoc(256);

    if (server.method() == HTTP_POST && server.hasArg("ssid") && server.hasArg("password")) {
        String newSsid = server.arg("ssid");
        String newPassword = server.arg("password");
        
        Serial.print("Attempting to connect to: ");
        Serial.println(newSsid);

        provisioningState = ProvisioningState::Connecting;
        provisioningMessage = "Attempting to connect...";

        bool connected = connectToNetwork(newSsid, newPassword, WIFI_AP_STA);

        if (connected) {
            saveCredentials(newSsid, newPassword);
            responseDoc["status"] = "success";
            responseDoc["message"] = "Connected and configuration saved.";
            Serial.println("Configuration successful! Switching to STA mode.");
            provisioningState = ProvisioningState::Success;
            provisioningMessage = "Provisioning successful for " + newSsid;
            lastProvisionedSsid = newSsid;
        } else {
            responseDoc["status"] = "error";
            responseDoc["message"] = "Failed to connect to the network. Please check credentials.";
            Serial.println("Connection failed. Re-initializing AP mode.");
            provisioningState = ProvisioningState::Failure;
            provisioningMessage = "Provisioning failed for " + newSsid;
            lastProvisionedSsid = newSsid;
        }

        String jsonResponse;
        serializeJson(responseDoc, jsonResponse);
        server.send(200, "application/json", jsonResponse);

        if (connected) {
            pendingPortalShutdown = true;
            portalShutdownAt = millis() + config.portalShutdownDelayMs;
        } else {
            initProvisioning(false);
        }
        return;
    } else {
        responseDoc["status"] = "error";
        responseDoc["message"] = "Invalid request or missing parameters.";
    }

    String jsonResponse;
    serializeJson(responseDoc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void ProvisioningManager::handleStatus() {
    DynamicJsonDocument doc(256);
    doc["mode"] = isProvisioningMode ? "ap" : "sta";
    doc["connected"] = WiFi.status() == WL_CONNECTED;
    doc["state"] = provisioningStateToString(provisioningState);
    doc["message"] = provisioningMessage;
    if (doc["connected"]) {
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
    } else if (lastProvisionedSsid.length()) {
        doc["ssid"] = lastProvisionedSsid;
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void ProvisioningManager::handleNotFound() {
    if (server.hostHeader().indexOf("generate_204") != -1 ||
        server.uri().indexOf("hotspot-detect.html") != -1 ||
        server.uri().indexOf("redirect") != -1) {
        
        Serial.println("Detected Captive Portal Check, redirecting...");
        server.sendHeader("Location", "http://" + config.localIP.toString() + "/");
        server.send(302, "text/plain", "");
    } else {
        handleRoot();
    }
}

const char* ProvisioningManager::provisioningStateToString(ProvisioningState state) const {
    switch (state) {
        case ProvisioningState::Idle:
            return "idle";
        case ProvisioningState::Connecting:
            return "connecting";
        case ProvisioningState::Success:
            return "success";
        case ProvisioningState::Failure:
            return "failure";
        default:
            return "unknown";
    }
}
