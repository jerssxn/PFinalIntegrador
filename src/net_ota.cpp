#include "net_ota.h"
#include "secrets.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>

#define OTA_BUFFER_SIZE 4096

// Estructura para pasarle datos a la tarea de descarga.
struct OtaJob {
    char url[256];
    char version[32];
};

bool otaLooksLikeUpdate(const String& payload) {
    // Heurística simple: contiene "url" y apunta a un .bin
    return payload.indexOf("url") >= 0 && payload.indexOf(".bin") >= 0;
}

// ── Tarea que descarga e instala el nuevo firmware ──────────────────────────
static void otaTask(void* param) {
    OtaJob* job = (OtaJob*)param;
    String url = String(job->url);
    Serial.printf("[OTA] Iniciando actualización a %s desde:\n      %s\n", job->version, job->url);

    WiFiClient* netClient;
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    bool https = url.startsWith("https");
    if (https) {
        // El binario puede venir de S3 (URL prefirmada) cuyo CA NO es el del
        // broker. Por eso, para la descarga OTA no validamos el certificado:
        // la URL prefirmada ya está autenticada y es de un solo uso/temporal.
        secureClient.setInsecure();
        netClient = &secureClient;
    } else {
        netClient = &plainClient;
    }

    // Seguir redirecciones (los endpoints de S3 suelen redirigir).
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(true);

    // Log de progreso de la descarga (cada ~10%).
    httpUpdate.onStart([]() { Serial.println("[OTA] Descarga iniciada."); });
    httpUpdate.onProgress([](int cur, int total) {
        static int lastPct = -1;
        int pct = total > 0 ? (cur * 100 / total) : 0;
        if (pct != lastPct && (pct % 10 == 0)) {
            lastPct = pct;
            Serial.printf("[OTA] Descargando... %d%%  (%d/%d bytes)\n", pct, cur, total);
        }
    });
    httpUpdate.onEnd([]() { Serial.println("[OTA] Descarga completa, aplicando firmware..."); });

    Serial.println("[OTA] Contactando al servidor de firmware...");
    t_httpUpdate_return ret = httpUpdate.update(*netClient, url);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] FALLÓ (%d): %s\n",
                          httpUpdate.getLastError(),
                          httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] Sin actualizaciones.");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("[OTA] OK. Reiniciando...");
            break;
    }

    free(job);
    vTaskDelete(NULL);
}

void otaHandleMessage(const String& payload) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[OTA] JSON inválido: %s\n", err.c_str());
        return;
    }

    const char* url = doc["url"] | "";
    const char* version = doc["version"] | "desconocida";
    if (strlen(url) == 0) {
        Serial.println("[OTA] El mensaje no contiene 'url'. Ignorado.");
        return;
    }

    Serial.printf("[OTA] Versión actual: %s -> nueva: %s\n", FIRMWARE_VERSION, version);

    OtaJob* job = (OtaJob*)malloc(sizeof(OtaJob));
    if (!job) {
        Serial.println("[OTA] Sin memoria para la tarea OTA.");
        return;
    }
    strncpy(job->url, url, sizeof(job->url) - 1);
    job->url[sizeof(job->url) - 1] = '\0';
    strncpy(job->version, version, sizeof(job->version) - 1);
    job->version[sizeof(job->version) - 1] = '\0';

    // La descarga corre en el Core 1 (la red) para no bloquear el sensor (Core 0).
    xTaskCreatePinnedToCore(otaTask, "OTA_Task", 8192, job, 1, NULL, 1);
}
