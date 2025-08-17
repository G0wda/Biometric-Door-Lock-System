#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"

// ------------------- WIFI + SERVER -------------------
const char* ssid = "mywifi";
const char* password = "";
const char* serverName = "http://192.19.0.152:5000/logs"; // API endpoint

// ------------------- FINGERPRINT -------------------
#define RX_PIN 16  // R307 TX → ESP32 RX
#define TX_PIN 17  // R307 RX → ESP32 TX

HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// ------------------- NTP TIME (Indian Timezone) -------------------
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 5 * 3600 + 1800; // +5:30 hrs
const int daylightOffset_sec = 0;

// ------------------- LED CONTROL -------------------

// ------------------- TIME STRING -------------------
String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01 00:00:00"; // fallback
  }
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// ------------------- SEND LOG -------------------
void sendLog(int id, String action) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"id\":" + String(id) + 
                  ",\"name\":\"User-" + String(id) + "\"" +
                  ",\"action\":\"" + action + "\"" +
                  ",\"timestamp\":\"" + getTimeString() + "\"}";

    int httpResponseCode = http.POST(json);

    if (httpResponseCode > 0) {
      Serial.println("Log sent: " + String(httpResponseCode));
    } else {
      Serial.println("Error: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
  finger.begin(57600);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  if (finger.verifyPassword()) {
    Serial.println("✅ Fingerprint sensor found!");
  } else {
    Serial.println("❌ Fingerprint sensor NOT found.");
    while (1) delay(1);
  }

  Serial.println("\n--- Fingerprint Menu ---");
  Serial.println("a = Auto Enroll (assigns next free ID)");
  Serial.println("e = Enroll Fingerprint (manual ID)");
  Serial.println("v = Validate Fingerprint");
  Serial.println("l = List Enrolled IDs");
  Serial.println("d = Delete Fingerprint by ID");
  Serial.println("r = Reset (Delete ALL)");
  Serial.println("------------------------\n");
}

// ------------------- LOOP -------------------
void loop() {
  if (Serial.available()) {
    char choice = Serial.read();
    if (choice == '\n' || choice == '\r') return;

    switch (choice) {
      case 'a': {
        int freeID = getNextFreeID();
        if (freeID == -1) Serial.println("❌ Database is full");
        else enrollFinger(freeID);
        break;
      }
      case 'e': {
        Serial.println("Enter ID (1–127):");
        while (!Serial.available());
        int id = Serial.parseInt();
        enrollFinger(id);
        break;
      }
      case 'v': verifyFinger(); break;
      case 'l': listEnrolled(); break;
      case 'd': {
        Serial.println("Enter ID to delete:");
        while (!Serial.available());
        int id = Serial.parseInt();
        deleteFingerprint(id);
        break;
      }
      case 'r': {
        finger.emptyDatabase();
        Serial.println("✅ Database cleared.");
        break;
      }
      default: Serial.println("Invalid choice.");
    }
    delay(500);
  }
}

// ------------------- FUNCTIONS -------------------
int getNextFreeID() {
  for (int id = 1; id <= 127; id++) {
    int p = finger.loadModel(id);
    if (p != FINGERPRINT_OK) return id;
  }
  return -1;
}

void enrollFinger(int id) {
  int p = -1;
  Serial.printf("Enrolling ID #%d\n", id);

  Serial.println("Place finger...");
  while ((p = finger.getImage()) != FINGERPRINT_OK);

  if (finger.image2Tz(1) != FINGERPRINT_OK) { Serial.println("First scan failed");  return; }

  Serial.println("Remove finger...");
  delay(2000);

  Serial.println("Place same finger again...");
  while ((p = finger.getImage()) != FINGERPRINT_OK);

  if (finger.image2Tz(2) != FINGERPRINT_OK) { Serial.println("Second scan failed");  return; }

  if (finger.createModel() != FINGERPRINT_OK) { Serial.println("Fingerprints didn’t match");  return; }

  if (finger.storeModel(id) == FINGERPRINT_OK) {
    Serial.printf("✅ Enrolled successfully at ID %d\n", id);
  } else {
    Serial.println("❌ Error storing fingerprint.");
  }
}

void verifyFinger() {
  int p = -1;
  Serial.println("Place finger...");

  while ((p = finger.getImage()) != FINGERPRINT_OK) {
    if (p == FINGERPRINT_NOFINGER) { delay(100); continue; }
    else { Serial.println("Capture error."); return; }
  }

  if (finger.image2Tz() != FINGERPRINT_OK) { Serial.println("Image conversion failed.");  return; }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.printf("✅ Match found! ID: %d, Confidence: %d\n", finger.fingerID, finger.confidence);
    sendLog(finger.fingerID, "Validated");
  } else {
    Serial.println("❌ No match found.");
  }

}

void listEnrolled() {
  Serial.println("Checking enrolled IDs...");
  int count = 0;
  for (int id = 1; id <= 127; id++) {
    if (finger.loadModel(id) == FINGERPRINT_OK) {
      Serial.printf("ID %d -> Enrolled\n", id);
      count++;
    }
  }
  Serial.printf("Total enrolled: %d\n", count);
}

void deleteFingerprint(uint16_t id) {
  int p = finger.deleteModel(id);
  if (p == FINGERPRINT_OK) Serial.printf("✅ Deleted ID %d\n", id);
  else Serial.printf("❌ Error deleting ID %d (code %d)\n", id, p);
}
