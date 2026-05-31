#include <ESP8266WiFi.h>
#include <espnow.h>

// --- Pins L298N (D1, D2, D5, D6, D7, D8) ---
#define IN1 D1
#define IN2 D2
#define IN3 D5
#define IN4 D6
#define ENA D7
#define ENB D8

// --- Vitesse max réduite pour meilleur contrôle ---
#define MAX_MOTOR_SPEED 450
#define SIGNAL_TIMEOUT  1000

unsigned long lastRecvTime = 0;
bool robotActif = false;

typedef struct struct_message {
  int x;
  int y;
  int button;
} struct_message;

struct_message receiverData;

// --- Contrôle moteurs ---
void rotateMotor(int rightMotorSpeed, int leftMotorSpeed) {
  // Moteur droit (OUT3/OUT4)
  if (rightMotorSpeed < -50) {
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  } else if (rightMotorSpeed > 50) {
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  } else {
    digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);
    rightMotorSpeed = 0;
  }
  analogWrite(ENB, abs(rightMotorSpeed));

  // Moteur gauche (OUT1/OUT2)
  if (leftMotorSpeed < -50) {
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  } else if (leftMotorSpeed > 50) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  } else {
    digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);
    leftMotorSpeed = 0;
  }
  analogWrite(ENA, abs(leftMotorSpeed));
}

// --- Calcul throttle + steering ---
void throttleAndSteeringMovements() {
  int xVal = receiverData.x;
  int yVal = receiverData.y;

  // Zone morte large pour éviter les mouvements parasites
  if (xVal > 1700 && xVal < 2400) xVal = 2048;
  if (yVal > 1700 && yVal < 2400) yVal = 2048;

  int throttle = map(yVal, 0, 4095, -MAX_MOTOR_SPEED, MAX_MOTOR_SPEED);
  int steering = map(xVal, 0, 4095, -MAX_MOTOR_SPEED, MAX_MOTOR_SPEED);

  int leftMotorSpeed  = constrain(throttle + steering, -MAX_MOTOR_SPEED, MAX_MOTOR_SPEED);
  int rightMotorSpeed = constrain(throttle - steering, -MAX_MOTOR_SPEED, MAX_MOTOR_SPEED);

  rotateMotor(rightMotorSpeed, leftMotorSpeed);
}

// --- Callback réception ESP-NOW ---
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  lastRecvTime = millis();
  memcpy(&receiverData, incomingData, sizeof(receiverData));

  static bool dernierEtatBouton = HIGH;
  if (receiverData.button == LOW && dernierEtatBouton == HIGH) {
    robotActif = !robotActif;
    if (!robotActif) rotateMotor(0, 0);
    delay(50);
  }
  dernierEtatBouton = receiverData.button;

  if (robotActif) {
    throttleAndSteeringMovements();
  } else {
    rotateMotor(0, 0);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);

  // Arrêt complet au démarrage
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);    analogWrite(ENB, 0);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    esp_now_register_recv_cb(OnDataRecv);
    Serial.println("Mode Vitesse Douce pret.");
  }
}

void loop() {
  // Sécurité : arrêt si plus de signal pendant SIGNAL_TIMEOUT ms
  if (millis() - lastRecvTime > SIGNAL_TIMEOUT) {
    rotateMotor(0, 0);
  }
}
