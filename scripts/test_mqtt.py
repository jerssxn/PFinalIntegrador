#!/usr/bin/env python3
"""
Prueba de extremo a extremo de la conexión MQTT contra el broker EMQX real,
usando exactamente el mismo flujo, tópicos y seguridad (TLS 8883) que usará
el firmware del ESP32.

Qué hace:
  1. Conecta un cliente como usuario "backend" y se suscribe a
     colombia/valle/tulua/univalle/sensor/out  (lo que el sensor publica).
  2. Conecta un cliente como usuario "sensor" y publica un JSON biométrico
     de ejemplo en ese tópico.
  3. Verifica que el mensaje llegó al backend  -> prueba el camino sensor->nube.
  4. (Opcional) El backend publica un comando en .../sensor/in y comprobamos
     que se acepta -> prueba el camino nube->sensor (lo que el ESP escucha).

Requisitos:
    pip install paho-mqtt

Uso típico (TLS, puerto 8883):
    python scripts/test_mqtt.py \
        --host lermita.freeddns.org --port 8883 --tls \
        --sensor-pass  "CLAVE_DEL_SENSOR" \
        --backend-pass "CLAVE_DEL_BACKEND"

Si solo tienes la clave del sensor, puedes hacer una prueba de solo-publicación
(no verifica recepción, pero confirma credenciales + TLS + permiso de publicar):
    python scripts/test_mqtt.py --host lermita.freeddns.org --port 8883 --tls \
        --sensor-pass "CLAVE_DEL_SENSOR" --only-publish
"""
import argparse
import json
import ssl
import sys
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    sys.exit("Falta paho-mqtt. Instálalo con:  pip install paho-mqtt")

DEFAULT_BASE = "colombia/valle/tulua/univalle/sensor"

received = []


def make_client(client_id, user, password, host, port, use_tls, insecure):
    client = mqtt.Client(client_id=client_id, clean_session=True)
    if user:
        client.username_pw_set(user, password)
    if use_tls:
        client.tls_set(cert_reqs=ssl.CERT_NONE if insecure else ssl.CERT_REQUIRED)
        if insecure:
            client.tls_insecure_set(True)
    client.connect(host, port, keepalive=30)
    return client


def main():
    ap = argparse.ArgumentParser(description="Prueba MQTT contra el broker EMQX real")
    ap.add_argument("--host", default="lermita.freeddns.org")
    ap.add_argument("--port", type=int, default=8883)
    ap.add_argument("--tls", action="store_true", help="usar TLS (puerto 8883)")
    ap.add_argument("--insecure", action="store_true",
                    help="no validar el certificado (solo pruebas)")
    ap.add_argument("--base", default=DEFAULT_BASE,
                    help="prefijo de tópico (por defecto el del ACL)")
    ap.add_argument("--sensor-user", default="sensor")
    ap.add_argument("--sensor-pass", default=None)
    ap.add_argument("--backend-user", default="backend")
    ap.add_argument("--backend-pass", default=None)
    ap.add_argument("--only-publish", action="store_true",
                    help="solo publica como sensor (no verifica recepción)")
    args = ap.parse_args()

    topic_out = args.base + "/out"
    topic_in = args.base + "/in"

    print(f"== Broker: {args.host}:{args.port}  TLS={args.tls} ==")
    print(f"== Tópico OUT (sensor publica): {topic_out}")
    print(f"== Tópico IN  (sensor escucha): {topic_in}\n")

    # ----- 1) Suscriptor backend (a menos que sea solo-publicación) -----
    backend = None
    if not args.only_publish:
        if not args.backend_pass:
            sys.exit("Falta --backend-pass (o usa --only-publish para probar solo el envío).")

        def on_message(c, u, msg):
            received.append(msg.payload.decode(errors="replace"))
            print(f"[BACKEND] <- recibido en {msg.topic}: {msg.payload.decode(errors='replace')}")

        try:
            backend = make_client("test-backend", args.backend_user, args.backend_pass,
                                  args.host, args.port, args.tls, args.insecure)
        except Exception as e:
            sys.exit(f"[BACKEND] No se pudo conectar: {e}")
        backend.on_message = on_message
        backend.subscribe(topic_out, qos=1)
        backend.loop_start()
        print(f"[BACKEND] conectado y suscrito a {topic_out}")
        time.sleep(1)

    # ----- 2) Publicador sensor -----
    if not args.sensor_pass:
        sys.exit("Falta --sensor-pass (la contraseña del usuario MQTT 'sensor').")
    try:
        sensor = make_client("test-sensor", args.sensor_user, args.sensor_pass,
                             args.host, args.port, args.tls, args.insecure)
    except Exception as e:
        sys.exit(f"[SENSOR] No se pudo conectar: {e}")
    sensor.loop_start()
    print("[SENSOR] conectado")

    payload = json.dumps({
        "device": "test-script", "bpm": 78.0, "spo2": 97.0,
        "finger": True, "beat": True, "ir": 54213, "ts": int(time.time())
    })
    info = sensor.publish(topic_out, payload, qos=1)
    info.wait_for_publish(timeout=5)
    print(f"[SENSOR] -> publicado en {topic_out}: {payload}")

    # ----- 3) Verificación -----
    ok = True
    if not args.only_publish:
        deadline = time.time() + 5
        while not received and time.time() < deadline:
            time.sleep(0.2)
        if received:
            print("\n RESULTADO: El backend recibió el mensaje del sensor. Flujo sensor->nube OK.")
        else:
            ok = False
            print("\n RESULTADO: No llegó el mensaje al backend en 5s.")
            print("   Revisa el ACL (sensor debe poder PUBLICAR en .../sensor/out y")
            print("   backend SUSCRIBIRSE a ese tópico) y las credenciales.")

        # ----- 4) Camino inverso: backend publica un comando en .../sensor/in -----
        cmd = json.dumps({"cmd": "ping", "msg": "hola sensor"})
        backend.publish(topic_in, cmd, qos=1).wait_for_publish(timeout=5)
        print(f"[BACKEND] -> comando publicado en {topic_in}: {cmd}")
        print("   (El ESP32 real lo recibiría en su callback MQTT.)")

    time.sleep(0.5)
    sensor.loop_stop(); sensor.disconnect()
    if backend:
        backend.loop_stop(); backend.disconnect()

    sys.exit(0 if ok else 2)


if __name__ == "__main__":
    main()
