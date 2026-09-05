// =====================================================
// BLYNK CONFIGURATION
// Replace with your actual Blynk Template credentials
// =====================================================
#define BLYNK_TEMPLATE_ID "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "Smart Fire Alarm System"
#define BLYNK_AUTH_TOKEN "YOUR_AUTH_TOKEN"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ---------------- NETWORK CREDENTIALS ----------------
char ssid[] = "YOUR_WIFI_SSID";     // Case-sensitive Hotspot Name
char pass[] = "YOUR_WIFI_PASSWORD"; // Case-sensitive Hotspot Password

// ---------------- ESP32-S3 SAFE PIN DEFINITIONS ----------------
#define MQ2_PIN       1    // Analog Input (MQ-2 Smoke Sensor ADC)
#define DHT_PIN       4    // Digital Input (DHT22 Temp & Humidity)
#define FLAME_PIN     5    // Digital Input (IR Flame Sensor)

#define GREEN_LED     6    // Output (Normal System Status)
#define RED_LED       7    // Output (Fire Alarm Status)

#define LCD_SDA       8    // Hardware I2C SDA
#define LCD_SCL       9    // Hardware I2C SCL

#define BUZZER        15   // Output (Piezo Buzzer)

// ---------------- DHT22 SENSOR INITIALIZATION ----------------
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// ---------------- LCD INITIALIZATION (0x27 I2C Address) ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------------- BLYNK TIMERS & TERMINAL ----------------
BlynkTimer timer;
WidgetTerminal terminal(V5);

// =====================================================
// CALIBRATED SENSOR THRESHOLDS
// =====================================================
const int MQ2_BASELINE_ADC = 717;            // Calibrated clean-air baseline
const int SMOKE_THRESHOLD = 1017;           // Calibrated MQ-2 alarm trigger
const int SMOKE_CLEAR_THRESHOLD = 917;       // Calibrated MQ-2 clear trigger
const int SMOKE_CONFIRMATIONS = 3;           // Alarm debounce count
const int SMOKE_CLEAR_CONFIRMATIONS = 5;     // Clear debounce count

const float TEMPERATURE_THRESHOLD = 50.0;    // High temp trigger in °C

// =====================================================
// SYSTEM STATE VARIABLES
// =====================================================
int smokeConfirmCount = 0;
int smokeClearCount = 0;
bool smokeAlarm = false;

bool manualMute = false;
bool lastFireState = false;

// Non-blocking Buzzer Timing
unsigned long previousBuzzerTime = 0;
bool buzzerState = false;
const unsigned long BUZZER_INTERVAL = 500;

// Non-blocking LCD Update Timing
unsigned long previousLCDTime = 0;
const unsigned long LCD_INTERVAL = 1000;

// Global Sensor Values
float globalTemp = 0.0;
float globalHum = 0.0;
int globalSmokeADC = 0;
int globalSmokePercent = 0;
bool globalFlame = false;
bool globalDhtValid = false;
bool globalHighTemp = false;
bool globalFireDetected = false;

// Function Declarations
int getSmokePercentage(int rawADC);
void updateSmokeAlarm(int smokeValue);
void updateBuzzer(bool alarm);
void processSensors();
void updateLCD();
void printHelp();

// =====================================================
// CONVERT MQ-2 ADC TO SMOKE PERCENTAGE (0% - 100%)
// =====================================================
int getSmokePercentage(int rawADC) {
  const int MAX_ADC = 4095; // ESP32-S3 12-bit ADC Max

  if (rawADC <= MQ2_BASELINE_ADC) return 0;
  if (rawADC >= MAX_ADC) return 100;

  // Map calibrated range above clean-air baseline to 0-100%
  return map(rawADC, MQ2_BASELINE_ADC, MAX_ADC, 0, 100);
}

// =====================================================
// SETUP FUNCTION
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(3000); // USB CDC initialization delay for ESP32-S3

  Serial.println(F("\n[BOOT] Initializing Smart Fire Alarm System..."));

  // Configure Pin Modes
  pinMode(MQ2_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // Set Default Initial States
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);

  // Initialize Sensors & Peripherals
  dht.begin();

  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("SMART FIRE");
  lcd.setCursor(0, 1);
  lcd.print("ALARM SYSTEM");
  delay(2000);
  lcd.clear();

  // Connect to WiFi and Blynk Server
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Configure Non-blocking Sensor Processing Timer (Runs every 1 second)
  timer.setInterval(1000L, processSensors);

  // Clear and initialize Blynk Terminal App
  terminal.clear();
  terminal.println(F("================================="));
  terminal.println(F("  Smart Fire Alarm System Online"));
  terminal.println(F("  Type 'HELP' for command menu"));
  terminal.println(F("================================="));
  terminal.flush();

  Serial.println(F(">>> SUCCESS: SYSTEM FULLY ONLINE <<<\n"));
}

// =====================================================
// MQ-2 SMOKE DEBOUNCE LOGIC
// =====================================================
void updateSmokeAlarm(int smokeValue) {
  if (smokeValue >= SMOKE_THRESHOLD) {
    smokeConfirmCount++;
    smokeClearCount = 0;
    if (smokeConfirmCount >= SMOKE_CONFIRMATIONS) {
      smokeAlarm = true;
      smokeConfirmCount = SMOKE_CONFIRMATIONS;
    }
  } 
  else if (smokeValue <= SMOKE_CLEAR_THRESHOLD) {
    smokeClearCount++;
    smokeConfirmCount = 0;
    if (smokeClearCount >= SMOKE_CLEAR_CONFIRMATIONS) {
      smokeAlarm = false;
      smokeClearCount = SMOKE_CLEAR_CONFIRMATIONS;
    }
  } 
  else {
    smokeConfirmCount = 0;
    smokeClearCount = 0;
  }
}

// =====================================================
// NON-BLOCKING BUZZER CONTROL
// =====================================================
void updateBuzzer(bool alarm) {
  if (!alarm || manualMute) {
    digitalWrite(BUZZER, LOW);
    buzzerState = false;
    return;
  }

  if (millis() - previousBuzzerTime >= BUZZER_INTERVAL) {
    previousBuzzerTime = millis();
    buzzerState = !buzzerState;
    digitalWrite(BUZZER, buzzerState);
  }
}

// =====================================================
// MAIN SENSOR PROCESSING (Runs Every 1 Second)
// =====================================================
void processSensors() {
  // 1. Read MQ-2 Smoke Sensor ADC & Calculate Percentage
  globalSmokeADC = analogRead(MQ2_PIN);
  globalSmokePercent = getSmokePercentage(globalSmokeADC);
  updateSmokeAlarm(globalSmokeADC);

  // 2. Read IR Flame Sensor Digital Value (LOW = Flame Detected)
  int flameState = digitalRead(FLAME_PIN);
  globalFlame = (flameState == LOW);

  // 3. Read Temperature and Humidity from DHT22
  globalTemp = dht.readTemperature();
  globalHum = dht.readHumidity();
  globalDhtValid = !isnan(globalTemp) && !isnan(globalHum);

  // 4. Evaluate Temperature Emergency Threshold
  globalHighTemp = false;
  if (globalDhtValid && (globalTemp >= TEMPERATURE_THRESHOLD)) {
    globalHighTemp = true;
  }

  // 5. Evaluate Composite System Alarm Trigger
  globalFireDetected = smokeAlarm || globalFlame || globalHighTemp;

  // ---------------------------------------------------
  // LIVE LED INDICATOR LOGIC
  // ---------------------------------------------------
  if (globalFireDetected) {
    digitalWrite(GREEN_LED, LOW);   // Turn OFF Normal LED
    digitalWrite(RED_LED, HIGH);    // Turn ON Emergency LED
  } else {
    digitalWrite(GREEN_LED, HIGH);  // Turn ON Normal LED
    digitalWrite(RED_LED, LOW);     // Turn OFF Emergency LED
  }

  // ---------------------------------------------------
  // STREAM LIVE DATA TO BLYNK DASHBOARD
  // ---------------------------------------------------
  if (globalDhtValid) Blynk.virtualWrite(V0, globalTemp); // V0: Temp (°C)
  Blynk.virtualWrite(V1, globalSmokePercent);             // V1: Smoke (%)
  Blynk.virtualWrite(V2, globalFlame ? 1 : 0);            // V2: Flame State

  // Detect Emergency State Transitions for App Logging & Events
  if (globalFireDetected && !lastFireState) {
    lastFireState = true;
    if (!manualMute) {
      Blynk.virtualWrite(V4, "🔥 FIRE ALERT DETECTED!");
      terminal.println(F("[CRITICAL] Fire hazard detected!"));
      terminal.print(F("Smoke Level: ")); terminal.print(globalSmokePercent); terminal.print(F("% (ADC ")); terminal.print(globalSmokeADC); terminal.println(F(")"));
      terminal.print(F("Temp: ")); terminal.print(globalTemp, 1);
      terminal.print(F("C | Flame: ")); terminal.println(globalFlame ? "YES" : "NO");
      terminal.flush();

      Blynk.logEvent("fire_alarm", "ALERT! Fire or gas hazard detected.");
    }
  } else if (!globalFireDetected && lastFireState) {
    lastFireState = false;
    if (!manualMute) {
      Blynk.virtualWrite(V4, "SYSTEM NORMAL");
      terminal.println(F("[INFO] Hazard cleared. System status normal."));
      terminal.flush();
    }
  }

  // Output Live Diagnostics to Serial Monitor
  Serial.println(F("--------------------------------"));
  Serial.print(F("Smoke Level:  ")); Serial.print(globalSmokePercent); Serial.print(F("%  (ADC: ")); Serial.print(globalSmokeADC); Serial.println(F(")"));
  Serial.print(F("Flame IR:     ")); Serial.println(globalFlame ? "DETECTED" : "CLEAR");
  if (globalDhtValid) {
    Serial.print(F("Temp:         ")); Serial.print(globalTemp, 1); Serial.println(F(" °C"));
  }
  Serial.print(F("LED Status:   ")); Serial.println(globalFireDetected ? "RED (ALARM)" : "GREEN (SAFE)");
  Serial.println();
}

// =====================================================
// BLYNK APP INTERACTIVE COMMAND HANDLERS
// =====================================================

// Manual Mute Button Switch (Virtual Pin V3)
BLYNK_WRITE(V3) {
  manualMute = param.asInt();
  if (manualMute) {
    digitalWrite(BUZZER, LOW);
    Blynk.virtualWrite(V4, "ALARM MUTED");
    terminal.println(F("[SYSTEM] Buzzer manually muted via app."));
  } else {
    Blynk.virtualWrite(V4, globalFireDetected ? "🔥 FIRE ALERT DETECTED!" : "SYSTEM NORMAL");
    terminal.println(F("[SYSTEM] Mute released. Auto-monitoring active."));
  }
  terminal.flush();
}

// Interactive Terminal Console (Virtual Pin V5)
BLYNK_WRITE(V5) {
  String cmd = param.asString();
  cmd.trim();
  cmd.toUpperCase();

  terminal.print("User> ");
  terminal.println(cmd);

  if (cmd == "STATUS") {
    terminal.println(F("--- Live Sensor Readout ---"));
    terminal.print(F("Temperature: ")); 
    if (globalDhtValid) { terminal.print(globalTemp, 1); terminal.println(F(" °C")); }
    else { terminal.println(F("DHT ERROR")); }
    
    terminal.print(F("Smoke Level: ")); terminal.print(globalSmokePercent); terminal.print(F("% (ADC ")); terminal.print(globalSmokeADC); terminal.println(F(")"));
    terminal.print(F("Flame IR:    ")); terminal.println(globalFlame ? "DETECTED!" : "SAFE");
    terminal.print(F("LED Reading: ")); terminal.println(globalFireDetected ? "RED (ALARM)" : "GREEN (SAFE)");
  } 
  else if (cmd == "TEST") {
    terminal.println(F("[TEST] Triggering 2-second buzzer test..."));
    terminal.flush();
    digitalWrite(BUZZER, HIGH);
    delay(2000);
    digitalWrite(BUZZER, LOW);
    terminal.println(F("[TEST] Buzzer hardware check complete."));
  } 
  else if (cmd == "CLEAR") {
    terminal.clear();
  } 
  else if (cmd == "HELP") {
    printHelp();
  } 
  else {
    terminal.print(F("Unknown command: '")); terminal.print(cmd); terminal.println(F("'"));
    terminal.println(F("Type 'HELP' for valid commands."));
  }
  terminal.flush();
}

void printHelp() {
  terminal.println(F("--- Command Menu ---"));
  terminal.println(F(" STATUS - Display current sensor and LED readings"));
  terminal.println(F(" TEST   - Sound test alarm for 2 seconds"));
  terminal.println(F(" CLEAR  - Clear terminal screen log"));
  terminal.println(F(" HELP   - Print command menu options"));
}

// =====================================================
// NON-BLOCKING LCD UPDATE
// =====================================================
void updateLCD() {
  if (millis() - previousLCDTime >= LCD_INTERVAL) {
    previousLCDTime = millis();
    lcd.clear();

    if (globalFireDetected) {
      lcd.setCursor(0, 0);
      lcd.print("!! FIRE ALERT !!");
      lcd.setCursor(0, 1);

      if (globalFlame) {
        lcd.print("FLAME DETECTED");
      } else if (smokeAlarm) {
        lcd.print("SMOKE: "); lcd.print(globalSmokePercent); lcd.print("%");
      } else if (globalHighTemp) {
        lcd.print("HIGH TEMP");
      }
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Smoke: "); lcd.print(globalSmokePercent); lcd.print("%");
      lcd.setCursor(0, 1);

      if (globalDhtValid) {
        lcd.print("T:"); lcd.print(globalTemp, 1); lcd.print("C ");
        lcd.print("H:"); lcd.print(globalHum, 0); lcd.print("%");
      } else {
        lcd.print("DHT22 ERROR");
      }
    }
  }
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  Blynk.run();
  timer.run();

  updateBuzzer(globalFireDetected);
  updateLCD();
}
