/*
 * secrets.h — Credenciales y configuración de red/MQTT.
 *
 * Los valores reales se inyectan al compilar desde el archivo .env
 * (ver platformio.ini + scripts/build_with_env.py). Aquí solo se declaran
 * las variables globales que el resto del firmware consume.
 */
#ifndef SECRETS_H
#define SECRETS_H

#include <Arduino.h>

// ── WiFi ─────────────────────────────────────────────────────────────────
extern const char* WIFI_SSID_STR;       ///< Nombre de la red WiFi
extern const char* WIFI_PASSWORD_STR;    ///< Contraseña de la red WiFi

// ── MQTT ─────────────────────────────────────────────────────────────────
extern const char* MQTT_SERVER_STR;      ///< Host del broker EMQX
extern const int   MQTT_PORT_INT;        ///< Puerto MQTT (1883 plano / 8883 TLS)
extern const char* MQTT_USER_STR;        ///< Usuario MQTT (según ACL: "sensor")
extern const char* MQTT_PASSWORD_STR;    ///< Contraseña MQTT del usuario
extern const bool  MQTT_USE_TLS;         ///< true = TLS (8883), false = plano (1883)

// ── Tópicos (armados a partir de la jerarquía COUNTRY/STATE/CITY/...) ──────
extern const char* TOPIC_PUB;            ///< Donde el sensor PUBLICA  (.../sensor/out)
extern const char* TOPIC_SUB;            ///< Donde el sensor ESCUCHA  (.../sensor/in)

// ── Certificado raíz para TLS ──────────────────────────────────────────────
extern const char* ROOT_CA_CERT;         ///< CA raíz (ISRG Root X1 / Let's Encrypt)

// ── Identidad del dispositivo ──────────────────────────────────────────────
String getDeviceId();                    ///< ID único basado en la MAC del ESP32

#endif // SECRETS_H
