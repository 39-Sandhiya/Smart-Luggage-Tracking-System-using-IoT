// Firebase Configuration
const firebaseConfig = {
    apiKey: "AIzaSyDEa0Yi8Bm5D1rJqqcAn5GhsjqOmDyQzOo",
    authDomain: "smartluggage-6f902.firebaseapp.com",
    databaseURL: "https://smartluggage-6f902-default-rtdb.asia-southeast1.firebasedatabase.app",
    projectId: "smartluggage-6f902",
    storageBucket: "smartluggage-6f902.firebasestorage.app",
    messagingSenderId: "247186740817",
    appId: "1:247186740817:web:8b33628d8ef5fbb8f54a70"
};

// Initialize Firebase
firebase.initializeApp(firebaseConfig);
const database = firebase.database();

// Global variables
let map, marker, polyline, route = [];
let lastAlertId = 0;

// Initialize Map
window.addEventListener('load', function() {
    map = L.map('map').setView([11.6643, 78.1460], 14);
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '© OpenStreetMap contributors'
    }).addTo(map);
    marker = L.marker([11.6643, 78.1460]).addTo(map);
    polyline = L.polyline([], { color: '#3498db', weight: 3 }).addTo(map);
    
    // Initialize Firebase data if empty
    initializeFirebaseData();
});

// Initialize default data structure
function initializeFirebaseData() {
    const luggageRef = database.ref('luggage_data');
    luggageRef.once('value', function(snapshot) {
        if (!snapshot.exists()) {
            luggageRef.set({
                lat: 11.6643,
                lng: 78.1460,
                isAlert: false,
                alertReason: "",
                power: true,
                mode: "Normal",
                lastUpdated: Date.now()
            });
        }
    });
}

// Listen for real-time changes
const luggageRef = database.ref('luggage_data');

luggageRef.on('value', function(snapshot) {
    const data = snapshot.val();
    if (!data) return;
    
    console.log("Data received:", data); // Debug log
    
    // 1. Update Map (always, regardless of power)
    if (map && marker && data.lat && data.lng) {
        const pos = [data.lat, data.lng];
        route.push(pos);
        if (route.length > 100) route.shift();
        marker.setLatLng(pos);
        polyline.setLatLngs(route);
        map.setView(pos, 14);
    }
    
    // 2. Check if system is ON or OFF
    const isPowerOn = data.power === true;
    const powerBtn = document.getElementById('power-toggle');
    const statusBar = document.getElementById('status-bar');
    const statusText = document.getElementById('status-text');
    const modeBtn = document.getElementById('mode-toggle');
    
    // Update Power Button
    if (isPowerOn) {
        powerBtn.innerText = '🟢 SYSTEM ON';
        powerBtn.className = 'power-on';
    } else {
        powerBtn.innerText = '⚫ SYSTEM OFF';
        powerBtn.className = 'power-off';
    }
    
    // 3. Update Mode Display
    const mode = data.mode || "Normal";
    if (mode === "Normal") {
        modeBtn.innerText = '👤 Wearable Mode';
        modeBtn.style.background = '#1e3a5f';
    } else {
        modeBtn.innerText = '📦 Rack Mode';
        modeBtn.style.background = '#5a3a1e';
    }
    
    // 4. Handle Alerts - ONLY if system is ON
    const isAlert = data.isAlert === true;
    const alertReason = data.alertReason || "";
    
    if (!isPowerOn) {
        // System is OFF - no alerts, show offline status
        statusBar.className = 'status-safe';
        statusText.innerText = '⚫ SYSTEM OFFLINE - NOT MONITORING';
        
        // Clear any stuck alert display
        if (document.getElementById('status-bar').classList.contains('status-alert')) {
            // Force clear alert visual
        }
    } 
    else if (isPowerOn && isAlert && alertReason) {
        // System is ON and there's an active alert
        statusBar.className = 'status-alert';
        statusText.innerText = `⚠️ ALERT: ${alertReason}`;
        
        // Add to alert history (avoid duplicates)
        addToAlertHistory(alertReason);
    } 
    else if (isPowerOn && !isAlert) {
        // System is ON and no alerts - all good
        statusBar.className = 'status-safe';
        statusText.innerText = '🟢 SYSTEM ACTIVE - ALL SECURE';
    }
    
    // 5. Update mode hint based on current mode
    const modeHint = document.getElementById('mode-hint');
    if (mode === "Normal") {
        modeHint.innerHTML = '👤 Wearable: Reed + LDR sensors only | Motion sensor OFF';
    } else {
        modeHint.innerHTML = '📦 Rack: All sensors active (Reed + LDR + Motion)';
    }
});

// Add alert to history with timestamp
function addToAlertHistory(message) {
    const alertList = document.getElementById('alert-list');
    const timestamp = new Date().toLocaleTimeString();
    const alertId = Date.now();
    
    // Remove "No alerts" placeholder if exists
    if (alertList.children.length === 1 && alertList.children[0].innerText === "No alerts yet") {
        alertList.innerHTML = '';
    }
    
    const li = document.createElement('li');
    li.id = `alert-${alertId}`;
    li.innerHTML = `🔔 <strong>${timestamp}</strong><br>${message}`;
    alertList.insertBefore(li, alertList.firstChild);
    
    // Keep only last 15 alerts
    while (alertList.children.length > 15) {
        alertList.removeChild(alertList.lastChild);
    }
    
    // Auto-clear old alerts after 30 seconds (optional)
    setTimeout(() => {
        const element = document.getElementById(`alert-${alertId}`);
        if (element) element.style.opacity = '0.5';
    }, 30000);
}

// Power Button Logic
document.getElementById('power-toggle').addEventListener('click', function() {
    luggageRef.once('value', function(snapshot) {
        const data = snapshot.val();
        if (data) {
            const newPowerState = !data.power;
            
            // Update Firebase
            luggageRef.update({ 
                power: newPowerState,
                // IMPORTANT: Clear alerts when turning OFF
                isAlert: newPowerState ? (data.isAlert || false) : false,
                alertReason: newPowerState ? (data.alertReason || "") : "",
                lastUpdated: Date.now()
            }).then(() => {
                console.log(`Power toggled to: ${newPowerState ? 'ON' : 'OFF'}`);
                
                // Add to history when power changes
                if (!newPowerState) {
                    addToAlertHistory("🔌 System turned OFF manually");
                } else {
                    addToAlertHistory("🔋 System turned ON manually");
                }
            }).catch(error => {
                console.error("Error toggling power:", error);
            });
        }
    });
});

// Mode Button Logic
document.getElementById('mode-toggle').addEventListener('click', function() {
    luggageRef.once('value', function(snapshot) {
        const data = snapshot.val();
        if (data) {
            // Cycle: Normal (Wearable) <-> Tracking (Rack)
            const newMode = (data.mode === "Normal") ? "Tracking" : "Normal";
            const modeName = (newMode === "Normal") ? "Wearable" : "Rack";
            
            luggageRef.update({ 
                mode: newMode,
                lastUpdated: Date.now()
            }).then(() => {
                console.log(`Mode changed to: ${modeName}`);
                addToAlertHistory(`🔄 Mode changed to ${modeName} mode`);
            }).catch(error => {
                console.error("Error changing mode:", error);
            });
        }
    });
});

// Manual Alert Clear Button (for testing - add to HTML if needed)
// This helps when alerts get stuck during manual testing
window.clearManualAlert = function() {
    luggageRef.update({
        isAlert: false,
        alertReason: ""
    }).then(() => {
        console.log("Alert manually cleared");
        addToAlertHistory("✓ Alert manually cleared");
    });
};

console.log("✅ Web App Ready - Waiting for Firebase data...");
