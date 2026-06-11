#include "net_wifi.h"
#include "secrets.h"
#include <WiFi.h>

static unsigned long lastReconnectAttempt = 0;
static const unsigned long RECONNECT_INTERVAL = 5000;  // 5 s entre reintentos

void wifiConnect() {
    Serial.printf("[WIFI] Conectando a la red '%s'...\n", WIFI_SSID_STR);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // mejor latencia para MQTT
    WiFi.begin(WIFI_SSID_STR, WIFI_PASSWORD_STR);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.printf("[WIFI] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println();
        Serial.println("[WIFI] No se pudo conectar (se reintentará en segundo plano).");
    }
}

void wifiEnsureConnected() {
    if (WiFi.status() == WL_CONNECTED) return;

    unsigned long now = millis();
    if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
    lastReconnectAttempt = now;

    Serial.println("[WIFI] Enlace perdido. Reintentando...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID_STR, WIFI_PASSWORD_STR);
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String wifiGetIp() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return String("0.0.0.0");
}
