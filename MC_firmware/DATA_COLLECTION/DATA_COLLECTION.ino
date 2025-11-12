#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ThingSpeak.h>
#include <DHT.h>
#define DHT_PIN 15
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);
const int mq3_pin   = 33;  // Alcohol
const int mq135_pin = 32;  // VOC
const int mq137_pin = 35;  // Ammonia
const char* WIFI_SSID = "WIFI SSID";
const char* WIFI_PASS = "PASSWORD";
WiFiClient tsClient;
unsigned long thingspeakChannelID = ID;
const char* thingspeakApiKey = "API_KEY";

const char* IOT_HUB_HOST = "hostname.net";
const char* DEVICE_ID = "esp32-01";

const char* SAS_TOKEN = "SharedAccessSignature";

WiFiClientSecure httpsClient;
void sendToAzure(float temp, float hum, int mq135, int mq137, int mq3) {
  Serial.println("\n[Azure] Attempting HTTPS POST...");

  if (!httpsClient.connect(IOT_HUB_HOST, 443)) {
    Serial.println("[Azure] Connection failed ");
    return;
  }
  String payload = "{\"temperature\":" + String(temp) +
                   ",\"humidity\":"   + String(hum) +
                   ",\"voc\":"        + String(mq135) +
                   ",\"ammonia\":"    + String(mq137) +
                   ",\"alcohol\":"    + String(mq3) + "}";


  String url = "/devices/" + String(DEVICE_ID) + "/messages/events?api-version=2018-06-30";

  httpsClient.println("POST " + url + " HTTP/1.1");
  httpsClient.println("Host: " + String(IOT_HUB_HOST));
  httpsClient.println("Authorization: " + String(SAS_TOKEN));
  httpsClient.println("Content-Type: application/json");
  httpsClient.println("Content-Length: " + String(payload.length()));
  httpsClient.println("Connection: close");
  httpsClient.println();
  httpsClient.println(payload);
  unsigned long timeout = millis();
  while (httpsClient.connected() && millis() - timeout < 5000) {
    if (httpsClient.available()) {
      String line = httpsClient.readStringUntil('\n');
      Serial.println("[Azure Response] " + line);
    }
  }
  httpsClient.stop();
  Serial.println("[Azure] Data sent ");
}
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(mq3_pin, INPUT);
  pinMode(mq135_pin, INPUT);

  Serial.print("Connecting to WiFi: ");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected ");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  ThingSpeak.begin(tsClient);
  httpsClient.setInsecure();  
}

void loop() {
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  int mq3    = analogRead(mq3_pin);
  int mq135  = analogRead(mq135_pin);
  int mq137  = analogRead(mq137_pin);

  Serial.println("\n=== Sensor Readings ===");
  Serial.printf("Temp: %.1f C | Hum: %.1f %%\n", temp, hum);
  Serial.printf("VOC (MQ135): %d | Ammonia (MQ137): %d | Alcohol (MQ3): %d\n", mq135, mq137, mq3);
  ThingSpeak.setField(1, mq135);
  ThingSpeak.setField(2, mq137);
  ThingSpeak.setField(3, mq3);
  ThingSpeak.setField(4, hum);
  ThingSpeak.setField(5, temp);

  int code = ThingSpeak.writeFields(thingspeakChannelID, thingspeakApiKey);
  if (code == 200) Serial.println("[ThingSpeak] Upload OK ");
  else Serial.println("[ThingSpeak] Error  Code: " + String(code));
  sendToAzure(temp, hum, mq135, mq137, mq3);
  Serial.println("Waiting 5 minutes before next cycle...\n");
  delay(300000);
}
