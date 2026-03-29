#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "UniversalMesh.h"

#ifndef NODE_NAME
#define NODE_NAME "C6-Secure-Sensor"
#endif

#ifndef MESH_SECURE_KEY
#define MESH_SECURE_KEY "My16CharKey1234!" // Exactly 16 characters!
#endif

#define APP_ID_TEMP_HUMID 0x03
#define SLEEP_SECONDS 30

UniversalMesh mesh;

// --- RTC MEMORY ---
RTC_DATA_ATTR uint8_t rtcCoordinatorMac[6] = {0};
RTC_DATA_ATTR uint8_t rtcChannel = 0; 
RTC_DATA_ATTR bool rtcIsConfigured = false;

void setup() {
    Serial.begin(115200);
    delay(3000); // For Native USB debugging
    
    Serial.println("\n=== Secure Sensor Node Waking Up ===");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // ---> ENABLE AES ENCRYPTION <---
    mesh.setNetworkKey(MESH_SECURE_KEY);

    bool justDiscovered = false;

    // 1. DISCOVERY (Only runs on first boot or dropped connection)
    if (!rtcIsConfigured || rtcChannel == 0) {
        Serial.println("[BOOT] Network unknown. Sweeping channels...");
        mesh.begin(1); 
        rtcChannel = mesh.findCoordinatorChannel(NODE_NAME);
        
        if (rtcChannel > 0) {
            mesh.getCoordinatorMac(rtcCoordinatorMac);
            rtcIsConfigured = true;
            Serial.printf("[BOOT] Locked on Channel %d. Saved to RTC.\n", rtcChannel);
            justDiscovered = true;
        } else {
            Serial.println("[BOOT] Coordinator unreachable.");
            esp_sleep_enable_timer_wakeup(SLEEP_SECONDS * 1000000ULL);
            esp_deep_sleep_start();
        }
    }

    // 2. TRANSMIT
    if (rtcIsConfigured) {
        mesh.begin(rtcChannel);
        mesh.setCoordinatorMac(rtcCoordinatorMac);

        // If we just discovered the network, send a formal PING with our name
        // to let the coordinator know we've joined.
        if(justDiscovered) {
            Serial.println("[BOOT] Announcing presence to coordinator...");
            mesh.send(rtcCoordinatorMac, MESH_TYPE_PING, 0x00, (const uint8_t*)NODE_NAME, strlen(NODE_NAME), 4);
            delay(100); // Brief pause to allow packet to send before sleeping
        }
        
        // --- Pack Payload ---
        float cpuTemp = 0.0;
        #if defined(ESP32)
            cpuTemp = temperatureRead(); // Native ESP32 internal CPU temp
        #endif
        
        JsonDocument doc;
        doc["cpu_temp"] = cpuTemp;
        doc["uptime_ms"] = millis(); // Helpful diagnostic to see wake time

        String jsonOutput;
        serializeJson(doc, jsonOutput);
        
        // --- Send via UniversalMesh Core SECURELY ---
        // Changing sendToCoordinator to sendSecureToCoordinator uses AES
        if (mesh.sendSecureToCoordinator(APP_ID_TEMP_HUMID, jsonOutput)) {
            Serial.println("[TX] Encrypted telemetry sent successfully!");
        } else {
            Serial.println("[TX] Delivery failed. Wiping RTC state.");
            rtcIsConfigured = false;
        }
        
        // 3. SLEEP
        Serial.printf("[SLEEP] Shutting down for %d seconds.\n", SLEEP_SECONDS);
        delay(20); 
        esp_sleep_enable_timer_wakeup(SLEEP_SECONDS * 1000000ULL);
        esp_deep_sleep_start();
    }
}

void loop() {}