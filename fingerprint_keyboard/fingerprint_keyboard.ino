#include "sfm.hpp"
#include "WiFi.h"
#include "WiFiUdp.h"
#include "esp_wifi.h"
#include "esp32-hal-bt.h"

#define SFM_VCC 10
#define SFM_IRQ 0
#define SFM_RX 20
#define SFM_TX 21

#define WIFI_SSID    "WIFI_SSID"
#define WIFI_PASS    "WIFI_PASS"
#define SERVER_HOST  "SERVER_HOST"
#define SERVER_PORT  4444
#define AUTH_TOKEN   "AUTH_TOKEN"
#define XOR_KEY      "XOR_KEY"

const char PASSWORD_1[] = "PASSWORD_1";
const char PASSWORD_2[] = "PASSWORD_2";

SFM_Module SFM(SFM_VCC, SFM_IRQ, SFM_RX, SFM_TX);
WiFiUDP udp;

bool lastTouchState = false;

void sfmPinInt1() {
  SFM.pinInterrupt();
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

bool sendPassword(const char *pwd) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi down");
    return false;
  }
  char msg[128];
  snprintf(msg, sizeof(msg), "%s %s\n", AUTH_TOKEN, pwd);
  uint8_t len = strlen(msg);
  uint8_t keyLen = sizeof(XOR_KEY) - 1;
  for (uint8_t i = 0; i < len; i++) msg[i] ^= XOR_KEY[i % keyLen];
  for (int i = 0; i < 2; i++) {
    udp.beginPacket(SERVER_HOST, SERVER_PORT);
    udp.write((uint8_t *)msg, len);
    udp.endPacket();
    if (i == 0) delay(150);
  }
  return true;
}

void setup() {
  btStop();
  Serial.begin(115200);
  delay(500);

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

  bool currentTouchState = SFM.isTouched();

  if (currentTouchState != lastTouchState) {
    lastTouchState = currentTouchState;

    if (currentTouchState) {
      uint16_t uid = 0;
      SFM.recognition_1vN(uid);

      if (uid != 0) {
        const char *pwd = (uid == 1) ? PASSWORD_1 : (uid == 2) ? PASSWORD_2 : NULL;
        if (pwd == NULL) {
          Serial.print("Matched UID ");
          Serial.print(uid);
          Serial.println(" - no password assigned");
        } else {
          Serial.print("Matched UID ");
          Serial.print(uid);
          Serial.print(" - sending password");
          Serial.println(sendPassword(pwd) ? " ... done" : " ... failed");
        }
      }
    }
  }
}
