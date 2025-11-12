#include <WiFi.h>
#include <HTTPClient.h>
#include <ThingSpeak.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

#define DHTPIN 27
#define DHTTYPE DHT22
#define MQ135_PIN 35   // VOC
#define MQ3_PIN   32   // Alcohol
#define MQ137_PIN 34   // Ammonia

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;

// CREDENTIALS CHANGED
const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "password";

// ThingSpeak 
unsigned long CHANNEL_ID = ID;
const char* WRITE_KEY = "API_KEY";

// Render 
String RENDER_URL = "RENDER_URL";
String CATEGORY_URL = "RENDER_URL";

// Timing 
const unsigned long SEND_INTERVAL = 2UL * 60UL * 1000UL; 
unsigned long lastSend = 0;

// Category 
String lastCategory = "not set";

// Connect Wi-Fi 
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("\nConnecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection Failed. Retrying later...");
  }
}

// Fetch Category 
void fetchCategory() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    return;
  }

  HTTPClient http;
  http.begin(CATEGORY_URL);
  int code = http.GET();

  if (code == 200) {
    String response = http.getString();
    Serial.println("Raw response: " + response);

    // Extract only the category name
    int start = response.indexOf(":\"") + 2;
    int end = response.indexOf("\"", start);
    if (start > 1 && end > start) {
      lastCategory = response.substring(start, end);
    } else {
      lastCategory = "unknown";
    }

    Serial.println("Category updated: " + lastCategory);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Category:");
    lcd.setCursor(0, 1);
    lcd.print(lastCategory);
    delay(180000); 
    lcd.clear();

  } else {
    Serial.println("Failed to fetch category, using last known: " + lastCategory);
  }

  http.end();
}
void sendThingSpeak(float t, float h, int alc, int voc, int nh3) {
  ThingSpeak.setField(1, t);
  ThingSpeak.setField(2, h);
  ThingSpeak.setField(3, alc);
  ThingSpeak.setField(4, voc);
  ThingSpeak.setField(5, nh3);
  int code = ThingSpeak.writeFields(CHANNEL_ID, WRITE_KEY);
  Serial.printf("ThingSpeak HTTP code: %d\n", code);
}
void sendRender(float t, float h, int alc, int voc, int nh3) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    return;
  }

  HTTPClient http;
  http.begin(RENDER_URL);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"temperature\":" + String(t) + ",\"humidity\":" + String(h) +
                   ",\"alcohol\":" + String(alc) + ",\"voc\":" + String(voc) +
                   ",\"ammonia\":" + String(nh3) + "}";

  int code = http.POST(payload);
  Serial.printf("Render HTTP code: %d\n", code);

  if (code == 200) {
    Serial.println("Render Response: " + http.getString());
  } else {
    Serial.println("Render POST failed");
  }

  http.end();
}
void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);

  dht.begin();
  lcd.init();
  lcd.backlight();

  connectWiFi();
  ThingSpeak.begin(client);

  Serial.println("\nSystem Ready - AC Compatible (No Baseline Mode)");
  fetchCategory();  
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastCheck = 0;
  if (now - lastCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost. Reconnecting...");
      connectWiFi();
    }
    lastCheck = now;
  }

  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int voc = analogRead(MQ135_PIN);
    int alc = analogRead(MQ3_PIN);
    int nh3 = analogRead(MQ137_PIN);

    Serial.printf("\nTemp: %.1f°C | Hum: %.1f%% | Alcohol:%d | VOC:%d | Ammonia:%d\n",
                  temp, hum, alc, voc, nh3);

    sendThingSpeak(temp, hum, alc, voc, nh3);
    sendRender(temp, hum, alc, voc, nh3);
  }

  delay(200);
}
