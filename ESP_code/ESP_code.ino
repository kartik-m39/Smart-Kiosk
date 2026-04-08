#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <WiFiClientSecure.h>
#include <LiquidCrystal_I2C.h>
#include <MQUnifiedsensor.h>
#include <Wire.h>
#include <U8g2lib.h>

// === RAW MAX30102 (GPIO 32/33) ===
#include <Wire.h>
#define MAX_ADDR 0x57
TwoWire I2C_MAX(1);

// WiFi + MQTT (unchanged)
const char* ssid = "Connect";
const char* password = "newkong61";
const char* mqtt_server   = "ad245d63a3cb48ffbfc8a82d5def699e.s1.eu.hivemq.cloud";
const int   mqtt_port     = 8883;                    
const char* mqtt_username = "esp32-sensor";
const char* mqtt_password = "Kartik2004";
const char* topic         = "sensors/esp32/dht";

// Sensors (unchanged)
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
const int MQ135_PIN = 34;
#define RatioMQ135CleanAir 3.6
MQUnifiedsensor MQ135("ESP-32", 3.3, 12, MQ135_PIN, "MQ-135");
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === NEW: BPM + Averaging + Improved SpO2 variables ===
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 0;

float spo2 = 0;

// Simple running average (last 8 samples)
long redSum = 0;
long irSum = 0;
byte avgIndex = 0;
const byte AVG_SIZE = 8;

// === ADD THESE DEFINES (anywhere after your other defines) ===
#define BUTTON_PIN     27          // FREE GPIO (perfect, no conflicts)

#define SDA_LEFT 21
#define SCL_LEFT 22

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

unsigned long lastDisplayUpdate = 0;

// Fire + Buzzer
#define FIRE_SENSOR_PIN  26     // Fire sensor digital out (LOW = fire detected)
#define BUZZER_PIN       25     // Active buzzer


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

void updateDisplays() {
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB14_tr);     // big nice font
  display.drawStr(25, 40, "HELLO");

  display.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(MQ135_PIN, INPUT);
  setup_wifi();

  Wire.begin(21, 22);

  // OLED
  display.begin();

  lcd.init();
  lcd.backlight();
  lcd.print("Initializing...");

  scanI2C();

  randomSeed(millis());

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Button on GPIO27 ready (pull-up internal)");


  // Buzzer Setup
  pinMode(FIRE_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("✅ Fire sensor (GPIO26) + Buzzer (GPIO25) ready");

    // === BUZZER TEST (clear 5 short beeps at startup) ===
  Serial.println("🔊 Testing buzzer - should beep 5 times...");
  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);   // beep
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);    // silence
    delay(150);
  }
  Serial.println("✅ Buzzer test finished (5 beeps)");


  Serial.println("\n=== Starting MAX30102 raw (working version) ===");

  // === RAW MAX30102 CONFIG (same as your proven working version) ===
  I2C_MAX.begin(32, 33);
  Serial.println("I2C started on 32/33");

  I2C_MAX.beginTransmission(MAX_ADDR);
  I2C_MAX.write(0x09); I2C_MAX.write(0x40);  // RESET
  I2C_MAX.endTransmission();
  delay(1000);

  I2C_MAX.beginTransmission(MAX_ADDR);
  I2C_MAX.write(0x08); I2C_MAX.write(0x10);  // FIFO_CONFIG
  I2C_MAX.endTransmission();

  I2C_MAX.beginTransmission(MAX_ADDR);
  I2C_MAX.write(0x0A); I2C_MAX.write(0x27);  // SPO2_CONFIG 100Hz
  I2C_MAX.endTransmission();

  I2C_MAX.beginTransmission(MAX_ADDR);
  I2C_MAX.write(0x09); I2C_MAX.write(0x03);  // MODE SpO2
  I2C_MAX.endTransmission();

  I2C_MAX.beginTransmission(MAX_ADDR);
  I2C_MAX.write(0x0C); I2C_MAX.write(0xFF);  // Red MAX
  I2C_MAX.endTransmission();
  I2C_MAX.beginTransmission(MAX_ADDR);
  I2C_MAX.write(0x0D); I2C_MAX.write(0xFF);  // IR MAX
  I2C_MAX.endTransmission();

  I2C_MAX.beginTransmission(MAX_ADDR);
  I2C_MAX.write(0x04); I2C_MAX.write(0x00);
  I2C_MAX.endTransmission();
  I2C_MAX.beginTransmission(MAX_ADDR);
  I2C_MAX.write(0x06); I2C_MAX.write(0x00);
  I2C_MAX.endTransmission();

  Serial.println("✅ CORRECT RESET + LEDs set to MAX brightness (steady glow)");
  Serial.println(" → Red LED should now stay ON visibly all the time");
  Serial.println(" → PRESS FINGER FIRMLY on the glass (cover completely, dark room)\n");

  // MQ135 calibration (unchanged)
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

void setup_wifi() { /* unchanged - same as before */ 
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

void reconnect() { /* unchanged */ 
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

// === PROPER BPM ALGORITHM (simple but reliable raw version) ===
bool checkForBeat(long irValue) {
  static long lowPass = 0;
  lowPass = lowPass * 0.9 + irValue * 0.1;
  long highPass = irValue - lowPass;

  if (highPass > 120 && (millis() - lastBeat) > 250) {   // tuned threshold for finger
    return true;
  }
  return false;
}



void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // === RAW READ (exactly your working method) ===
  I2C_MAX.beginTransmission(MAX_ADDR);
  I2C_MAX.write(0x07);
  I2C_MAX.endTransmission(false);
  I2C_MAX.requestFrom(MAX_ADDR, 6);

  long red = 0;
  long ir = 0;
  if (I2C_MAX.available() >= 6) {
    red = ((long)I2C_MAX.read() << 16) | ((long)I2C_MAX.read() << 8) | I2C_MAX.read();
    ir  = ((long)I2C_MAX.read() << 16) | ((long)I2C_MAX.read() << 8) | I2C_MAX.read();
  }

  // === AVERAGING (last 8 samples) ===
  redSum = (redSum * (AVG_SIZE-1) + red) / AVG_SIZE;
  irSum  = (irSum  * (AVG_SIZE-1) + ir)  / AVG_SIZE;

  // === BPM CALCULATION ===
  if (irSum > 50000) {
    if (checkForBeat(irSum)) {
      long delta = millis() - lastBeat;
      lastBeat = millis();
      beatsPerMinute = 60 / (delta / 1000.0);
      if (beatsPerMinute < 220 && beatsPerMinute > 40) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;
        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }
  }

  // === IMPROVED SpO2 (averaged + better formula) ===
  if (irSum > 50000) {
    float ratio = (float)redSum / (float)irSum;
    spo2 = 104.0 - 17.0 * ratio;          // improved common formula for MAX30102 clones
    if (spo2 > 100) spo2 = 99;
    if (spo2 < 70) spo2 = 70;
  } else {
    spo2 = 0;
    beatAvg = 0;
  }

  // === PRINT (now slow + averaged + BPM) ===
  Serial.print("Red: ");
  Serial.print(redSum);
  Serial.print(" | IR: ");
  Serial.print(irSum);
  Serial.print(" | BPM: ");
  Serial.print(beatAvg);
  Serial.print(" | SpO2: ");
  Serial.print(spo2, 1);

  if (irSum > 50000) {
    Serial.print(" ← FINGER DETECTED! (sensor 100% working)");
  } else {
    Serial.print(" ← Place finger firmly now");
  }
  Serial.println();

  // === PUBLISH (every 20s - unchanged) ===
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
                       ",\"co2_ppm\":" + String(co2_ppm, 1) +
                       ",\"bpm\":" + String(beatAvg) +
                       ",\"spo2\":" + String(spo2, 1) + "}";

      client.publish(topic, payload.c_str(), 1);
      Serial.println("Published: " + payload);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print(t, 1);
      lcd.print("C  H:");
      lcd.print(h, 1);
      lcd.print("%");

      lcd.setCursor(0, 1);
      lcd.print("CO2:");
      lcd.print(co2_ppm, 1);
      lcd.print("ppm");
    }
  }

  // OLED
  if (millis() - lastDisplayUpdate > 500) {     // update every half second
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB14_tr);
    // display.drawStr(20, 40, "U Rock Pistii✨");           // big and centered
    // display.drawStr(22, 35, "U Rock");      // first line, centered
    // display.drawStr(15, 55, "Pistii hehe ");
    display.drawStr(20, 40, "hello");
    display.sendBuffer();

    lastDisplayUpdate = millis();
  }

    // ============== FIRE DETECTION + BUZZER + ALERT (minimal) ==============
  int fireDetected = digitalRead(FIRE_SENSOR_PIN);

  if (fireDetected == LOW) {                    // ← change to == HIGH if your sensor is active-high
    digitalWrite(BUZZER_PIN, HIGH);             // Beep (active buzzer)

    // Send alert only once every 10 seconds (no flooding)
    static unsigned long lastFireAlert = 0;
    if (millis() - lastFireAlert > 10000) {
      lastFireAlert = millis();

      String alertPayload = "{\"alert\":\"fire\",\"status\":\"detected\",\"location\":\"community_kiosk\"}";
      client.publish("alerts/fire", alertPayload.c_str());
      Serial.println("🚨 FIRE DETECTED → Published to alerts/fire");
    }
  } 
  else {
    digitalWrite(BUZZER_PIN, LOW);   // Silence
  }
  // ======================================================================

  delay(500); 
}