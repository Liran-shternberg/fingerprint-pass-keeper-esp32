#include "sfm.hpp"
#include "WiFi.h"
#include "WiFiUdp.h"
#include "esp_wifi.h"
#include "esp32-hal-bt.h"

#define SFM_VCC 10
#define SFM_IRQ 0
#define SFM_RX 20
#define SFM_TX 21
#define LED_PIN 8
#define TOUCH_DEBOUNCE_MS 50

#include "secrets.h"
#include "cipher.h"

#define SERVER_PORT  4444

SFM_Module SFM(SFM_VCC, SFM_IRQ, SFM_RX, SFM_TX);
WiFiUDP udp;

bool lastTouchState = false;
bool rawTouchState = false;
bool stableTouchState = false;
unsigned long lastTouchChange = 0;

void sfmPinInt1() {
  SFM.pinInterrupt();
}

unsigned long blinkTimer = 0;
uint8_t blinkRemaining = 0;
bool blinkOn = false;

void startBlink(uint8_t times) {
  blinkOn = true;
  blinkRemaining = times * 2;
  digitalWrite(LED_PIN, LOW);
  blinkTimer = millis();
}

void updateBlink() {
  if (blinkRemaining == 0) return;
  unsigned long now = millis();
  if (now - blinkTimer >= 200) {
    blinkTimer = now;
    blinkOn = !blinkOn;
    digitalWrite(LED_PIN, blinkOn ? LOW : HIGH);
    blinkRemaining--;
    if (blinkRemaining == 0) digitalWrite(LED_PIN, HIGH);
  }
}

uint8_t regStep1(uint16_t &uid) {
  Serial.println("Put your finger on the sensor");
  while (!SFM.isTouched()) delay(10);
  uint8_t ret = SFM.register_3c3r_1st();
  if (ret != SFM_ACK_SUCCESS) {
    Serial.print("Step 1 failed: 0x");
    Serial.println(ret, HEX);
    return ret;
  }
  Serial.println("Release your finger");
  delay(2000);
  return SFM_ACK_SUCCESS;
}

uint8_t regStep2() {
  Serial.println("Put your finger again");
  while (!SFM.isTouched()) delay(10);
  uint8_t ret = SFM.register_3c3r_2nd();
  if (ret != SFM_ACK_SUCCESS) {
    Serial.print("Step 2 failed: 0x");
    Serial.println(ret, HEX);
    return ret;
  }
  Serial.println("Release your finger");
  delay(2000);
  return SFM_ACK_SUCCESS;
}

uint8_t regStep3(uint16_t &uid) {
  Serial.println("Put your finger a third time");
  while (!SFM.isTouched()) delay(10);
  uid = 0;
  uint8_t ret = SFM.register_3c3r_3rd(uid);
  if (ret == SFM_ACK_SUCCESS && uid != 0) {
    Serial.print("Enrolled UID: ");
    Serial.println(uid);
  } else {
    Serial.print("Step 3 failed: 0x");
    Serial.println(ret, HEX);
  }
  return ret;
}

void enrollFinger() {
  Serial.println("=== Enrollment ===");
  uint16_t uid = 0;
  if (regStep1(uid) != SFM_ACK_SUCCESS) return;
  if (regStep2() != SFM_ACK_SUCCESS) return;
  regStep3(uid);
  Serial.println("=== Done ===");
}

// ponytail: sends a fixed pre-encrypted blob (cipher.h), so the same bytes go
// over the wire every time - replayable by someone already on the LAN.
// Upgrade path: encrypt on the fly with the public key if replay matters.
bool sendCipher(const uint8_t *cipher) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi down");
    return false;
  }
  bool ok = false;
  for (int i = 0; i < 2; i++) {
    if (udp.beginPacket(SERVER_HOST, SERVER_PORT) == 1) {
      udp.write(cipher, CIPHER_LEN);
      if (udp.endPacket() == 1) ok = true;
    }
    if (i == 0) delay(150);
  }
  return ok;
}

void setup() {
  btStop();
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_max_tx_power(WIFI_POWER_8_5dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  SFM.setPinInterrupt(sfmPinInt1);
  SFM.enable();

  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "WiFi up" : "WiFi down");

  Serial.println("Fingerprint keyboard ready");
  Serial.println("'e' enroll, 'd' delete all users");
}

void loop() {
  updateBlink();

  if (Serial.available()) {
    char c = Serial.read();
    while (Serial.available()) Serial.read();
    if (c == 'e' || c == 'E') {
      enrollFinger();
    } else if (c == 'd' || c == 'D') {
      SFM.deleteAllUser();
      Serial.print("Deleted all users. Count: ");
      Serial.println(SFM.getUserCount());
    }
    delay(10);
  }

  bool rawTouch = SFM.isTouched();
  if (rawTouch != rawTouchState) {
    rawTouchState = rawTouch;
    lastTouchChange = millis();
  }
  if (millis() - lastTouchChange >= TOUCH_DEBOUNCE_MS) {
    stableTouchState = rawTouchState;
  }

  if (stableTouchState != lastTouchState) {
    lastTouchState = stableTouchState;

    if (stableTouchState) {
      uint16_t uid = 0;
      SFM.stopAll();
      SFM.recognition_1vN(uid);

      if (uid != 0) {
        startBlink(1);
        const uint8_t *cipher = NULL;
        if (uid == 1) cipher = CIPHER_1;
        else if (uid == 2) cipher = CIPHER_2;
        if (cipher == NULL) {
          Serial.print("Matched UID ");
          Serial.print(uid);
          Serial.println(" - no password assigned");
        } else {
          Serial.print("Matched UID ");
          Serial.print(uid);
          Serial.print(" - sending password");
          Serial.println(sendCipher(cipher) ? " ... done" : " ... failed");
        }
      } else {
        startBlink(2);
      }
    }
  }
}
