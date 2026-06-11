/*
 * net_ota.h — Actualización de firmware por aire (OTA) desde S3.
 *
 * Flujo:
 *   1. GitHub Actions compila el firmware y lo sube a S3
 *      (https://firmware.lermita.freeddns.org/firmware_vX.Y.Z.bin).
 *   2. Publica por MQTT en .../sensor/in un JSON:
 *        {"cmd":"ota","version":"v1.2.0","url":"https://.../firmware_v1.2.0.bin"}
 *   3. El ESP32 recibe ese mensaje, descarga el .bin por HTTP(S) y se
 *      reprograma a sí mismo usando la librería Update.
 */
#ifndef NET_OTA_H
#define NET_OTA_H

#include <Arduino.h>

#define FIRMWARE_VERSION "v1.0.0"   ///< Versión actual de este firmware

bool otaLooksLikeUpdate(const String& payload); ///< ¿El mensaje trae una URL de firmware?
void otaHandleMessage(const String& payload);   ///< Procesa el comando OTA y lanza la descarga

#endif // NET_OTA_H
