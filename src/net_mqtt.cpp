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

// Traduce el código de estado de PubSubClient a algo legible (para diagnóstico).
static const char* mqttStateStr(int s) {
    switch (s) {
        case -4: return "TIMEOUT (el servidor no respondio a tiempo)";
        case -3: return "CONEXION PERDIDA";
        case -2: return "CONEXION FALLIDA (no se pudo abrir el socket/TLS)";
        case -1: return "DESCONECTADO";
        case  0: return "CONECTADO";
        case  1: return "PROTOCOLO INCORRECTO";
        case  2: return "CLIENT ID RECHAZADO";
        case  3: return "SERVIDOR NO DISPONIBLE";
        case  4: return "USUARIO/CONTRASENA MAL";
        case  5: return "NO AUTORIZADO (revisa ACL/permisos)";
        default: return "DESCONOCIDO";
    }
}

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

    Serial.printf("[MQTT] Conectando a %s:%d (usuario '%s', clientId '%s', TLS=%s)...\n",
                  MQTT_SERVER_STR, MQTT_PORT_INT, MQTT_USER_STR, clientId.c_str(),
                  MQTT_USE_TLS ? "si" : "no");

    unsigned long t0 = millis();
    bool ok = client.connect(clientId.c_str(), MQTT_USER_STR, MQTT_PASSWORD_STR);
    unsigned long dt = millis() - t0;

    if (ok) {
        Serial.printf("[MQTT] CONECTADO al broker en %lu ms.\n", dt);
        // Suscripción al canal de comandos (QoS 1).
        if (client.subscribe(TOPIC_SUB, 1)) {
            Serial.printf("[MQTT] Suscrito a %s (escuchando comandos/OTA)\n", TOPIC_SUB);
        } else {
            Serial.printf("[MQTT] Error al suscribirse a %s\n", TOPIC_SUB);
        }
    } else {
        int st = client.state();
        Serial.printf("[MQTT] FALLO tras %lu ms. state=%d -> %s\n", dt, st, mqttStateStr(st));
        Serial.println("[MQTT] Pista: verifica puerto/TLS, usuario/contrasena y que el broker este arriba.");
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
        Serial.println("[MQTT] Transporte: TLS (cifrado) con CA raiz cargada.");
    } else {
        client.setClient(plainClient);
        Serial.println("[MQTT] Transporte: TCP plano (sin cifrar).");
    }

    client.setServer(MQTT_SERVER_STR, MQTT_PORT_INT);
    client.setBufferSize(1024);   // payloads OTA pueden ser grandes
    client.setKeepAlive(30);
    client.setCallback(onMqttMessage);

    Serial.println("[MQTT] Configurado.");
    Serial.printf("[MQTT]  Broker -> %s:%d  (usuario '%s')\n",
                  MQTT_SERVER_STR, MQTT_PORT_INT, MQTT_USER_STR);
    Serial.printf("[MQTT]  PUB -> %s\n", TOPIC_PUB);
    Serial.printf("[MQTT]  SUB -> %s\n", TOPIC_SUB);
}

void mqttLoop() {
    static bool wasConnected = false;

    if (!wifiIsConnected()) {
        // Sin internet (modo solo-AP) no tiene sentido intentar MQTT.
        if (wasConnected) {
            Serial.println("[MQTT] Se perdio el WiFi: MQTT en pausa hasta recuperar internet.");
            wasConnected = false;
        }
        return;
    }

    if (!client.connected()) {
        if (wasConnected) {
            Serial.printf("[MQTT] Conexion caida (state=%d -> %s). Reintentando...\n",
                          client.state(), mqttStateStr(client.state()));
            wasConnected = false;
        }
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            if (mqttReconnect()) wasConnected = true;
        }
        return;
    }

    wasConnected = true;
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
