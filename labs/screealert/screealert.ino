#include <Arduino.h>
#include <Wire.h>
#include <GyverOLED.h>
#include <SensirionI2cSen66.h>
#include "esp_sleep.h"
#include "iicons_8x8.h"  

// ================= HARDWARE =================
GyverOLED<SSD1306_128x64> oled;  
SensirionI2cSen66 airSensor;

const int buttonPin = 7;
const int battery_adc_pin = 6;
const int I2C_SDA_PIN = 19;
const int I2C_SCL_PIN = 20;

// ================= BATTERY =================
const float R1 = 220.0;
const float R2 = 100.0;
const float voltage_divider_ratio = (R1 + R2) / R2;
const float lipo_max_voltage = 4.2;
const float lipo_min_voltage = 3.3;

// ================= MEASUREMENT =================
const int maxReadCount = 12;
const uint64_t sleepTime = 13ULL * 60ULL * 1000000ULL;
int readCount = 0;

float massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0, massConcentrationPm10p0;
float ambientHumidity, ambientTemperature, vocIndex, noxIndex;
uint16_t co2;

float batteryVoltage = 0;
float batteryPercentage = 0;

// ================= STATE =================
int screenIndex = 0;
int oldValue = LOW;
unsigned long lastReadTime = 0;
const long readInterval = 10000;

int alertCode = 0;
bool isAlertActive = false;
unsigned long alertStartTime = 0;
int previousScreenIndex = 0;

// ================= BUTTON DEBOUNCE =================
unsigned long lastButtonTime = 0;
const unsigned long debounceDelay = 50;

// ================= PROTOTYPES =================
void displayMessage(String line1, String line2);
void drawBattery(byte percent);
void readSensorData();
void readBattery();
void showScreen1();
void showScreen2();
void showScreen3();
void showScreen4();
void showAlertScreen();


void drawIconIf(bool condition, uint8_t iconIndex, uint8_t &x, uint8_t y, int yOffset) {
  if (condition) {
    oled.drawBitmap(x, y + yOffset, icons_8x8[iconIndex], 8, 8);
    x += 8 + 4;  // ICON_SIZE + SPACING_X
  }
}

void drawTopBar() {         //icon 
  const int Y_OFFSET = 0;
  uint8_t x = 0;
  uint8_t y = 64 - 58;   // สำหรับจอ 128x64 → วาดตรงขอบบนจริง ๆ

  // เงื่อนไขว่าจะแสดงไอคอนหรือไม่
  bool showBluetooth = true;
  bool showSignal    = true;
  bool showNoSignal  = true;
  bool showLink      = true;
  bool showWarning   = true;

  // เรียงวาดจากซ้ายไปขวา
  drawIconIf(showBluetooth, 8,   x, y, Y_OFFSET);
  drawIconIf(showSignal,    51,  x, y, Y_OFFSET);
  drawIconIf(showNoSignal,  126, x, y, Y_OFFSET);
  drawIconIf(showLink,      41,  x, y, Y_OFFSET);
  drawIconIf(showWarning,   24,  x, y, Y_OFFSET);

  // วาดแบตเตอรี่ที่มุมขวา
  oled.setCursorXY(110, 6);
  drawBattery((byte)batteryPercentage);
}

// =========================================================================
//  SETUP
// =========================================================================

void setup() {
    Serial.begin(115200);
    pinMode(buttonPin, INPUT);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    oled.init();
    oled.clear();

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT1) {
        displayMessage("System", "Starting...");
        delay(1000);

        airSensor.begin(Wire, SEN66_I2C_ADDR_6B);
        uint16_t error = airSensor.deviceReset();
        if (error) {
            displayMessage("ERROR:", "Sensor Reset");
            while (true);
        }

        error = airSensor.startContinuousMeasurement();
        if (error) {
            displayMessage("ERROR:", "Sensor Start");
            while (true);
        }

        displayMessage("Setup OK!", "Measuring...");
        delay(2000);
    } else {
        airSensor.begin(Wire, SEN66_I2C_ADDR_6B);
        airSensor.startContinuousMeasurement();
    }
}

// =========================================================================
//  LOOP
// =========================================================================
void loop() {
    // Serial command
    String command = "";
    if (Serial.available()) {
        command = Serial.readStringUntil('\n');
        command.trim();
    }

    // อ่านแบตเตอรี่
    readBattery();

    // ตั้ง alert จากแบตหรือ PM2.5
    if (!isAlertActive) {
        if (batteryPercentage <= 20) alertCode = 2;
        else if (massConcentrationPm2p5 > 37.5) alertCode = 1;
    }

    // Serial command
    if (command == "error1") alertCode = 1;
    else if (command == "error2") alertCode = 2;
    else if (command == "error3") alertCode = 3;
    else if (command == "error4") alertCode = 4;
    else if (command == "clear") alertCode = 0;

    // Alert handling
    if (alertCode > 0 && !isAlertActive) {
        previousScreenIndex = screenIndex;
        screenIndex = 99;
        isAlertActive = true;
        alertStartTime = millis();
    }

    // Button with debounce
    int newValue = digitalRead(buttonPin);
    if (newValue != oldValue) {
        if ((millis() - lastButtonTime) > debounceDelay && newValue == HIGH) {
            screenIndex = (screenIndex + 1) % 4;
            lastButtonTime = millis();
        }
        oldValue = newValue;
    }

    // Timed sensor reading
    if (millis() - lastReadTime >= readInterval) {
        lastReadTime = millis();
        readSensorData();
    }

    // Display update
    switch(screenIndex) {
        case 0: showScreen1(); break;
        case 1: showScreen2(); break;
        case 2: showScreen3(); break;
        case 3: showScreen4(); break;
        case 99: showAlertScreen(); break;
    }

    // Alert timeout
    if (isAlertActive && (millis() - alertStartTime >= 10000)) {
        isAlertActive = false;
        screenIndex = previousScreenIndex;
        alertCode = 0;
    }

    delay(100);
}

// ================== HELPER FUNCTIONS ==================
void displayMessage(String line1, String line2) {
    oled.clear(); 
    oled.setScale(2);
    oled.setCursorXY(0, 10); oled.print(line1);
    oled.setCursorXY(0, 35); oled.print(line2);
    oled.update();
}

void drawBattery(byte percent) {
    oled.drawByte(0b00111100); oled.drawByte(0b00111100); oled.drawByte(0b11111111);
    for (byte i = 0; i < 100 / 8; i++) {
        if (i < (100 - percent) / 8) oled.drawByte(0b10000001);
        else oled.drawByte(0b11111111);
    }
    oled.drawByte(0b11111111);
}



void readBattery() {
    int analogMilliVolts = analogReadMilliVolts(battery_adc_pin);
    batteryVoltage = (float)analogMilliVolts * (voltage_divider_ratio / 1000.0);
    batteryPercentage = constrain(((batteryVoltage - lipo_min_voltage)/(lipo_max_voltage-lipo_min_voltage))*100,0,100);
}

void readSensorData() {
    uint16_t error;
    error = airSensor.readMeasuredValues(
        massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0, massConcentrationPm10p0,
        ambientHumidity, ambientTemperature, vocIndex, noxIndex, co2
    );
    if (error) return;

    readCount++;
    if (readCount >= maxReadCount) {
        displayMessage("Sleep", "");
        delay(2000);
        airSensor.stopMeasurement();
        esp_sleep_enable_timer_wakeup(sleepTime);
        esp_sleep_enable_ext1_wakeup(1ULL << buttonPin, ESP_EXT1_WAKEUP_ANY_HIGH);
        esp_deep_sleep_start();
    }
}

// ================= SCREENS ==================
void showScreen1() {
    oled.clear(); oled.setScale(1);
    oled.setCursorXY(25,27); oled.print("PM1.0");
    oled.setCursorXY(25,41); oled.print("PM2.5");
    oled.setCursorXY(25,56); oled.print("PM10");
    oled.setCursorXY(79,27); oled.print(massConcentrationPm1p0,0);
    oled.setCursorXY(79,41); oled.print(massConcentrationPm2p5,0);
    oled.setCursorXY(79,56); oled.print(massConcentrationPm10p0,0);

    drawTopBar();
    oled.update();
}

void showScreen2() {
    oled.clear(); oled.setScale(1);
    oled.setCursorXY(39,31); oled.print("VOC "); oled.setCursorXY(75,31); oled.print(vocIndex,0);
    oled.setCursorXY(39,49); oled.print("NOx "); oled.setCursorXY(75,49); oled.print(noxIndex,0);

    drawTopBar();
    oled.update();
}

void showScreen3() {
    oled.clear(); oled.setScale(1);
    oled.setCursorXY(21,27); oled.print("CO2");
    oled.setCursorXY(21,41); oled.print("Temp");
    oled.setCursorXY(21,56); oled.print("Humidity");
    oled.setCursorXY(79,27); oled.print(co2);
    oled.setCursorXY(79,41); oled.print(ambientTemperature,1);
    oled.setCursorXY(79,56); oled.print(ambientHumidity,1);

    drawTopBar();
    oled.update();
}

void showScreen4() {
    oled.clear(); oled.setScale(1);
    oled.setCursorXY(25,31); oled.print("Sen66"); 
    oled.setCursorXY(76,31); oled.print("OK");

    oled.setCursorXY(25,49); oled.print("Battery"); 
    if (batteryPercentage <= 20) {
    oled.setCursorXY(76,49); 
    oled.print("LOW"); 
    } else {
    oled.setCursorXY(76,49); 
    oled.print("OK");
}

    drawTopBar();
    oled.update();
}

void showAlertScreen() {
    oled.clear(); oled.setScale(1);
    oled.setCursorXY(20,30); oled.print("ALERT STATUS");

    switch(alertCode) {
        case 0: oled.setCursorXY(25,35); oled.print("All systems normal"); break;
        case 1: oled.setScale(2); oled.setCursorXY(20,40); oled.print("HIGH PM!"); break;
        case 2: oled.setCursorXY(20,35); oled.print("Low Battery!"); oled.setCursorXY(20,50); oled.print("Charge soon."); break;
        case 3: oled.setCursorXY(20,35); oled.print("RTC Battery Low!"); oled.setCursorXY(20,50); oled.print("Replace RTC."); break;
        case 4: oled.setCursorXY(20,35); oled.print("No Phone Link!"); oled.setCursorXY(20,50); oled.print("Reconnect."); break;
        case 5: oled.setCursorXY(20,35); oled.print("Reset Device"); oled.setCursorXY(20,50); oled.print("Reconnect."); break;
    }

    drawTopBar();
    oled.update();
}

