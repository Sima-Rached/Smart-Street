#include <esp_now.h>
#include <WiFi.h>

// --- Structure des données (doit être identique à la voiture) ---
typedef struct struct_message {
  int x;
  int y;
  int button;
} struct_message;

struct_message data;

// --- Adresse MAC du récepteur (ESP8266 voiture) ---
uint8_t receiverAddress[] = {0x8C, 0xAA, 0xB5, 0x0D, 0x10, 0x83};

// --- Pins joystick ---
#define VRX  34  // Axe X
#define VRY  35  // Axe Y
#define SW   25  // Bouton (clic du joystick)

// --- Callback envoi ---
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nStatut d'envoi : ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Succès" : "Échec");
}

void setup() {
  Serial.begin(115200);

  pinMode(SW, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erreur d'initialisation ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Échec de l'ajout du pair");
    return;
  }
}

void loop() {
  data.x      = analogRead(VRX);
  data.y      = analogRead(VRY);
  data.button = digitalRead(SW);

  Serial.printf("X: %d | Y: %d | Bouton: %d\n", data.x, data.y, data.button);

  esp_now_send(receiverAddress, (uint8_t *)&data, sizeof(data));

  delay(50);  // 50 ms → réactivité maximale
}
