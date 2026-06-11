/*
 * net_wifi.h — Conexión WiFi en modo estación (STA).
 *
 * A diferencia del VitalGuard original (que creaba su propio Access Point),
 * aquí el ESP32 se conecta a una red WiFi existente con salida a internet
 * para poder alcanzar el broker MQTT en la nube.
 */
#ifndef NET_WIFI_H
#define NET_WIFI_H

#include <Arduino.h>

void wifiConnect();          ///< Conecta usando las credenciales de secrets/.env
void wifiEnsureConnected();  ///< Reconecta si se cayó la conexión (llamar en loop)
bool wifiIsConnected();      ///< true si hay enlace WiFi activo
String wifiGetIp();          ///< IP local asignada (para mostrar en OLED/serial)

#endif // NET_WIFI_H
