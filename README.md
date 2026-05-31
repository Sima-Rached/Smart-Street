# 🚦 Smart Street

Système embarqué de gestion intelligente d'une rue — 5 modules interconnectés via ESP-NOW et WiFi.

---

## 🔌 Modules

### 1. Feu Tricolore — `ESP8266`
Gère le cycle rouge/vert/orange pour voitures et piétons.
- **Bouton piéton** (D4) : coupe la phase verte anticipativement
- **LDR** (A0) : allume automatiquement les LEDs de nuit si luminosité < seuil
- **ESP-NOW** : envoie l'état du feu (ROUGE / VERT / ORANGE) à l'ESP32-CAM à chaque changement

**Pins utilisées :**
| Signal | Pin |
|---|---|
| Feu vert voiture | D7 (GPIO13) |
| Feu orange voiture | D6 (GPIO12) |
| Feu rouge voiture | D5 (GPIO14) |
| Feu vert piéton | D1 (GPIO5) |
| Feu rouge piéton | D2 (GPIO4) |
| Bouton piéton | D4 (GPIO2) |
| LDR | A0 |
| LEDs nuit | D0 (GPIO16) |

---

### 2. Radar Infractions — `ESP32-CAM`
Détecte les véhicules qui brûlent le feu rouge et prend une photo.
- **PIR** (GPIO13) : détecte un mouvement
- **Flash** (GPIO4) : s'allume 200 ms pendant la capture
- **Servo** (GPIO14) : balayage automatique 60°–120°
- **ESP-NOW** : reçoit l'état du feu depuis l'ESP8266
- **Serveur HTTP** (`192.168.4.1`) : dashboard avec photo, logs, et boutons de test

**Endpoints HTTP :**
| Route | Description |
|---|---|
| `GET /` | Dashboard web |
| `GET /photo` | Dernière photo d'infraction (JPEG) |
| `GET /status` | État JSON (feu, cooldown, compteur) |
| `GET /log` | Journal des événements |
| `POST /test/red?v=1\|0` | Forcer état du feu (test) |
| `POST /test/pir` | Simuler détection PIR (test) |
| `POST /test/resetcooldown` | Remettre le cooldown à zéro (test) |

**Cooldown** : 5 secondes minimum entre deux captures pour éviter les doublons.

---

### 3. Manette Joystick — `ESP32`
Télécommande sans fil pour le robot voiture.
- Lit les axes X/Y du joystick (ADC 12 bits, 0–4095)
- Envoie les données toutes les **50 ms** via ESP-NOW
- Bouton joystick (GPIO25) : active/désactive le robot

**Pins utilisées :**
| Signal | Pin |
|---|---|
| Axe X | GPIO34 |
| Axe Y | GPIO35 |
| Bouton | GPIO25 |

---

### 4. Robot Voiture — `ESP8266`
Voiture RC contrôlée par la manette via ESP-NOW.
- Pilotage par **throttle + steering** (mixage différentiel)
- **Zone morte** large (1700–2400) pour éviter les dérives au repos
- Bouton joystick = bascule ON/OFF du robot
- **Timeout** : arrêt automatique si aucun signal reçu depuis 1 seconde

**Vitesse max :** 450 (sur 1023) — réglable via `MAX_MOTOR_SPEED`

**Pins L298N :**
| Signal | Pin |
|---|---|
| IN1, IN2 (moteur gauche) | D1, D2 |
| IN3, IN4 (moteur droit) | D5, D6 |
| ENA (PWM gauche) | D7 |
| ENB (PWM droit) | D8 |

---

### 5. Parking — `Arduino UNO`
Gestion d'un parking 4 places avec barrière automatique.
- **2 capteurs HC-SR04** : S1 (entrée) et S2 (sortie), seuil de détection ≤ 14 cm
- **Servo** : barrière ouverte 4 secondes puis refermeture automatique
- **LCD I2C** (0x27, 16×2) : affiche le nombre de places disponibles
- **Buzzer** : alerte sonore si parking plein
- **LED** (GPIO5) : allumée dès qu'un véhicule est détecté

**Logique de comptage :**
- Entrée confirmée quand S1 → puis S2 détecté : `places--`
- Sortie confirmée quand S2 → puis S1 détecté : `places++`

**Pins utilisées :**
| Signal | Pin |
|---|---|
| Trig/Echo S1 | 2 / 3 |
| Trig/Echo S2 | 6 / 7 |
| LED | 5 |
| Buzzer | 8 |
| Servo barrière | 10 |
| LCD SDA/SCL | A4 / A5 |

---

## 🔗 Architecture de communication

```
┌─────────────────┐   ESP-NOW (état feu)   ┌──────────────────┐
│  ESP8266        │ ─────────────────────► │  ESP32-CAM       │
│  Feu Tricolore  │                        │  Radar           │
└─────────────────┘                        └──────────────────┘

┌─────────────────┐   ESP-NOW (joystick)   ┌──────────────────┐
│  ESP32          │ ─────────────────────► │  ESP8266         │
│  Manette        │                        │  Robot Voiture   │
└─────────────────┘                        └──────────────────┘

┌─────────────────┐
│  Arduino UNO    │   (autonome)
│  Parking        │
└─────────────────┘
```
---

## 📦 Dépendances

| Bibliothèque | Module | Source |
|---|---|---|
| `ESP8266WiFi`, `espnow` | Feu, Voiture | ESP8266 Arduino Core |
| `WiFi`, `esp_now`, `esp_camera` | Radar | ESP32 Arduino Core |
| `ESP32Servo` | Radar | Library Manager |
| `WebServer` | Radar | ESP32 Arduino Core |
| `Wire`, `LiquidCrystal_I2C`, `Servo` | Parking | Library Manager |

---

## ⚙️ Mise en route rapide

1. Modifier les adresses MAC dans les fichiers concernés :
   - `feu_tricolore_esp8266.ino` → `camMAC[]` (MAC du ESP32-CAM)
   - `manette_esp32.ino` → `receiverAddress[]` (MAC du ESP8266 voiture)
2. Flasher chaque module avec l'Arduino IDE (board + port corrects)
3. Allumer dans l'ordre : **CAM en premier** (crée l'AP WiFi), puis **Feu**
4. Le dashboard radar est accessible sur `http://192.168.4.1` depuis un appareil connecté au réseau `Traffic-Cam`
