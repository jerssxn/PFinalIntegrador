#include "net_mqtt.h"
#include "secrets.h"
#include "net_ota.h"
#include "net_wifi.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

// Dos transportes posibles; se elige uno según MQTT_USE_TLS.
static WiFiClientSecure tlsClient;   // para TLS (puerto 8883)
static WiFiClient       plainClient; // para texto plano (puerto 1883)
static PubSubClient     client;

static String deviceId;
static String lastCommand = "";

static unsigned long lastReconnectAttempt = 0;
static const unsigned long RECONNECT_INTERVAL = 5000;  // 5 s entre reintentos

// ── Callback: se dispara con cada mensaje recibido en TOPIC_SUB ─────────────
static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    String msg;
    msg.reserve(length + 1);
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    Serial.printf("[MQTT] Mensaje en '%s': %s\n", topic, msg.c_str());
    lastCommand = msg;

    // El backend envía comandos por .../sensor/in. Si el mensaje trae una URL
    // de firmware, lo tratamos como orden de actualización OTA.
    if (otaLooksLikeUpdate(msg)) {
        Serial.println("[MQTT] Comando OTA detectado.");
        otaHandleMessage(msg);
    }
}

// ── Conexión / reconexión al broker ─────────────────────────────────────────
static bool mqttReconnect() {
    String clientId = deviceId + "-" + String(random(0xffff), HEX);

    Serial.printf("[MQTT] Conectando a %s:%d (usuario '%s', TLS=%s)...\n",
                  MQTT_SERVER_STR, MQTT_PORT_INT, MQTT_USER_STR,
                  MQTT_USE_TLS ? "si" : "no");

    bool ok = client.connect(clientId.c_str(), MQTT_USER_STR, MQTT_PASSWORD_STR);
    if (ok) {
        Serial.println("[MQTT] Conectado.");
        // Suscripción al canal de comandos (QoS 1).
        if (client.subscribe(TOPIC_SUB, 1)) {
            Serial.printf("[MQTT] Suscrito a %s\n", TOPIC_SUB);
        } else {
            Serial.printf("[MQTT] Error al suscribirse a %s\n", TOPIC_SUB);
        }
    } else {
        Serial.printf("[MQTT] Falló (state=%d). Revisa usuario/contraseña/puerto/TLS.\n",
                      client.state());
    }
    return ok;
}

void mqttSetup() {
    deviceId = getDeviceId();

    if (MQTT_USE_TLS) {
        tlsClient.setCACert(ROOT_CA_CERT);
        // Si tu broker usa certificado autofirmado y la validación falla,
        // puedes (solo para pruebas) sustituir la línea anterior por:
        //   tlsClient.setInsecure();
        client.setClient(tlsClient);
    } else {
        client.setClient(plainClient);
    }

    client.setServer(MQTT_SERVER_STR, MQTT_PORT_INT);
    client.setBufferSize(1024);   // payloads OTA pueden ser grandes
    client.setKeepAlive(30);
    client.setCallback(onMqttMessage);

    Serial.println("[MQTT] Configurado.");
    Serial.printf("[MQTT]  PUB -> %s\n", TOPIC_PUB);
    Serial.printf("[MQTT]  SUB -> %s\n", TOPIC_SUB);
}

void mqttLoop() {
    if (!wifiIsConnected()) return;  // sin WiFi no tiene sentido intentar

    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            mqttReconnect();
        }
        return;
    }
    client.loop();
}

bool mqttIsConnected() {
    return client.connected();
}

void mqttPublishBiometrics(float bpm, float spo2, bool finger, bool beat, long rawIR) {
    if (!client.connected()) return;

    // JSON compacto con la lectura.
    String payload = "{";
    payload += "\"device\":\"" + deviceId + "\",";
    payload += "\"bpm\":"    + String(bpm, 1) + ",";
    payload += "\"spo2\":"   + String(spo2, 1) + ",";
    payload += "\"finger\":" + String(finger ? "true" : "false") + ",";
    payload += "\"beat\":"   + String(beat ? "true" : "false") + ",";
    payload += "\"ir\":"     + String(rawIR) + ",";
    payload += "\"ts\":"     + String((unsigned long)time(nullptr));
    payload += "}";

    bool ok = client.publish(TOPIC_PUB, payload.c_str());
    if (ok) {
        Serial.printf("[MQTT] -> %s  %s\n", TOPIC_PUB, payload.c_str());
    } else {
        Serial.println("[MQTT] Error al publicar (¿payload muy grande o sin conexión?).");
    }
}

String mqttGetLastCommand() {
    return lastCommand;
}
