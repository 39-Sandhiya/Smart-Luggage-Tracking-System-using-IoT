# 🧳 Smart Luggage Tracking System using IoT

An IoT-based smart luggage tracking system that enhances travel security and enables real-time location monitoring. The system uses GPS and ESP32 microcontroller to track luggage location and send live updates to the user. It includes sensor-based safety features, Telegram alerts, and Firebase cloud storage for data logging.

---

## 🎯 Features

- 📍 **Real-time GPS Tracking** — Live location updates via GPS module
- 🔔 **Telegram Alerts** — Instant notifications for unauthorized movement or tampering
- 🌡️ **Temperature Monitoring** — Safety alerts for abnormal heat conditions
- ☁️ **Firebase Database** — Cloud storage for location history and sensor data
- 📊 **Web Dashboard** — Visual display of luggage status and location
- 🔒 **Security Alerts** — Motion detection and instant user notifications

---

## 🛠️ Tech Stack

| Component | Technology |
|-----------|------------|
| **Microcontroller** | ESP32 |
| **GPS Module** | NEO-6M / TinyGPS++ |
| **Communication** | WiFi, Telegram Bot API |
| **Database** | Firebase Realtime Database |
| **Dashboard** | HTML/CSS/JS (VS Code) |
| **Sensors** | Motion Sensor, Temperature Sensor |
| **IDE** | Arduino IDE |

---

## 📊 Dashboard Output

![Dashboard](images/dashboard_output.png)

---

## 💻 How It Works

1. **GPS Module** continuously tracks luggage location
2. **ESP32** processes data and sends to **Firebase** cloud database
3. **Telegram Bot** sends instant alerts for:
   - Unauthorized movement detected
   - Temperature exceeds safe limit
   - Location changes unexpectedly
4. **Web Dashboard** displays real-time status and location history
5. User can monitor luggage anytime via phone or computer

---

## 🚀 Getting Started

### Hardware Requirements
- ESP32 Development Board
- NEO-6M GPS Module
- Temperature Sensor (DHT11/DHT22)
- PIR Motion Sensor
- Jumper Wires & Breadboard

### Software Setup
1. Install **Arduino IDE** and ESP32 board support
2. Install required libraries: `WiFi`, `UniversalTelegramBot`, `TinyGPS++`, `ArduinoJson`
3. Update WiFi credentials and Telegram Bot Token in `main.ino`
4. Configure Firebase credentials in `firebase_config.js`
5. Upload code to ESP32
6. Open dashboard in browser to monitor

---

## 📄 Project Report

[View Full Report (PDF)](docs/project_report.pdf)

---

## 📜 License

This project is for academic purposes.
