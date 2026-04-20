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
const char* ssid     = "Connect";
const char* password = "newkong61";

// ====== BACKEND URL ======
const char* serverURL = "https://kwf1lz9w-3000.inc1.devtunnels.ms/upload-image";

// ====== MOTION DETECTION TUNING ======
const int      PIXEL_DIFF_THRESHOLD   = 30;    // per-channel sensitivity (0–255)
const float    MOTION_RATIO_THRESHOLD = 0.12f; // 12% of pixels must differ
const int      MOTION_CONFIRM_FRAMES  = 2;     // consecutive hot frames before upload
const uint32_t UPLOAD_COOLDOWN_MS     = 5000;  // min ms between uploads

// ====== FRAME DIFFERENCE (RGB565) ======
// RGB565: each pixel is 2 bytes — R(5 bits) | G(6 bits) | B(5 bits)
// We extract R, G, B channels and diff each one independently.
int frameDifference(uint8_t* f1, uint8_t* f2, size_t len) {
  int diffCount = 0;
  // Step by 2 bytes = 1 pixel
  for (size_t i = 0; i < len - 1; i += 2) {
    uint16_t px1 = ((uint16_t)f1[i] << 8) | f1[i + 1];
    uint16_t px2 = ((uint16_t)f2[i] << 8) | f2[i + 1];

    // Extract RGB channels
    int r1 = (px1 >> 11) & 0x1F;   int r2 = (px2 >> 11) & 0x1F;
    int g1 = (px1 >>  5) & 0x3F;   int g2 = (px2 >>  5) & 0x3F;
    int b1 =  px1        & 0x1F;   int b2 =  px2        & 0x1F;

    // Scale to 0–255 for fair threshold comparison
    int rDiff = abs(r1 * 8 - r2 * 8);
    int gDiff = abs(g1 * 4 - g2 * 4);
    int bDiff = abs(b1 * 8 - b2 * 8);

    if (rDiff > PIXEL_DIFF_THRESHOLD ||
        gDiff > PIXEL_DIFF_THRESHOLD ||
        bDiff > PIXEL_DIFF_THRESHOLD) {
      diffCount++;
    }
  }
  return diffCount;
}

// ====== CAPTURE JPEG AND UPLOAD ======
// Switches to JPEG, shoots one frame, uploads, switches back to RGB565.
// ====== REUSABLE CAMERA INIT ======
bool initCamera(pixformat_t format, framesize_t size, int fbCount) {
  esp_camera_deinit();
  delay(100);

  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sscb_sda  = SIOD_GPIO_NUM;
  config.pin_sscb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 10000000;
  config.jpeg_quality  = 12;
  config.pixel_format  = format;
  config.frame_size    = size;
  config.fb_count      = fbCount;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("❌ Camera reinit failed!");
    return false;
  }
  return true;
}

// ====== CAPTURE JPEG AND UPLOAD ======
void captureAndSendPhoto() {
  // Reinit with JPEG + larger frame for the upload
  if (!initCamera(PIXFORMAT_JPEG, FRAMESIZE_QVGA, 1)) return;
  delay(300);

  // Flush 3 stale frames
  for (int i = 0; i < 3; i++) {
    camera_fb_t* stale = esp_camera_fb_get();
    if (stale) esp_camera_fb_return(stale);
    delay(50);
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ JPEG capture failed");
  } else if (fb->len < 2 || fb->buf[0] != 0xFF || fb->buf[1] != 0xD8) {
    Serial.println("❌ Invalid JPEG header, discarding");
    esp_camera_fb_return(fb);
  } else {
    Serial.printf("📷 JPEG size: %d bytes\n", fb->len);

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverURL);
      http.addHeader("Content-Type", "image/jpeg");
      int response = http.POST(fb->buf, fb->len);
      if (response > 0)
        Serial.printf("✅ Uploaded! HTTP %d\n", response);
      else
        Serial.printf("❌ Upload failed: %s\n", http.errorToString(response).c_str());
      http.end();
    }
    esp_camera_fb_return(fb);
  }

  // Reinit back to RGB565 QQVGA for motion detection
  initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 2);
  delay(300);
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected! IP: " + WiFi.localIP().toString());

  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sscb_sda  = SIOD_GPIO_NUM;
  config.pin_sscb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 10000000;

  // RGB565 = full color, raw pixels → perfect for diffing
  // QQVGA (160x120) keeps the buffer small and comparison fast
  config.pixel_format  = PIXFORMAT_RGB565;
  config.frame_size    = FRAMESIZE_QQVGA;
  config.jpeg_quality  = 12;
  config.fb_count      = 2;

  // if (esp_camera_init(&config) != ESP_OK) {
  //   Serial.println("❌ Camera init failed!");
  //   while (true) delay(1000);
  // }
  // Serial.println("✅ Camera ready! Watching for motion...");

  if (!initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 2)) {
    while (true) delay(1000);
  }
  Serial.println("✅ Camera ready! Watching for motion...");
}

// ====== LOOP ======
void loop() {
  static camera_fb_t* prevFrame     = NULL;
  static int          motionConfirm = 0;
  static uint32_t     lastUpload    = 0;

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Frame capture failed");
    delay(1000);
    return;
  }

  if (prevFrame != NULL) {
    // Total pixels = len / 2  (each RGB565 pixel is 2 bytes)
    int   totalPixels   = fb->len / 2;
    int   diff          = frameDifference(fb->buf, prevFrame->buf, fb->len);
    float movementRatio = (float)diff / (float)totalPixels;

    Serial.printf("📊 Changed pixels: %d / %d | Ratio: %.4f\n",
                  diff, totalPixels, movementRatio);

    if (movementRatio > MOTION_RATIO_THRESHOLD) {
      motionConfirm++;
      Serial.printf("⚠️  Motion detected! Streak: %d / %d\n",
                    motionConfirm, MOTION_CONFIRM_FRAMES);
    } else {
      motionConfirm = 0;
    }

    if (motionConfirm >= MOTION_CONFIRM_FRAMES &&
        millis() - lastUpload > UPLOAD_COOLDOWN_MS) {

      Serial.println("📸 Motion confirmed! Capturing JPEG and uploading...");

      // Free both buffers before switching format
      esp_camera_fb_return(prevFrame);
      prevFrame = NULL;
      esp_camera_fb_return(fb);
      fb = NULL;

      captureAndSendPhoto();

      lastUpload    = millis();
      motionConfirm = 0;

      delay(500);
      return;
    }

    esp_camera_fb_return(prevFrame);
  }

  prevFrame = fb;
  delay(200); // ~5 fps detection rate
}