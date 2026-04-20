#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

// ====== CAMERA CONFIG (AI THINKER) ======
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ====== WIFI ======
const char* ssid = "Connect";
const char* password = "newkong61";

// ====== YOUR BACKEND URL ======
const char* serverURL = "https://kwf1lz9w-3000.inc1.devtunnels.ms/upload-image";

// ====== SEND PHOTO FUNCTION ======
void sendPhoto(camera_fb_t* fb) {
  if (WiFi.status() != WL_CONNECTED || !fb) {
    Serial.println("❌ WiFi not connected or frame invalid");
    return;
  }

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "image/jpeg");

  int response = http.POST(fb->buf, fb->len);

  if (response > 0) {
    Serial.printf("✅ Photo sent! Response: %d\n", response);
  } else {
    Serial.printf("❌ Upload failed: %s\n", http.errorToString(response).c_str());
  }

  http.end();
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);

  // WiFi connect
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi connected!");
  Serial.println(WiFi.localIP());

  // Camera config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // 🔥 Stable settings
  config.frame_size = FRAMESIZE_QVGA;   // 320x240
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // Init camera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("❌ Camera init failed!");
    while (true) delay(1000);
  }

  Serial.println("✅ Camera ready!");
}

// ====== LOOP ======
void loop() {
  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("❌ Capture failed");
    delay(2000);
    return;
  }

  Serial.println("📸 Captured image, sending...");

  sendPhoto(fb);

  esp_camera_fb_return(fb);  // VERY IMPORTANT

  Serial.println("✅ Done. Waiting 10 seconds...\n");

  delay(10000);   // ⏱️ 10 seconds
}