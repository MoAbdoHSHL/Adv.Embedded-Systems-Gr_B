# Team_B2
---------------
## Rionela Kovaci
## Christian Percival
## Mohamed Abdo
------------------------
# Project:
## SmartFit — ESP32 Wearable Fitness Tracker

SmartFit is a wearable fitness monitoring device built on the ESP32 platform. It tracks a user's heart rate and step count in real time, provides instant visual and audio feedback through an RGB LED and buzzer, and streams live data over WiFi via MQTT to a live web dashboard.

### System Overview
<img width="1138" height="402" alt="image" src="https://github.com/user-attachments/assets/465c6819-0453-453d-b5b3-e0f7a626535d" />

### Features
- **Heart Rate Monitoring** — Reads an analog pulse sensor and detects heartbeats using peak-detection logic, then computes BPM (beats per minute) with a smoothing/averaging filter to reduce noise.
- **Step Counting** — Uses a tilt switch to detect motion/steps and keeps a running step count.
- **Heart Rate Zones** — Classifies BPM into 5 zones (from resting to danger) and reflects the current zone through RGB LED colors and buzzer alerts.
- **Danger Alerts** — Automatically triggers an alert when BPM exceeds a critical threshold.
- **Wireless Connectivity** — Connects to WiFi and publishes vitals, step count, and alerts to an MQTT broker for remote monitoring.
- **Live Web Dashboard** — A browser-based interface subscribes to the broker over WebSocket and displays BPM, HR zone, step count, and danger alerts in real time.

### Hardware
| Component            | Pin      |
|-----------------------|----------|
| Heart Rate Sensor     | A0       |
| Tilt Switch (Steps)   | GPIO 5   |
| RGB LED — Red         | GPIO 12  |
| RGB LED — Green       | GPIO 11  |
| RGB LED — Blue        | GPIO 10  |
| Buzzer                | GPIO 6   |

### Heart Rate Zones
| Zone | BPM Range   | LED Color | Buzzer     | Dashboard Label |
|------|-------------|-----------|------------|------------------|
| 1    | < 100       | Blue      | Off        | Rest             |
| 2    | 100–119     | Green     | Off        | Fat Burn         |
| 3    | 120–139     | Yellow    | Off        | Cardio           |
| 4    | 140–159     | Orange    | Short beep | Hard             |
| 5    | ≥ 160       | Red       | On + Danger Alert | Maximum   |

### MQTT Topics
- `smartfit/vitals/bpm` — current averaged BPM
- `smartfit/vitals/zone` — current heart rate zone (1–5)
- `smartfit/motion/steps` — total step count
- `smartfit/alerts/danger` — triggered when BPM reaches the danger zone

### Software Requirements

**Firmware (ESP32)**
- Arduino IDE (or PlatformIO)
- Libraries: `WiFi.h`, `PubSubClient.h`

**Broker**
- Mosquitto MQTT broker, configured to accept both:
  - standard MQTT on port `1883` (for the ESP32)
  - MQTT over WebSocket on port `9001` (for the web dashboard)

**Web Dashboard**
- Plain HTML/CSS/JS, no build step required
- Uses the [MQTT.js](https://github.com/mqttjs/MQTT.js) library (loaded via CDN) to connect over WebSocket

### Setup

1. **Broker** — Install Mosquitto and enable a WebSocket listener on port `9001` alongside the default `1883` listener.
2. **Firmware** — Update the WiFi credentials (`ssid`, `password`) and MQTT broker address (`mqtt_server`) in the ESP32 source code, then flash it to the board.
3. **Dashboard** — Open `index.html` in a browser (or serve it from any web server) and set `brokerIP` to your broker's IP address. It will auto-connect and subscribe to `smartfit/#`.
4. **Verify** — Move/tilt the device and check a pulse on the sensor; BPM, zone, and step count should update live on the dashboard.

### Dashboard Preview

The dashboard shows:
- Large live **BPM** reading
- Current **HR Zone** badge (color-coded, matching the LED)
- Live **step count**
- A **danger alert banner** that appears automatically when Zone 5 is reached
