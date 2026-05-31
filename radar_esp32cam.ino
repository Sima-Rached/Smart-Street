#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "esp_camera.h"
#include <WebServer.h>
#include <ESP32Servo.h>
#include "html_page.h"

// --- Config Point d'Accès ---
const char* ap_ssid = "Traffic-Cam";
const char* ap_pass = "12345678";

// --- Servo ---
Servo myServo;
const int servoPin  = 14;
int angle           = 90;
int direction       = 1;
unsigned long lastServoMillis = 0;
const int servoInterval = 50;
const int SERVO_MIN = 60;
const int SERVO_MAX = 120;

// --- Capture cooldown ---
unsigned long lastCaptureMillis = 0;
const unsigned long captureDelay = 5000;

// ── Pins AI Thinker ──────────────────────────────────
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22
#define FLASH_GPIO       4
#define PIR_PIN         13

WebServer server(80);
volatile bool isRed  = false;
volatile bool pirFlag = false;

// ── Last infraction photo (heap-allocated) ──
uint8_t* infraBuffer = nullptr;
size_t   infraLen    = 0;
bool     infraReady  = false;
int      infraCount  = 0;

typedef enum  { STATE_RED, STATE_GREEN, STATE_ORANGE } TrafficState;
typedef struct { TrafficState state; } struct_message;

// ── Log buffer ──────────────────────────────────────
const int LOG_MAX = 30;
String    logBuf[LOG_MAX];
int       logHead  = 0;
int       logCount = 0;

void addLog(const String& msg) {
  Serial.println(msg);
  logBuf[logHead] = "[" + String(millis()) + "ms] " + msg;
  logHead  = (logHead + 1) % LOG_MAX;
  if (logCount < LOG_MAX) logCount++;
}

// ── ISR ─────────────────────────────────────────────
void IRAM_ATTR onPIR() { pirFlag = true; }

// ── ESP-NOW callback ─────────────────────────────────
void onDataRecv(const esp_now_recv_info*, const uint8_t* data, int len) {
  struct_message msg;
  memcpy(&msg, data, sizeof(msg));
  bool prev = isRed;
  isRed = (msg.state == STATE_RED);
  if (isRed != prev)
    addLog(isRed ? "ESP-NOW: feu ROUGE." : "ESP-NOW: feu VERT/ORANGE.");
}

// ── Infraction capture ───────────────────────────────
void captureInfraction() {
  if (infraBuffer != nullptr) { free(infraBuffer); infraBuffer = nullptr; }
  infraLen   = 0;
  infraReady = false;

  digitalWrite(FLASH_GPIO, HIGH);
  delay(80);

  camera_fb_t* fb = esp_camera_fb_get();

  delay(120);  // total flash = 200 ms
  digitalWrite(FLASH_GPIO, LOW);

  if (!fb) { addLog("ERREUR: camera_fb_get echoue."); return; }

  infraBuffer = (uint8_t*)malloc(fb->len);
  if (infraBuffer) {
    memcpy(infraBuffer, fb->buf, fb->len);
    infraLen   = fb->len;
    infraReady = true;
    infraCount++;
    addLog("!!! INFRACTION #" + String(infraCount) + " - PHOTO PRISE !!!");
  } else {
    addLog("ERREUR: malloc echoue.");
  }
  esp_camera_fb_return(fb);
}

// ── HTTP handlers ────────────────────────────────────

void handlePhoto() {
  if (!infraReady || infraBuffer == nullptr) {
    server.send(204, "text/plain", "Aucune photo disponible.");
    return;
  }
  server.sendHeader("Content-Type",        "image/jpeg");
  server.sendHeader("Content-Disposition", "inline; filename=\"infraction_" + String(infraCount) + ".jpg\"");
  server.sendHeader("Cache-Control",       "no-cache");
  server.sendContent((const char*)infraBuffer, infraLen);
}

void handleStatus() {
  unsigned long elapsed   = millis() - lastCaptureMillis;
  unsigned long remaining = (elapsed < captureDelay) ? (captureDelay - elapsed) : 0;
  String json = "{";
  json += "\"isRed\":"      + String(isRed      ? "true" : "false") + ",";
  json += "\"infraReady\":" + String(infraReady ? "true" : "false") + ",";
  json += "\"infraCount\":" + String(infraCount)                    + ",";
  json += "\"cooldownMs\":" + String(remaining);
  json += "}";
  server.send(200, "application/json", json);
}

void handleLog() {
  String out;
  int start = (logCount < LOG_MAX) ? 0 : logHead;
  for (int i = 0; i < logCount; i++)
    out += logBuf[(start + i) % LOG_MAX] + "\n";
  server.send(200, "text/plain", out);
}

void handleTestRed() {
  isRed = (server.arg("v") == "1");
  addLog("[TEST] isRed force: " + String(isRed ? "ROUGE" : "VERT"));
  server.send(200, "text/plain", "OK");
}

void handleTestPir() {
  pirFlag = true;
  addLog("[TEST] pirFlag force.");
  server.send(200, "text/plain", "OK");
}

void handleTestReset() {
  lastCaptureMillis = 0;
  addLog("[TEST] Cooldown remis a zero.");
  server.send(200, "text/plain", "OK");
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

// ── Setup ────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2400);
  myServo.write(angle);

  pinMode(PIR_PIN, INPUT);
  pinMode(FLASH_GPIO, OUTPUT);
  digitalWrite(FLASH_GPIO, LOW);
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), onPIR, RISING);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0  = Y2_GPIO_NUM;  config.pin_d1  = Y3_GPIO_NUM; config.pin_d2  = Y4_GPIO_NUM;
  config.pin_d3  = Y5_GPIO_NUM;  config.pin_d4  = Y6_GPIO_NUM; config.pin_d5  = Y7_GPIO_NUM;
  config.pin_d6  = Y8_GPIO_NUM;  config.pin_d7  = Y9_GPIO_NUM; config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM; config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 10;             config.fb_count     = 1;
  esp_camera_init(&config);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_pass, 1);
  addLog("WiFi AP actif. IP: " + WiFi.softAPIP().toString());

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onDataRecv);
    addLog("ESP-NOW initialise.");
  } else {
    addLog("ERREUR: ESP-NOW init echouee.");
  }

  server.on("/",                   HTTP_GET,  handleRoot);
  server.on("/photo",              HTTP_GET,  handlePhoto);
  server.on("/status",             HTTP_GET,  handleStatus);
  server.on("/log",                HTTP_GET,  handleLog);
  server.on("/test/red",           HTTP_POST, handleTestRed);
  server.on("/test/pir",           HTTP_POST, handleTestPir);
  server.on("/test/resetcooldown", HTTP_POST, handleTestReset);
  server.begin();
  addLog("Serveur HTTP demarre.");
}

// ── Loop ─────────────────────────────────────────────
void loop() {
  server.handleClient();
  unsigned long now = millis();

  // Servo sweep
  if (now - lastServoMillis >= servoInterval) {
    lastServoMillis = now;
    angle += direction;
    myServo.write(angle);
    if (angle >= SERVO_MAX || angle <= SERVO_MIN) direction *= -1;
  }

  // PIR + infraction logic
  if (pirFlag) {
    pirFlag = false;

    if (isRed && (now - lastCaptureMillis >= captureDelay)) {
      lastCaptureMillis = now;
      captureInfraction();
    } else if (isRed) {
      unsigned long remaining = captureDelay - (now - lastCaptureMillis);
      addLog("Mouvement detecte - cooldown actif: " + String(remaining / 1000) + "s restant.");
    }
    // Feu vert : ignore silencieusement
  }
}
