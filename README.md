# NodeMCU Fingerprint Attendance & Logging System  

This project is a **biometric attendance/logging system** built using an **NodeMCU** microcontroller and an **R307 fingerprint sensor**.  
It allows fingerprint **enrollment, validation, listing, and deletion**, while also logging every authentication event to a **remote server** with timestamps.  

---

## 📌 Overview  

- NodeMCU connects to WiFi and syncs real-time clock via NTP.  
- Users can interact with the fingerprint sensor via the **Serial Menu**.  
- Fingerprints are stored in the **R307 sensor’s flash database**.  
- Logs (with user ID, action, and timestamp) are sent to a **Flask/Python backend** or any API endpoint.  

---

## 🚀 Features  

- Fingerprint enrollment (manual ID or auto-assign).  
- Fingerprint validation (sends log to server if match is found).  
- Delete single fingerprint or reset entire database.  
- List all enrolled fingerprints.  
- Time synchronization via NTP (Indian Standard Time UTC+5:30).  
- REST API logging with JSON.  
- Works with ESP32’s WiFi + HTTPClient libraries.  

---

## 🛠 Hardware Requirements  

- ESP32 Development Board  
- R307 Fingerprint Sensor  
- Breadboard + Jumper Wires  
- (Optional) Buzzer/LED for success/failure feedback  

---

## 🔌 Wiring  

| R307 Sensor Pin | MCU Pin | Description |  
|-----------------|-----------|-------------|  
| VCC (5V)        | 5V        | Power       |  
| GND             | GND       | Ground      |  
| TX              | GPIO 0   | Data to MCU |  
| RX              | GPIO 2   | Data from MCU |  

⚠️ R307 works on 3.3V logic. ESP32 GPIO pins are safe for direct connection.  

---

## 📚 Software Requirements  

- [Arduino IDE](https://www.arduino.cc/en/software) (or PlatformIO)  
- ESP32 board package installed in Arduino IDE  
- Libraries:  
  - [Adafruit Fingerprint Sensor Library](https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library)  
  - WiFi (built-in with NodeMCU)  
  - HTTPClient (built-in with NodeMCU)  

---




