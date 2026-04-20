#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <WiFiClientSecure.h>
#include <LiquidCrystal_I2C.h>
#include <MQUnifiedsensor.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// WiFi + MQTT
const char* ssid = "Connect";
const char* password = "newkong61";
const char* mqtt_server   = "ad245d63a3cb48ffbfc8a82d5def699e.s1.eu.hivemq.cloud";
const int   mqtt_port     = 8883;                    
const char* mqtt_username = "esp32-sensor";
const char* mqtt_password = "Kartik2004";
const char* topic         = "sensors/esp32/dht";

// Sensors
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
const int MQ135_PIN = 34;
#define RatioMQ135CleanAir 3.6
MQUnifiedsensor MQ135("ESP-32", 3.3, 12, MQ135_PIN, "MQ-135");
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ====================== GPS ANTI-THEFT ======================
HardwareSerial gpsSerial(2);        // UART2
TinyGPSPlus gps;

const float THEFT_RADIUS_METERS = 1.0;   // ← Change later if needed
float homeLat = 0.0;
float homeLon = 0.0;
bool homeSet = false;
unsigned long lastTheftAlert = 0;

// Button, Fire & Buzzer
#define BUTTON_PIN       27
#define FIRE_SENSOR_PIN  26
#define BUZZER_PIN       25

#define SDA_LEFT 21
#define SCL_LEFT 22

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
unsigned long lastDisplayUpdate = 0;

// WiFi + MQTT clients
WiFiClientSecure espClient;
PubSubClient client(espClient);

void scanI2C() {
  Serial.println("Scanning I2C bus (SDA=21, SCL=22)...");
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Found device at 0x");
      Serial.println(addr, HEX);
    }
  }
  Serial.println("Scan done.");
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(MQ135_PIN, INPUT);
  setup_wifi();

  Wire.begin(21, 22);

  display.begin();

  lcd.init();
  lcd.backlight();
  lcd.print("Initializing...");

  scanI2C();

  randomSeed(millis());

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Button on GPIO27 ready (pull-up internal)");

  pinMode(FIRE_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("✅ Fire sensor (GPIO26) + Buzzer (GPIO25) ready");

  // Buzzer test
  Serial.println("🔊 Testing buzzer - should beep 5 times...");
  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }
  Serial.println("✅ Buzzer test finished");

  // GPS
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("✅ NEO-6M GPS initialized on UART2 (pins 16/17). Awaiting satellite fix...");

  // MQ135 calibration
  MQ135.setRegressionMethod(1);
  MQ135.setA(110.47);
  MQ135.setB(-2.862);
  MQ135.init();
  Serial.print("Calibrating MQ135");
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ135.update();
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
    delay(1000);
  }
  MQ135.setR0(calcR0 / 10);
  Serial.println(" done!");

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);

  lcd.clear();
  lcd.print("Ready!");
  delay(2000);
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to private HiveMQ...");
    String clientId = "ESP32-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println(" connected!");
    } else {
      Serial.print(" failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

float haversine(float lat1, float lon1, float lat2, float lon2) {
  float R = 6371000.0;
  float phi1 = lat1 * 3.141592653589793 / 180.0;
  float phi2 = lat2 * 3.141592653589793 / 180.0;
  float deltaPhi = (lat2 - lat1) * 3.141592653589793 / 180.0;
  float deltaLambda = (lon2 - lon1) * 3.141592653589793 / 180.0;

  float a = sin(deltaPhi / 2.0) * sin(deltaPhi / 2.0) +
            cos(phi1) * cos(phi2) *
            sin(deltaLambda / 2.0) * sin(deltaLambda / 2.0);
  float c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return R * c;
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // === PUBLISH every 20 seconds (temp + humidity + CO2) ===
  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 20000) {
    lastMsg = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    MQ135.update();
    float co2_ppm = MQ135.readSensor();

    if (!isnan(h) && !isnan(t)) {
      String payload = "{\"temp\":" + String(t) + 
                       ",\"humidity\":" + String(h) + 
                       ",\"co2_ppm\":" + String(co2_ppm, 1) + "}";

      client.publish(topic, payload.c_str(), 1);
      Serial.println("Published: " + payload);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("T:"); lcd.print(t, 1); lcd.print("C  H:"); lcd.print(h, 1); lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("CO2:"); lcd.print(co2_ppm, 1); lcd.print("ppm");
    }
  }

  // OLED
  if (millis() - lastDisplayUpdate > 500) {
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB14_tr);
    display.drawStr(20, 40, "hello");
    display.sendBuffer();
    lastDisplayUpdate = millis();
  }

  // ============== FIRE DETECTION + BUZZER + GPS COORDINATES ==============
  int fireDetected = digitalRead(FIRE_SENSOR_PIN);
  if (fireDetected == LOW) {
    digitalWrite(BUZZER_PIN, HIGH);

    static unsigned long lastFireAlert = 0;
    if (millis() - lastFireAlert > 10000) {
      lastFireAlert = millis();

      // === FIRE ALERT WITH GPS COORDINATES (for testing) ===
      String alertPayload = "{\"alert\":\"fire\",\"status\":\"detected\",\"location\":\"community_kiosk\"";
      if (gps.location.isValid()) {
        alertPayload += ",\"lat\":" + String(gps.location.lat(), 6) +
                        ",\"lon\":" + String(gps.location.lng(), 6);
      } else {
        alertPayload += ",\"gps\":\"no_fix\"";
      }
      alertPayload += "}";

      client.publish("alerts/fire", alertPayload.c_str());
      Serial.println("🚨 FIRE DETECTED → Published to alerts/fire (with GPS coords)");
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // ====================== GPS ANTI-THEFT ======================
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isValid() && gps.location.isUpdated()) {
    if (!homeSet) {
      homeLat = gps.location.lat();
      homeLon = gps.location.lng();
      homeSet = true;
      Serial.println("🏠 GPS HOME POSITION SET SUCCESSFULLY!");
      Serial.print("Home Lat: "); Serial.println(homeLat, 6);
      Serial.print("Home Lon: "); Serial.println(homeLon, 6);
    }

    if (homeSet) {
      float dist = haversine(homeLat, homeLon, gps.location.lat(), gps.location.lng());
      Serial.print("GPS Distance from home: ");
      Serial.print(dist, 1);
      Serial.println(" m");

      if (dist > THEFT_RADIUS_METERS) {
        if (millis() - lastTheftAlert > 30000) {
          lastTheftAlert = millis();
          String theftPayload = "{\"alert\":\"theft\",\"status\":\"moved\",\"location\":\"community_kiosk\",\"lat\":" +
                                String(gps.location.lat(), 6) + ",\"lon\":" + String(gps.location.lng(), 6) +
                                ",\"distance_m\":" + String(dist, 1) + "}";
          client.publish("alerts/theft", theftPayload.c_str(), 1);
          Serial.println("🚨 THEFT ALERT: Kiosk moved beyond radius!");

          for (int i = 0; i < 3; i++) {
            digitalWrite(BUZZER_PIN, HIGH); delay(200);
            digitalWrite(BUZZER_PIN, LOW);  delay(200);
          }
        }
      }
    }
  }

  // ====================== GPS DEBUG (shows every 5 seconds) ======================
  static unsigned long lastGpsDebug = 0;
  if (millis() - lastGpsDebug > 10000) {
    lastGpsDebug = millis();
    Serial.print("GPS | Sats: ");
    Serial.print(gps.satellites.value());
    Serial.print(" | Valid fix: ");
    Serial.print(gps.location.isValid() ? "YES" : "NO");
    if (gps.location.isValid()) {
      Serial.print(" | Lat: "); Serial.print(gps.location.lat(), 6);
      Serial.print(" Lon: "); Serial.print(gps.location.lng(), 6);
    } else {
      Serial.print(" | Age of last fix: "); Serial.print(gps.location.age());
      Serial.print(" ms");
    }
    Serial.println();
  }

  // ====================== EMERGENCY BUTTON ======================
  static unsigned long lastButtonPress = 0;
  if (digitalRead(BUTTON_PIN) == LOW && (millis() - lastButtonPress > 500)) {
    lastButtonPress = millis();

    static unsigned long lastEmergencyAlert = 0;
    if (millis() - lastEmergencyAlert > 10000) {
      lastEmergencyAlert = millis();

      String emergencyPayload = "{\"alert\":\"emergency\",\"status\":\"pressed\",\"location\":\"community_kiosk\"}";
      client.publish("alerts/emergency", emergencyPayload.c_str(), 1);
      Serial.println("🚨 EMERGENCY BUTTON PRESSED → Published to alerts/emergency");

      digitalWrite(BUZZER_PIN, HIGH);
      delay(300);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  delay(500); 
}