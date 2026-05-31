#include <ESP8266WiFi.h>
#include <espnow.h>

// ── Pins ────────────────────────────────────────────
const int carGreen  = 13;  // D7
const int carOrange = 12;  // D6
const int carRed    = 14;  // D5

const int pedGreen = 5;   // D1
const int pedRed   = 4;   // D2

const int buttonPin = 2;  // D4

const int ldrPin   = A0;
const int nightLeds = 16; // D0

// ── ESP-NOW ─────────────────────────────────────────
uint8_t camMAC[] = {0x00, 0x4B, 0x12, 0x95, 0x3E, 0x1C}; // MAC du ESP32-CAM

typedef enum { STATE_RED, STATE_GREEN, STATE_ORANGE } TrafficState;

typedef struct {
  TrafficState state;
} struct_message;

struct_message dataToSend;

void onDataSent(uint8_t *mac_addr, uint8_t status) {
  Serial.print("[SENT] ");
  Serial.println(status == 0 ? "OK" : "FAIL");
}

String getStateString(TrafficState state) {
  switch (state) {
    case STATE_RED:    return "ROUGE";
    case STATE_GREEN:  return " VERT";
    case STATE_ORANGE: return " ORANGE";
    default:           return " INCONNU";
  }
}

void sendState(TrafficState s) {
  dataToSend.state = s;
  Serial.println(getStateString(s));
  esp_now_send(camMAC, (uint8_t *)&dataToSend, sizeof(dataToSend));
}

// ── Night lighting ───────────────────────────────────
void updateNightLeds() {
  int ldrValue = analogRead(ldrPin);
  if (ldrValue > 550) {
    digitalWrite(nightLeds, HIGH);
  } else {
    digitalWrite(nightLeds, LOW);
  }
}

// ── Traffic light states ─────────────────────────────
void redState() {
  digitalWrite(carGreen,  LOW);
  digitalWrite(carOrange, LOW);
  digitalWrite(carRed,    HIGH);
  digitalWrite(pedGreen,  HIGH);
  digitalWrite(pedRed,    LOW);
  sendState(STATE_RED);
}

void greenState() {
  digitalWrite(carGreen,  HIGH);
  digitalWrite(carOrange, LOW);
  digitalWrite(carRed,    LOW);
  digitalWrite(pedGreen,  LOW);
  digitalWrite(pedRed,    HIGH);
  sendState(STATE_GREEN);
}

void blinkOrange() {
  digitalWrite(carGreen, LOW);
  digitalWrite(carRed,   LOW);
  sendState(STATE_ORANGE);

  for (int i = 0; i < 10; i++) {
    digitalWrite(carOrange, HIGH); delay(200);
    digitalWrite(carOrange, LOW);  delay(200);
  }
}

// ── Setup ────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(carGreen,  OUTPUT);
  pinMode(carOrange, OUTPUT);
  pinMode(carRed,    OUTPUT);
  pinMode(pedGreen,  OUTPUT);
  pinMode(pedRed,    OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(nightLeds, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Force canal 1 (même canal que le AP de la CAM)
  wifi_promiscuous_enable(1);
  wifi_set_channel(1);
  wifi_promiscuous_enable(0);

  if (esp_now_init() != 0) return;

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(onDataSent);
  esp_now_add_peer(camMAC, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  Serial.println("ESP8266 pret sur Canal 1");
}

// ── Loop ─────────────────────────────────────────────
void loop() {
  // Phase 1 : Feu rouge pour voitures (15 s)
  updateNightLeds();
  Serial.println("\n Phase 1: FEU ROUGE (15 secondes)");
  redState();
  for (int i = 0; i < 150; i++) {  // 150 x 100 ms = 15 s
    updateNightLeds();
    delay(100);
  }

  // Phase 2 : Feu vert pour voitures (15 s max, interruptible)
  Serial.println("\n Phase 2: FEU VERT (15 secondes max)");
  greenState();
  for (int i = 0; i < 150; i++) {
    updateNightLeds();
    delay(100);
    if (i > 10) {
      if (digitalRead(buttonPin) == LOW) break;
    }
  }

  // Phase 3 : Feu orange clignotant
  Serial.println("\n Phase 3: FEU ORANGE");
  blinkOrange();
  Serial.println("ANALOG LDR:");
  Serial.println(analogRead(A0));
  delay(500);
}
