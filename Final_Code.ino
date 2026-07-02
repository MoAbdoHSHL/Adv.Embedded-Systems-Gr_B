/*
 * ============================================================
 * Adv. Embedded Systems Labs & Project — Gr_B2
 * Rionela Kovaci | Christian Percival | Mohamed Abdo
 * ============================================================
 * SmartFit ESP32 — Wearable fitness tracker
 * Reads heart rate (analog pulse sensor) and step count (tilt switch),
 * computes BPM via peak detection + rolling average, and maps it to
 * 5 heart-rate zones with RGB LED + buzzer feedback.
 * Publishes vitals/steps/alerts over WiFi to an MQTT broker (topics under "smartfit/").
 */
 
#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Rios iphone";
const char* password = "rioriorio";
const char* mqtt_server = "172.20.10.3";

#define HEART_PIN A0
#define TILT_PIN 5
#define LED_RED 12
#define LED_GREEN 11
#define LED_BLUE 10
#define BUZZER_PIN 6

WiFiClient espClient;
PubSubClient client(espClient);

int stepCount = 0;
bool lastTiltState = HIGH;
unsigned long lastReconnect = 0;

const int delayMsec = 60;
unsigned long lastBeatTime = 0;
int rawValue = 0;

#define BPM_BUFFER_SIZE 5
int bpmBuffer[BPM_BUFFER_SIZE];
int bpmIndex = 0;
int bpmCount = 0;

void setLEDColor(int r, int g, int b) {
  analogWrite(LED_RED, r);
  analogWrite(LED_GREEN, g);
  analogWrite(LED_BLUE, b);
}

void setHRZone(int bpm) {
  if (bpm < ((100))) {
    setLEDColor(0,0,255);
    digitalWrite(BUZZER_PIN, LOW);
    client.publish("smartfit/vitals/zone","1");
  }
  else if (bpm < 120) {
    setLEDColor(0,255,0);
    digitalWrite(BUZZER_PIN, LOW);
    client.publish("smartfit/vitals/zone","2");
  }
  else if (bpm < 140) {
    setLEDColor(255,255,0);
    digitalWrite(BUZZER_PIN, LOW);
    client.publish("smartfit/vitals/zone","3");
  }
  else if (bpm < 160) {
    setLEDColor(255,165,0);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    client.publish("smartfit/vitals/zone","4");
  }
  else {
    setLEDColor(255,0,0);
    digitalWrite(BUZZER_PIN, HIGH);
    client.publish("smartfit/vitals/zone","5");
    client.publish("smartfit/alerts/danger","1");
  }
}

int getAverageBPM(int newBpm) {
  if (newBpm < 40 || newBpm > 180) return -1;
  bpmBuffer[bpmIndex % BPM_BUFFER_SIZE] = newBpm;
  bpmIndex++;
  if (bpmCount < BPM_BUFFER_SIZE) bpmCount++;
  int sum = 0;
  for (int i = 0; i < bpmCount; i++) sum += bpmBuffer[i];
  return sum / bpmCount;
}

// thresholds scaled x4 for ESP32's 12-bit ADC
bool heartbeatDetected(int sensorPin, int sampleDelay) {
  static int maxValue = 0;
  static bool isPeak = false;
  static unsigned long lastBeatMs = 0;
  bool result = false;

  rawValue = analogRead(sensorPin);
  rawValue *= (1000 / sampleDelay);

  if (rawValue * 4L < maxValue) maxValue = rawValue * 0.8;

  if (rawValue > maxValue - (4000 / sampleDelay)) {
    if (rawValue > maxValue) maxValue = rawValue;
    if (!isPeak) {
      result = true;
      lastBeatMs = millis();
    }
    isPeak = true;
  } else if (rawValue < maxValue - (12000 / sampleDelay)) {
    isPeak = false;
    maxValue -= (4000 / sampleDelay);
  }

  // If stuck with no beat for 3 seconds, force a clean reset
  if (millis() - lastBeatMs > 3000) {
    maxValue = rawValue;
    isPeak = false;
    lastBeatMs = millis();
    lastBeatTime = 0;   // <-- also reset the BPM timer so the next real beat re-seeds cleanly
  }

  return result;
}

void readSteps() {
  bool currentState = digitalRead(TILT_PIN);
  if (currentState == LOW && lastTiltState == HIGH) {
    stepCount++;
    char stepsStr[8];
    itoa(stepCount, stepsStr, 10);
    client.publish("smartfit/motion/steps", stepsStr);
    Serial.print("Steps: "); Serial.println(stepCount);
  }
  lastTiltState = currentState;
}

void reconnectMQTT() {
  unsigned long now = millis();
  if (now - lastReconnect > 5000) {
    lastReconnect = now;
    String clientId = "SmartFitESP32-" + String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT...");
    if (client.connect(clientId.c_str())) {
      Serial.println("connected!");
    } else {
      Serial.print("failed rc="); Serial.println(client.state());
    }
  }
}

void setup() {
  Serial.begin(9600);
  delay(2000);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(TILT_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  setLEDColor(0, 0, 255);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());

  client.setServer(mqtt_server, 1883);
  client.setKeepAlive(60);
}

void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();

  if (heartbeatDetected(HEART_PIN, delayMsec)) {
    unsigned long now = millis();

    if (lastBeatTime == 0) {
      lastBeatTime = now;
    } else {
      int beatInterval = now - lastBeatTime;
      if (beatInterval > 350 && beatInterval < 2000) {
        lastBeatTime = now;
        int bpm = 60000 / beatInterval;
        int avgBpm = getAverageBPM(bpm);
        if (avgBpm > 0) {
          Serial.print("BPM: "); Serial.println(avgBpm);
          char bpmStr[8];
          itoa(avgBpm, bpmStr, 10);
          client.publish("smartfit/vitals/bpm", bpmStr);
          setHRZone(avgBpm);
        }
      }
    }
  }

  readSteps();
  delay(delayMsec);
}