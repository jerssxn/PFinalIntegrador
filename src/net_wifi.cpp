#include "net_wifi.h"
#include "secrets.h"
#include <WiFi.h>

// Access Point local SIEMPRE activo — réplica del comportamiento del VitalGuard
// original. Garantiza que el dashboard web y el sensor funcionen aunque no haya
// una red WiFi con internet configurada. Conéctate a esta red y abre
// http://192.168.4.1
static const char* AP_SSID = "VitalGuard-S3";
static const char* AP_PASS = "vitalguard123";

static unsigned long lastReconnectAttempt = 0;
static const unsigned long RECONNECT_INTERVAL = 15000;  // 15 s entre reintentos

// ¿Hay credenciales WiFi REALES configuradas en .env? Si quedaron en placeholder
// ("PON_AQUI_...") o "changeme", NO intentamos conectar como estación: los
// reintentos constantes corren en el Core 0 y "ahogan" la tarea del sensor
// (que también vive en el Core 0), haciendo que no se midan BPM/SpO2.
static bool wifiConfigured() {
    String ssid = String(WIFI_SSID_STR);
    if (ssid.length() == 0)        return false;
    if (ssid == "changeme")        return false;
    if (ssid.startsWith("PON_AQUI")) return false;
    return true;
}

void wifiConnect() {
    // AP + STA en simultáneo: el AP local nunca se cae; STA es solo para la nube.
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);  // mejor latencia para MQTT
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("[WIFI] AP local '%s' activo. IP: %s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());

    if (!wifiConfigured()) {
        Serial.println("[WIFI] Sin credenciales validas en .env -> SOLO AP local.");
        Serial.println("[WIFI] Dashboard y sensor OK; no habra MQTT/OTA hasta poner WiFi real.");
        return;
    }

    Serial.printf("[WIFI] Conectando a la red '%s'...\n", WIFI_SSID_STR);
    WiFi.begin(WIFI_SSID_STR, WIFI_PASSWORD_STR);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // ~10 s máximo
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Conectado a internet. IP STA: %s\n",
                      WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WIFI] No conecto (se reintentara en segundo plano).");
    }
}

void wifiEnsureConnected() {
    if (!wifiConfigured()) return;             // sin creds: no reintentar (no satura el Core 0)
    if (WiFi.status() == WL_CONNECTED) return;

    unsigned long now = millis();
    if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
    lastReconnectAttempt = now;

    Serial.println("[WIFI] Reintentando enlace STA...");
    WiFi.begin(WIFI_SSID_STR, WIFI_PASSWORD_STR);  // no tocamos el AP local
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String wifiGetIp() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return WiFi.softAPIP().toString();  // sin STA, el dashboard vive en el AP
}

String wifiGetApIp() {
    return WiFi.softAPIP().toString();
}
