/*
 * secrets.cpp — Resuelve las credenciales inyectadas por .env y arma los
 * tópicos MQTT respetando el ACL del broker:
 *
 *   sensor PUBLICA  -> colombia/valle/tulua/univalle/sensor/out
 *   sensor ESCUCHA  -> colombia/valle/tulua/univalle/sensor/in
 *
 * Estructura del tópico: COUNTRY/STATE/CITY/INSTITUTION/MQTT_USER/{out,in}
 */
#include "secrets.h"
#include <WiFi.h>

// ── Valores por defecto si no se inyectó nada desde .env ───────────────────
#ifndef COUNTRY
#define COUNTRY "colombia"
#endif
#ifndef STATE
#define STATE "valle"
#endif
#ifndef CITY
#define CITY "tulua"
#endif
#ifndef INSTITUTION
#define INSTITUTION "univalle"
#endif
#ifndef MQTT_SERVER
#define MQTT_SERVER "lermita.freeddns.org"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
#define MQTT_USER "sensor"
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD "changeme"
#endif
#ifndef MQTT_TLS
#define MQTT_TLS 0
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "changeme"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "changeme"
#endif

// ── WiFi ───────────────────────────────────────────────────────────────────
const char* WIFI_SSID_STR     = WIFI_SSID;
const char* WIFI_PASSWORD_STR = WIFI_PASSWORD;

// ── MQTT ─────────────────────────────────────────────────────────────────
const char* MQTT_SERVER_STR   = MQTT_SERVER;
const int   MQTT_PORT_INT     = MQTT_PORT;
const char* MQTT_USER_STR     = MQTT_USER;
const char* MQTT_PASSWORD_STR = MQTT_PASSWORD;
const bool  MQTT_USE_TLS      = (MQTT_TLS != 0);

// ── Tópicos: se construyen una sola vez al arrancar ────────────────────────
static String topicBase  = String(COUNTRY) + "/" + STATE + "/" + CITY + "/" +
                           INSTITUTION + "/" + MQTT_USER;
static String topicPubStr = topicBase + "/out";
static String topicSubStr = topicBase + "/in";

const char* TOPIC_PUB = topicPubStr.c_str();
const char* TOPIC_SUB = topicSubStr.c_str();

// ── ID del dispositivo a partir de la MAC ──────────────────────────────────
String getDeviceId() {
    uint8_t mac[6];
    char buf[20];
    WiFi.macAddress(mac);
    snprintf(buf, sizeof(buf), "vitalguard-%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String(buf);
}

// ── Certificado raíz para TLS ──────────────────────────────────────────────
// ISRG Root X1 — CA raíz de Let's Encrypt. Sirve si el broker usa un
// certificado emitido por Let's Encrypt (lo habitual con dominios freeddns).
// Si tu broker usa otro CA, reemplaza este bloque por el certificado correcto.
const char* ROOT_CA_CERT =
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";
