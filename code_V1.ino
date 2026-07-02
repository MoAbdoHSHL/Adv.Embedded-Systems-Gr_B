#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "###########";
const char* password = "###########";
const char* mqtt_server = "###########"; // ✅ IP instead of .local

#define HEART_PIN A0
#define TILT_PIN 5
#define LED_RED 12
#define LED_GREEN 11
#define LED_BLUE 10

WiFiClient espClient;
PubSubClient client(espClient);

int stepCount = 0;
bool lastTiltState = HIGH;
int rawValue = 0;
int maxValue = 0;
bool isPeak = false;
int beatMsec = 0;
unsigned long lastReconnect = 0;
unsigned long lastLoopTime = 0;
const int delayMsec = 60;

void setLEDColor(int r, int g, int b) {
  analogWrite(LED_RED, r);
  analogWrite(LED_GREEN, g);
  analogWrite(LED_BLUE, b); // ✅ fixed typo: was "ba"
}

void setHRZone(int bpm) {
  if (bpm < 100) { setLEDColor(0,0,255); client.publish("smartfit/vitals/zone","1"); }
  else if (bpm < 120) { setLEDColor(0,255,0); client.publish("smartfit/vitals/zone","2"); }
  else if (bpm < 140) { setLEDColor(255,255,0); client.publish("smartfit/vitals/zone","3"); }
  else if (bpm < 160) { setLEDColor(255,165,0); client.publish("smartfit/vitals/zone","4"); }
  else { setLEDColor(255,0,0); client.publish("smartfit/vitals/zone","5"); client.publish("smartfit/alerts/danger","1"); }
}

bool heartbeatDetected() {
  bool result = false;
  rawValue = analogRead(HEART_PIN);
  rawValue *= (1000 / delayMsec);
  if (rawValue * 4L < maxValue) maxValue = rawValue * 0.8;
  if (rawValue > maxValue - (1000 / delayMsec)) {
    if (rawValue > maxValue) maxValue = rawValue;
    if (!isPeak) result = true;
    isPeak = true;
  } else if (rawValue < maxValue - (3000 / delayMsec)) {
    isPeak = false;
    maxValue -= (1000 / delayMsec);
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

void connectWiFi() {
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // ✅ prevents WiFi dropping
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // ✅ max power for stability
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
}

void reconnectMQTT() {
  unsigned long now = millis();
  if (now - lastReconnect > 5000) {
    lastReconnect = now;
    String clientId = "SmartFitESP32-" + String(random(0xffff), HEX); // ✅ unique ID
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
  delay(3000);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(TILT_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  setLEDColor(0, 0, 255);
  connectWiFi();
  client.setServer(mqtt_server, 1883);
  client.setKeepAlive(60);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { // ✅ reconnect WiFi if dropped
    Serial.println("WiFi lost, reconnecting...");
    connectWiFi();
  }
  if (!client.connected()) reconnectMQTT();
  client.loop(); // ✅ always called, not inside else

  unsigned long now = millis();
  if (now - lastLoopTime >= (unsigned long)delayMsec) {
    lastLoopTime = now;

    if (heartbeatDetected()) {
      int bpm = 60000 / beatMsec;
      if (bpm > 30 && bpm < 200) {
        Serial.print("BPM: "); Serial.println(bpm);
        char bpmStr[8];
        itoa(bpm, bpmStr, 10);
        client.publish("smartfit/vitals/bpm", bpmStr);
        setHRZone(bpm);
      }
      beatMsec = 0;
    }

    readSteps();
    beatMsec += delayMsec;
  }
}
