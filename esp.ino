#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// WiFi
const char* ssid      = "Oneplus";
const char* password  = "juicywrld";
const char* serverURL = "http://10.200.164.178:8080/upload";

// AI Thinker Pin Map
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

// FreeRTOS Queue
QueueHandle_t alertQueue;

typedef struct {
  char type[20];
  float temp;
  float hum;
  int dist;
} AlertData;

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed!");
    return;
  }
  Serial.println("Camera ready");
}

// Task 1 — Read from Arduino over UART
void uartTask(void* pvParameters) {
  char buf[100];
  int idx = 0;
  while (true) {
    while (Serial2.available()) {
      char c = Serial2.read();
      if (c == '\n') {
        buf[idx] = '\0';
        idx = 0;
        float temp = 0, hum = 0;
        int flame = 1, dist = 0, armed = 1;
        sscanf(buf, "TEMP:%f,HUM:%f,FLAME:%d,DIST:%d,ARMED:%d",
               &temp, &hum, &flame, &dist, &armed);
        Serial.printf("Received → T:%.1f H:%.1f F:%d D:%d A:%d\n",
                      temp, hum, flame, dist, armed);
        if (!armed) continue;
        AlertData alert;
        alert.temp = temp;
        alert.hum  = hum;
        alert.dist = dist;
        if (flame == 0) {
          strcpy(alert.type, "FIRE");
          xQueueSend(alertQueue, &alert, 0);
        } else if (dist > 0 && dist < 30) {
          strcpy(alert.type, "INTRUDER");
          xQueueSend(alertQueue, &alert, 0);
        }
      } else {
        if (idx < 99) buf[idx++] = c;
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// Task 2 — Capture + Send Photo
void cameraTask(void* pvParameters) {
  AlertData alert;
  while (true) {
    if (xQueueReceive(alertQueue, &alert, portMAX_DELAY)) {
      Serial.printf("ALERT: %s - capturing\n", alert.type);
      camera_fb_t* fb = esp_camera_fb_get();
      if (!fb) { Serial.println("Capture failed"); continue; }
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverURL);
        http.addHeader("Content-Type", "image/jpeg");
        http.addHeader("Alert-Type",  alert.type);
        http.addHeader("Temperature", String(alert.temp));
        http.addHeader("Humidity",    String(alert.hum));
        http.addHeader("Distance",    String(alert.dist));
        int code = http.POST(fb->buf, fb->len);
        Serial.printf("Server response: %d\n", code);
        http.end();
      } else {
        Serial.println("WiFi disconnected!");
      }
      esp_camera_fb_return(fb);
    }
  }
}

// Task 3 — WiFi Monitor
void wifiTask(void* pvParameters) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnecting WiFi...");
      WiFi.begin(ssid, password);
      int tries = 0;
      while (WiFi.status() != WL_CONNECTED && tries < 20) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        tries++;
      }
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  // GPIO14=RX, GPIO15=TX
  Serial2.begin(9600, SERIAL_8N1, 14, 15);

  Serial.println("Starting...");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi FAILED");
  }

  initCamera();

  alertQueue = xQueueCreate(5, sizeof(AlertData));
  xTaskCreate(uartTask,   "UARTTask",   4096, NULL, 1, NULL);
  xTaskCreate(cameraTask, "CameraTask", 8192, NULL, 2, NULL);
  xTaskCreate(wifiTask,   "WiFiTask",   4096, NULL, 1, NULL);
}

void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}