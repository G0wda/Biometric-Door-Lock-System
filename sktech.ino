/* ESP32 Biometric Smart Lock
   - Enroll fingerprints using R307 and store ID->Name mapping in SPIFFS (/users.txt)
   - Fingerprint templates remain on R307 (recommended)
   - Type commands in Serial Monitor:
       e - enroll
       d - delete by ID
       l - list users
   - Continuous scanning: when fingerprint matched, prints ID and name (if known)
*/

#include <HardwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include "FS.h"
#include "SPIFFS.h"

#define FINGER_RX 16  // R307 TX -> ESP32 RX2 (GPIO16)
#define FINGER_TX 17  // R307 RX <- ESP32 TX2 (GPIO17)
#define FINGER_BAUD 57600

HardwareSerial rSerial(2); // UART2
Adafruit_Fingerprint finger(&rSerial);

const char *USERS_FILE = "/users.txt"; // Each line: id,name

// ---------- Utility: SPIFFS file helpers ----------
bool spiffsInit() {
  if (!SPIFFS.begin(true)) { // format = true if first time
    Serial.println("SPIFFS mount failed!");
    return false;
  }
  return true;
}

void appendUserRecord(uint16_t id, const String &name) {
  File f = SPIFFS.open(USERS_FILE, FILE_APPEND);
  if (!f) {
    Serial.println("Failed to open users file for append");
    return;
  }
  f.printf("%u,%s\n", id, name.c_str());
  f.close();
}

void rewriteUserFileExcluding(uint16_t excludeId) {
  // read all and write back excluding excludeId
  File f = SPIFFS.open(USERS_FILE, FILE_READ);
  if (!f) return;
  String out = "";
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    int comma = line.indexOf(',');
    if (comma < 0) continue;
    uint16_t id = line.substring(0, comma).toInt();
    if (id == excludeId) continue;
    out += line + "\n";
  }
  f.close();

  File w = SPIFFS.open(USERS_FILE, FILE_WRITE);
  if (!w) {
    Serial.println("Failed to open users file for rewrite");
    return;
  }
  w.print(out);
  w.close();
}

String findNameById(uint16_t id) {
  File f = SPIFFS.open(USERS_FILE, FILE_READ);
  if (!f) return String();
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    int comma = line.indexOf(',');
    if (comma < 0) continue;
    uint16_t lid = line.substring(0, comma).toInt();
    if (lid == id) {
      String name = line.substring(comma + 1);
      f.close();
      return name;
    }
  }
  f.close();
  return String();
}

void listUsers() {
  Serial.println("Stored users:");
  File f = SPIFFS.open(USERS_FILE, FILE_READ);
  if (!f) {
    Serial.println("(none)");
    return;
  }
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    Serial.println(line);
  }
  f.close();
}

// ---------- Fingerprint helpers ----------
int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;

  return finger.fingerID; // matched ID
}

bool enrollToId(uint16_t id) {
  Serial.printf("Enrolling to ID #%u\n", id);
  int p = -1;
  Serial.println("Place finger on sensor...");
  // first capture
  while (true) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) break;
    else if (p == FINGERPRINT_NOFINGER) delay(200);
    else {
      Serial.print("Error capturing image: "); Serial.println(p);
      return false;
    }
  }
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting image 1");
    return false;
  }
  Serial.println("Remove finger.");
  delay(1500);

  Serial.println("Place same finger again...");
  while (true) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) break;
    else if (p == FINGERPRINT_NOFINGER) delay(200);
    else {
      Serial.print("Error capturing second image: "); Serial.println(p);
      return false;
    }
  }
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting image 2");
    return false;
  }
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.print("Could not create model: "); Serial.println(p);
    return false;
  }
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    Serial.print("Could not store model at ID "); Serial.print(id);
    Serial.print("  error: "); Serial.println(p);
    return false;
  }
  Serial.println("Enrolled successfully!");
  return true;
}

// ---------- Utilities ----------
uint16_t getNextFreeId() {
  // find first ID not present in users file (1..1000)
  bool used[1001] = {0};
  File f = SPIFFS.open(USERS_FILE, FILE_READ);
  if (f) {
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;
      int comma = line.indexOf(',');
      if (comma < 0) continue;
      uint16_t id = line.substring(0, comma).toInt();
      if (id >= 1 && id <= 1000) used[id] = true;
    }
    f.close();
  }
  for (uint16_t i = 1; i <= 1000; ++i) if (!used[i]) return i;
  return 0; // no free slot
}

// ---------- Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("ESP32 Biometric Enrollment - R307");
  // init SPIFFS
  if (!spiffsInit()) {
    Serial.println("SPIFFS init failed, continuing but cannot store users.");
  }

  // init fingerprint serial & sensor
  rSerial.begin(FINGER_BAUD, SERIAL_8N1, FINGER_RX, FINGER_TX);
  delay(100);
  finger.begin(FINGER_BAUD);

  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor found.");
  } else {
    Serial.println("Fingerprint sensor NOT found. Check wiring.");
    // still continue to allow debugging
  }

  // show existing templates count and list
  uint16_t count = finger.getTemplateCount();
  Serial.printf("Templates on sensor: %u\n", count);
  Serial.println("Type 'e' to enroll, 'd' to delete, 'l' to list users.");
}

unsigned long lastScan = 0;

void loop() {
  // serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;
    if (cmd == "e") {
      Serial.println("Enter name for new user (no comma):");
      while (!Serial.available()) delay(10);
      String name = Serial.readStringUntil('\n'); name.trim();
      if (name.length() == 0) { Serial.println("Empty name. Abort."); }
      uint16_t newId = getNextFreeId();
      if (newId == 0) { Serial.println("No free ID available.");  }
      if (enrollToId(newId)) {
        appendUserRecord(newId, name);
        Serial.printf("Saved user '%s' as ID %u\n", name.c_str(), newId);
      } else {
        Serial.println("Enrollment failed.");
      }
    } else if (cmd == "d") {
      Serial.println("Enter numeric ID to delete:");
      while (!Serial.available()) delay(10);
      int id = Serial.parseInt();
      if (id <= 0) { Serial.println("Invalid ID.");  }
      int res = finger.deleteModel(id);
      if (res == FINGERPRINT_OK) {
        rewriteUserFileExcluding(id);
        Serial.printf("Deleted ID %d from sensor and storage.\n", id);
      } else {
        Serial.printf("Failed to delete ID %d  (error %d)\n", id, res);
      }
    } else if (cmd == "l") {
      listUsers();
    } else {
      Serial.println("Unknown command. Use e/d/l");
    }
  }

  // periodic scan for fingerprint
  if (millis() - lastScan > 700) {
    lastScan = millis();
    int id = getFingerprintID();
    if (id > 0) {
      Serial.printf("Matched ID %d\n", id);
      String name = findNameById(id);
      if (name.length()) {
        Serial.printf("User: %s\n", name.c_str());
        // TODO: trigger unlock hardware here (relay)
      } else {
        Serial.println("Name not found on ESP. It exists on sensor though.");
      }
      delay(800); // debounce after a match
    }
  }
}
