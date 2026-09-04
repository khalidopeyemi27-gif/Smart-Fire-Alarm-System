# Smart IoT Fire Alarm & Emergency Monitoring System

An enterprise-grade, multi-sensor IoT fire detection and alert system built on the **ESP32-S3** microcontroller. The system continuously evaluates atmospheric smoke levels, infrared flame radiation, and ambient ambient temperature/humidity, triggering immediate local audiovisual alarms while streaming telemetry to a **Blynk Cloud** mobile dashboard.

---

## 📸 Overview & Key Features

- **Multi-Modal Hazard Detection:** Evaluates localized threats via MQ-2 Smoke/Gas Sensor, IR Flame Sensor, and DHT22 Temperature Sensor.
- **Non-Blocking Architecture:** Uses `millis()` timers and state machine logic—completely eliminating `delay()` freezes to ensure continuous Wi-Fi telemetry and instantaneous sensor polling.
- **Calibrated Signal Processing:** Features custom debouncing counters and dynamic baseline calibration for the MQ-2 smoke sensor to eliminate false positives caused by atmospheric drift.
- **Dual Visual/Audible Status Indicators:** Hardware status driven by dual state LEDs (Green for Safe, Red for Emergency) and a non-blocking piezo buzzer pattern generator.
- **Cloud Dashboard & Remote Terminal:** Full mobile monitoring integration via Blynk IoT, including real-time gauge feeds, automated event notifications, emergency mute control, and an interactive CLI terminal.
- **Fail-Safe Pin Assignment:** Configured strictly on non-strapping ESP32-S3 GPIO pins to ensure boot reliability and hardware stability.

---

## 🛠️ System Architecture & Pin Mapping

### Pinout Configuration (ESP32-S3)

| Peripheral / Sensor | Interface / Pin Type | ESP32-S3 GPIO | Function / Details |
| :--- | :--- | :--- | :--- |
| **MQ-2 Smoke Sensor** | Analog (ADC1) | `GPIO 1` | Calibrated smoke & gas concentration readout |
| **DHT22 Sensor** | Digital Input | `GPIO 4` | Ambient temperature and relative humidity |
| **IR Flame Sensor** | Digital Input | `GPIO 5` | Active-LOW infrared flame detection |
| **Green LED** | Digital Output | `GPIO 6` | Normal / System Healthy indicator |
| **Red LED** | Digital Output | `GPIO 7` | Hazard Alert / Emergency indicator |
| **I2C LCD Display** | I2C SDA | `GPIO 8` | 16x2 Display Data Line |
| **I2C LCD Display** | I2C SCL | `GPIO 9` | 16x2 Display Clock Line |
| **Piezo Buzzer** | Digital Output | `GPIO 15` | Pulsed non-blocking audible alarm |

---

## 📊 Blynk Virtual Pin Mapping

| Virtual Pin | Direction | Widget Type | Description |
| :--- | :--- | :--- | :--- |
| **`V0`** | Read (MCU $\rightarrow$ App) | Value Display / Gauge | Live Temperature (°C) |
| **`V1`** | Read (MCU $\rightarrow$ App) | Value Display / Gauge | Live Smoke ADC Value |
| **`V2`** | Read (MCU $\rightarrow$ App) | LED / Binary Widget | Flame Detection Status (0 = Safe, 1 = Detected) |
| **`V3`** | Write (App $\rightarrow$ MCU) | Switch / Button | Manual Alarm Mute Override |
| **`V4`** | Read (MCU $\rightarrow$ App) | Label / Status Bar | System Status Text ("SYSTEM NORMAL" vs "FIRE ALERT") |
| **`V5`** | Bidirectional | Terminal Widget | Interactive CLI Console (`STATUS`, `TEST`, `CLEAR`, `HELP`) |

---

## ⚙️ Calibration & Threshold Methodology

### MQ-2 Smoke Sensor Calibration
To account for ambient air quality shifts and heater element burn-in:
1. The sensor requires a **2–3 minute warm-up phase** to achieve thermal stability.
2. Atmospheric baseline in room air stabilizes at $\sim 717$ ADC.
3. A **$+300$ ADC buffer** is applied to derive the active trigger threshold ($1017$ ADC) with hysteresis clearing ($917$ ADC) to prevent rapid toggling near trigger limits.

---

## 🚀 Getting Started

### Prerequisites & Libraries
Ensure you have the **ESP32 Board Support Package (v3.x+)** installed in Arduino IDE, along with the following libraries:
- `WiFi.h` (Built-in ESP32 core)
- `Wire.h` (Built-in ESP32 core)
- `LiquidCrystal_I2C`
- `DHT sensor library` (Adafruit)
- `Blynk` (v1.3.5 or newer)

### Installation & Deployment

1. **Clone the Repository:**
   ```bash
   git clone [https://github.com/YOUR_USERNAME/esp32s3-smart-fire-alarm.git](https://github.com/YOUR_USERNAME/esp32s3-smart-fire-alarm.git)
