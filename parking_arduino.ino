#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// --- Pins capteurs ultrason ---
const int trigPin  = 2;
const int echoPin  = 3;
const int trigPin2 = 6;
const int echoPin2 = 7;

int ledPin  = 5;
int buzzpin = 8;

// --- Servo barrière ---
Servo barriere;
int servoPin   = 10;
int angleOpen  = 180;
int angleClose = 90;

unsigned long openTime    = 0;
bool barrierOpen          = false;
int  barrierDelay         = 4000;

// --- Mesures ultrason ---
long duration,  distance;
long duration2, distance2;

// --- État parking ---
int  places  = 1;   // places disponibles (max 4)
int  prev_s  = 0;   // 0=idle, 1=entrée en cours, 2=sortie en cours
bool busy    = false;
bool s1_prev = false;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ── Setup ────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  pinMode(ledPin,  OUTPUT);
  pinMode(buzzpin, OUTPUT);

  pinMode(trigPin,  OUTPUT);
  pinMode(echoPin,  INPUT);
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);

  barriere.attach(servoPin);
  barriere.write(angleClose);

  lcd.init();
  lcd.backlight();
}

// ── Loop ─────────────────────────────────────────────
void loop() {

  // ── Lecture S1 (entrée) ──────────────────────────
  digitalWrite(trigPin, LOW);  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH, 30000);
  distance = duration * 0.034 / 2;

  delay(50);

  // ── Lecture S2 (sortie) ──────────────────────────
  digitalWrite(trigPin2, LOW);  delayMicroseconds(2);
  digitalWrite(trigPin2, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin2, LOW);
  duration2 = pulseIn(echoPin2, HIGH, 30000);
  distance2 = duration2 * 0.034 / 2;

  bool s1 = (distance  > 0 && distance  <= 14);
  bool s2 = (distance2 > 0 && distance2 <= 14);

  // LED allumée si détection sur l'un ou l'autre capteur
  digitalWrite(ledPin, s1 || s2);

  // Détection front montant S1
  bool s1_rising = s1 && !s1_prev;
  s1_prev = s1;

  lcd.setCursor(0, 0);
  lcd.print("Places libres: ");

  if (!busy) {

    // ── Arrivée détectée sur S1 ──────────────────
    if (s1_rising && prev_s == 0) {
      if (places == 0) {
        // Parking plein : buzzer + message
        tone(buzzpin, 1000); delay(400); noTone(buzzpin);

        lcd.setCursor(0, 0); lcd.print("                ");
        lcd.setCursor(0, 1); lcd.print("  PARKING FULL  ");
        delay(2000);

        lcd.setCursor(0, 0); lcd.print("Places libres: ");
        lcd.setCursor(0, 1); lcd.print("                ");
        lcd.setCursor(0, 1); lcd.print(places);
      } else {
        // Place disponible : ouverture barrière
        prev_s = 1;
        busy   = true;
        barriere.write(angleOpen);
        openTime    = millis();
        barrierOpen = true;
      }
    }

    // ── Sortie détectée sur S2 ───────────────────
    if (s2 && prev_s == 0) {
      prev_s = 2;
      busy   = true;
      barriere.write(angleOpen);
      openTime    = millis();
      barrierOpen = true;
    }
  }

  // ── Entrée confirmée (S1 → S2) ──────────────────
  if (busy && prev_s == 1) {
    if (s2 && places > 0) {
      places--;
      prev_s = 0;
    }
  }

  // ── Sortie confirmée (S2 → S1) ──────────────────
  if (busy && prev_s == 2) {
    if (s1 && places < 4) {
      places++;
      prev_s = 0;
    }
  }

  // ── Déverrouillage quand la voie est libre ───────
  if (!s1 && !s2 && prev_s == 0) {
    busy = false;
  }

  // ── Fermeture automatique de la barrière ─────────
  if (barrierOpen && millis() - openTime > barrierDelay) {
    barriere.write(angleClose);
    barrierOpen = false;
  }

  // ── Affichage LCD ────────────────────────────────
  lcd.setCursor(0, 1);
  lcd.print(places);

  delay(200);
}
