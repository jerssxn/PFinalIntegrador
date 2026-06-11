/*
 * net_wifi.h — Red WiFi en modo AP + STA simultáneo.
 *
 * El ESP32 SIEMPRE levanta un Access Point local ("VitalGuard-S3") para que el
 * dashboard web y el sensor funcionen aunque no haya internet (como el
 * VitalGuard original). Además, si hay credenciales válidas en .env, se conecta
 * como estación (STA) a una red con salida a internet para alcanzar el broker
 * MQTT y recibir actualizaciones OTA.
 */
#ifndef NET_WIFI_H
#define NET_WIFI_H

#include <Arduino.h>

void wifiConnect();          ///< Levanta el AP local y conecta STA si hay credenciales
void wifiEnsureConnected();  ///< Reconecta STA si se cayó (llamar en loop)
bool wifiIsConnected();      ///< true si hay enlace STA activo (con internet)
String wifiGetIp();          ///< IP para el dashboard (STA si hay; si no, la del AP)
String wifiGetApIp();        ///< IP del Access Point local (normalmente 192.168.4.1)

#endif // NET_WIFI_H
