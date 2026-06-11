/*
 * net_mqtt.h — Cliente MQTT hacia el broker EMQX.
 *
 * - Publica los datos biométricos (BPM, SpO2, etc.) en TOPIC_PUB (.../sensor/out)
 * - Se suscribe a TOPIC_SUB (.../sensor/in) para recibir comandos del backend
 *   (incluida la orden de actualización OTA).
 * - La conexión puede ser cifrada (TLS, 8883) o plana (1883) según MQTT_TLS.
 */
#ifndef NET_MQTT_H
#define NET_MQTT_H

#include <Arduino.h>

void   mqttSetup();        ///< Configura cliente, TLS y callback
void   mqttLoop();         ///< Mantiene la conexión y procesa mensajes (llamar en loop)
bool   mqttIsConnected();  ///< true si está conectado al broker

/// Publica una lectura biométrica como JSON en TOPIC_PUB.
void   mqttPublishBiometrics(float bpm, float spo2, bool finger, bool beat, long rawIR);

/// Devuelve el último comando/alerta recibido por TOPIC_SUB (para mostrar en pantalla).
String mqttGetLastCommand();

#endif // NET_MQTT_H
