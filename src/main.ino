#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <MPU6050.h>
#include <FirebaseESP32.h>

// ---------------- FIREBASE CONFIGURATION ----------------
#define FIREBASE_HOST "https://smartluggage-6f902-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "AIzaSyDEa0Yi8Bm5D1rJqqcAn5GhsjqOmDyQzOo"

// ---------------- CONFIGURATION ----------------
const char* ssid = "POCO C61";
const char* password = "28111999";

#define BOTtoken "8797483316:AAFftgnbOERIBy_tO3qeKPAjPNt2uK-gOmM"
#define CHAT_ID "6242968688"

// ---------------- PIN SETUP ----------------
#define REED_PIN 18
#define LDR_PIN 34
#define SWITCH_PIN 23
#define BUZZER 5
#define GPS_RX 16
#define GPS_TX 17

// ---------------- OBJECTS ----------------
MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// ---------------- THRESHOLDS ----------------
#define LDR_THRESHOLD 800
#define MOTION_THRESHOLD 500
#define MOTION_TIME 3000
#define TOLERANCE_TIME 300
#define TELEGRAM_COOLDOWN 5000
#define FIREBASE_UPDATE_INTERVAL 2000

// ---------------- VARIABLES ----------------
int16_t ax, ay, az, prev_ax = 0, prev_ay = 0, prev_az = 0;
unsigned long motionStartTime = 0;
unsigned long lastMotionTime = 0;
unsigned long lastTelegramTime = 0;
unsigned long nextTelegramCheck = 0;
unsigned long lastFirebaseUpdate = 0;
unsigned long lastGPSUpdate = 0;
unsigned long lastFirebaseRead = 0;
bool lastPhysicalSwitchState = HIGH;
bool motionActive = false;
bool alertSent = false;
bool shouldSendTelegram = false;
String telegramReason = "";


// FLAGS
bool ldrTriggered = false;
bool reedTriggered = false; // <--- Add this here
bool currentAlertState = false;
String currentAlertReason = "";

// Mode control from Firebase
String currentMode = "Wearable";
bool appPowerControl = true;
bool firstRun = true;

// ---------------- FUNCTION DECLARATIONS ----------------
void setupFirebase();
void readFirebaseControls();
void updateFirebase();
void sendTelegramAlert(String reason);

// ---------------- SETUP ----------------
void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    
    pinMode(REED_PIN, INPUT_PULLUP);
    pinMode(SWITCH_PIN, INPUT_PULLUP);
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
    
    Wire.begin(21, 22);
    mpu.initialize();
    
    // Connect WiFi
    WiFi.begin( ssid, password);
    WiFi.setSleep(false);
    client.setInsecure();
    
    Serial.print("Connecting WiFi");
    unsigned long startAttempt = millis();
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
        if (millis() - startAttempt > 15000) {
            Serial.println("\nWiFi Failed! Restarting...");
            ESP.restart();
        }
    }
    
    Serial.println("\n✅ WiFi Connected!");
    
    // Setup Firebase
    setupFirebase();
    
    
    // Initialize Firebase settings
delay(2000); // Wait for Firebase connection

Firebase.setBool(firebaseData, "/luggage_data/power", true);

// Restore saved mode from Firebase instead of overwriting
if (Firebase.getString(firebaseData, "/luggage_data/mode")) {
    
    String savedMode = firebaseData.stringData();

    if (savedMode == "Rack" || savedMode == "Wearable") {
        currentMode = savedMode;

        Serial.print("✅ Restored Mode: ");
        Serial.println(currentMode);
    }
    else {
        // If invalid value exists
        currentMode = "Wearable";
        Firebase.setString(firebaseData, "/luggage_data/mode", "Wearable");

        Serial.println("⚠️ Invalid mode found. Reset to Wearable");
    }
}
else {
    // If mode not found in Firebase
    currentMode = "Wearable";
    Firebase.setString(firebaseData, "/luggage_data/mode", "Wearable");

    Serial.println("⚠️ Mode not found. Defaulting to Wearable");
}
    
    // Send startup message
    bot.sendMessage(CHAT_ID, "🛡️ System ONLINE\n📱 Firebase Connected", "");
    Serial.println("✅ System Ready!");
}

// ---------------- FIREBASE SETUP ----------------
void setupFirebase() {
    firebaseConfig.database_url = FIREBASE_HOST;
    firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
    
    Firebase.begin(&firebaseConfig, &firebaseAuth);
    Firebase.reconnectWiFi(true);
    
    Serial.println("✅ Firebase initialized");
}

// ---------------- READ FROM FIREBASE ----------------
void readFirebaseControls() {
    // Only read every 1 second to avoid flooding
    if (millis() - lastFirebaseRead < 1000) return;
    lastFirebaseRead = millis();
    
    // Read Power State from App - with error handling
    if (Firebase.getBool(firebaseData, "/luggage_data/power")) {
        if (firebaseData.dataType() == "boolean") {
            bool newPowerState = firebaseData.boolData();
            if (newPowerState != appPowerControl) {
                Serial.print("App changed power to: ");
                Serial.println(newPowerState ? "ON" : "OFF");
                appPowerControl = newPowerState;
            }
        }
    } else {
        // If read fails, don't change the value
        Serial.print("Firebase read error (power): ");
        Serial.println(firebaseData.errorReason());
    }
    
    // Read Mode from App
    if (Firebase.getString(firebaseData, "/luggage_data/mode")) {
        String newMode = firebaseData.stringData();
        if (newMode == "Rack" || newMode == "Wearable") {
            if (newMode != currentMode) {
                Serial.print("App changed mode to: ");
                Serial.println(newMode);
                currentMode = newMode;
            }
        }
    }
}

// ---------------- WRITE TO FIREBASE ----------------
void updateFirebase() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    // Update Location (only if GPS is valid)
    if (gps.location.isValid() && (millis() - lastGPSUpdate > 2000)) {
        Firebase.setFloat(firebaseData, "/luggage_data/lat", gps.location.lat());
        Firebase.setFloat(firebaseData, "/luggage_data/lng", gps.location.lng());
        lastGPSUpdate = millis();
    }
    
    // Update Sensor Readings
    int ldrValue = analogRead(LDR_PIN);
    Firebase.setInt(firebaseData, "/luggage_data/ldrValue", ldrValue);
    Firebase.setBool(firebaseData, "/luggage_data/reedTriggered", (digitalRead(REED_PIN) == HIGH));
    Firebase.setBool(firebaseData, "/luggage_data/ldrTriggered", ldrTriggered);
    
    // Motion state based on mode
    bool motionState = false;
    if (currentMode == "Rack") {
        motionState = motionActive;
    }
    Firebase.setBool(firebaseData, "/luggage_data/motionTriggered", motionState);
    
    // Update Alert Status
    Firebase.setBool(firebaseData, "/luggage_data/isAlert", currentAlertState);
    if (currentAlertState) {
        Firebase.setString(firebaseData, "/luggage_data/alertReason", currentAlertReason);
    } else {
        Firebase.setString(firebaseData, "/luggage_data/alertReason", "");
    }
    
    // Update System Status - DON'T overwrite appPowerControl here
    // We only write the COMBINED state for display
    bool systemPower = (digitalRead(SWITCH_PIN) == LOW) && appPowerControl;
    Firebase.setBool(firebaseData, "/luggage_data/systemState", systemPower);
  
    
    // Update Timestamp
    Firebase.setTimestamp(firebaseData, "/luggage_data/lastUpdated");
}

// ---------------- TELEGRAM ALERT ----------------
void sendTelegramAlert(String reason) {
    if (millis() - lastTelegramTime < TELEGRAM_COOLDOWN) return;
    
    String message = "⚠️ ALERT: " + reason + "\n";
    message += "📱 Mode: " + currentMode + "\n";
    
    if (gps.location.isValid()) {
        message += "📍 https://maps.google.com/?q=";
        message += String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
    } else {
        message += "📍 GPS searching...";
    }
    
    if (bot.sendMessage(CHAT_ID, message, "")) {
        lastTelegramTime = millis();
        Serial.println("📨 Telegram sent: " + reason);
    } else {
        Serial.println("❌ Telegram send failed!");
    }
}

void loop() {
    // 1. INPUT DATA (GPS & Firebase)
    while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
    }
    readFirebaseControls();

    // 2. POWER LOGIC (Smart Independent Control)
bool physicalSwitchState = (digitalRead(SWITCH_PIN) == LOW);

// Detect if the physical switch was just flipped
if (physicalSwitchState != lastPhysicalSwitchState) {
    appPowerControl = !appPowerControl; // Toggle the state
    Firebase.setBool(firebaseData, "/luggage_data/power", appPowerControl); // Sync to App
    lastPhysicalSwitchState = physicalSwitchState; // Update for next loop
    
    Serial.print("Switch flipped! System is now: ");
    Serial.println(appPowerControl ? "ARMED" : "DISARMED");
}

// Now the system only cares about the final 'appPowerControl' state
bool systemON = appPowerControl;

if (!systemON) {
    digitalWrite(BUZZER, LOW);
    motionActive = false;
    alertSent = false;
    ldrTriggered = false;
    reedTriggered = false; // Added reset per previous fix
    currentAlertState = false;
    currentAlertReason = "";
    
    if (millis() - lastFirebaseUpdate > FIREBASE_UPDATE_INTERVAL) {
        updateFirebase();
        lastFirebaseUpdate = millis();
    }
    return; 
}

    // --- TRACKING FOR CONTINUOUS ALARM ---
    bool alarmConditionPresent = false;
    String activeReason = "";

    // 3. LDR CHECK (Light)
    int ldrValue = analogRead(LDR_PIN);
    if (ldrValue < LDR_THRESHOLD) {
        alarmConditionPresent = true;
        activeReason = "LIGHT DETECTED";
        if (!ldrTriggered) { 
            sendTelegramAlert(activeReason);
            ldrTriggered = true;
        }
    } else {
        ldrTriggered = false;
    }

    // 4. REED CHECK (Zip)
    int reedState = digitalRead(REED_PIN);
    if (reedState == HIGH) { 
        alarmConditionPresent = true;
        activeReason = "ZIP TAMPERED";
        
        if (!reedTriggered) { 
            sendTelegramAlert(activeReason);
            reedTriggered = true; // Mark as sent
        }
    } else {
        // This now correctly resets the global flag when the zip is closed
        reedTriggered = false; 
    }

// 5. MPU CHECK (Motion - Rack Mode Only)
    if (currentMode == "Rack") {
        mpu.getAcceleration(&ax, &ay, &az);
        int totalChange = abs(ax - prev_ax) + abs(ay - prev_ay) + abs(az - prev_az);

        if (totalChange > MOTION_THRESHOLD) {
            lastMotionTime = millis();
            if (!motionActive) {
                motionStartTime = millis();
                motionActive = true;
            }
        }

        if (motionActive && (millis() - motionStartTime >= MOTION_TIME)) {
            alarmConditionPresent = true;
            activeReason = "CONTINUOUS MOTION";
            if (!alertSent) {
                sendTelegramAlert(activeReason);
                alertSent = true;
            }
        }

        if (millis() - lastMotionTime > TOLERANCE_TIME) {
            motionActive = false;
            alertSent = false;
        }
        prev_ax = ax; prev_ay = ay; prev_az = az;
    } 
    else {
        // RESET flags when not in Rack mode to prevent "ghost" triggers later
        motionActive = false;
        alertSent = false;
    }

    // 6. FINAL BUZZER CONTROL
    // This part keeps the buzzer ringing as long as any condition is true
    if (alarmConditionPresent) {
        digitalWrite(BUZZER, HIGH); 
        currentAlertState = true;
        currentAlertReason = activeReason;
    } else {
        digitalWrite(BUZZER, LOW);  // Only stops when ALL sensors are clear
        currentAlertState = false;
        currentAlertReason = "";
    }

    // 7. FIREBASE UPDATE
    if (millis() - lastFirebaseUpdate > FIREBASE_UPDATE_INTERVAL) {
        updateFirebase();
        lastFirebaseUpdate = millis();
    }

    delay(30);
}
