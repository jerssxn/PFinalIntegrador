# VitalGuard IoT — ESP32-S3 + MAX30102 conectado a la nube (MQTT + OTA)

Este proyecto toma el firmware **VitalGuard** (mide pulso `BPM`, oxígeno `SpO2`
y la onda `PPG` con un sensor MAX30102 y una pantalla OLED) y le añade la
**conectividad a la nube** del proyecto *Integrador*:

- Se conecta a un **WiFi real** (modo estación).
- **Publica los datos biométricos por MQTT** al broker EMQX.
- **Escucha comandos** del backend por MQTT.
- Soporta **actualización de firmware por aire (OTA)** descargando el binario
  desde **S3**, orquestado por **GitHub Actions**.
- Conserva el **dashboard web local** original (ahora accesible por la IP que
  el router le asigna al ESP32).

---

## 1. ¿Cómo encaja todo? (vista general)

```
┌─────────────────── ESP32-S3 ───────────────────┐
│  Core 0  sensorTask()   MAX30102 → BPM/SpO2/PPG │
│  Core 1  loop()         WiFi + WebSocket + MQTT │
└───────────────┬─────────────────────────────────┘
                │ publica JSON cada 2 s
                ▼
   colombia/valle/tulua/univalle/sensor/out
                │
                ▼
     Broker EMQX (lermita.freeddns.org)  ⇄  Backend
                ▲
                │ comandos / orden OTA
   colombia/valle/tulua/univalle/sensor/in
```

El ESP32 actúa como el **usuario `sensor`** del ACL del broker:

| Acción | Tópico | Quién |
|---|---|---|
| Publicar biométricos | `colombia/valle/tulua/univalle/sensor/out` | sensor (ESP32) |
| Recibir comandos / OTA | `colombia/valle/tulua/univalle/sensor/in` | sensor (ESP32) |

> El tópico se arma solo a partir de `COUNTRY/STATE/CITY/INSTITUTION/MQTT_USER`
> definido en el `.env`. Con los valores por defecto da exactamente los de arriba.

---

## 2. Estructura del proyecto

```
vitalguard-iot/
├── platformio.ini          # Configuración de compilación + inyección de credenciales
├── .env                    # Tus credenciales reales (NO se sube a git)
├── .env.template           # Plantilla de credenciales
├── scripts/
│   └── build_with_env.py   # Carga el .env y ejecuta PlatformIO
├── src/
│   ├── main.cpp            # Integración: sensor + red + dashboard
│   ├── webpage.h           # Dashboard web (HTML/JS) — sin cambios
│   ├── secrets.{h,cpp}     # Credenciales inyectadas + armado de tópicos + CA raíz
│   ├── net_wifi.{h,cpp}    # WiFi modo estación
│   ├── net_mqtt.{h,cpp}    # Cliente MQTT (TLS o plano), publish y callback
│   └── net_ota.{h,cpp}     # Actualización OTA desde S3
└── .github/workflows/
    └── ci.yml              # CI: compila, sube a S3 y publica el comando OTA
```

---

## 3. Configuración (antes de compilar)

1. Copia la plantilla y completa tus datos:

```bash
cp .env.template .env
```

2. Edita `.env` y rellena lo que falta:

```ini
MQTT_PASSWORD=...     # contraseña del usuario MQTT "sensor"
WIFI_SSID=...         # tu red WiFi (puede ser el hotspot del celular)
WIFI_PASSWORD=...
MQTT_PORT=1883        # 1883 (plano) u 8883 (TLS)
MQTT_TLS=0            # 0 = sin cifrar (1883) ; 1 = TLS (8883)
```

> **¿1883 o 8883?** Míralo en el dashboard de EMQX → *Management → Listeners*.
> Si hay un listener `ssl` en 8883, usa `MQTT_PORT=8883` y `MQTT_TLS=1`. Si solo
> hay `tcp` en 1883, usa `MQTT_PORT=1883` y `MQTT_TLS=0`. El dashboard en `:18083`
> **no** es el puerto MQTT.

> **Nota TLS:** si usas TLS y el broker tiene un certificado de Let's Encrypt, el
> CA raíz incluido (ISRG Root X1) ya sirve. Si es autofirmado y la conexión falla,
> en `net_mqtt.cpp` puedes usar `tlsClient.setInsecure();` (solo para pruebas).

---

## 4. Compilar y subir al ESP32-S3

Conecta la placa por USB y ejecuta:

```bash
# Compilar
python scripts/build_with_env.py run

# Compilar + subir
python scripts/build_with_env.py run -t upload

# Compilar + subir + ver el monitor serie
python scripts/build_with_env.py run -t upload -t monitor
```

El script carga el `.env`, lo inyecta como credenciales y llama a PlatformIO.

> ¿No tienes PlatformIO CLI? Instálalo con `pip install platformio`, o usa la
> extensión **PlatformIO IDE** de VS Code (necesitarás cargar las variables del
> `.env` en el entorno antes de pulsar *Build*; el script lo hace por ti).

### Qué verás al arrancar (monitor serie a 115200)

1. Conexión WiFi y la **IP** asignada.
2. Configuración y conexión MQTT (`PUB`/`SUB`).
3. `[SENSOR] Esperando dedo...` → coloca el dedo y verás los `[BEAT]`.
4. En la OLED, abajo a la derecha: `W` = WiFi OK, `M` = MQTT OK.

### Dashboard local

Abre en el navegador `http://<IP-del-ESP32>` (la IP que mostró el serie / OLED).
Verás la gráfica PPG, BPM, SpO2 y el historial, igual que el VitalGuard original.

---

## 5. Comprobar que llegan los datos al broker

En el **dashboard de EMQX** (`http://lermita.freeddns.org:18083`,
usuario `admin`) puedes usar *Diagnose → WebSocket Client* o *MQTT Client* para
**suscribirte** al tópico y ver los mensajes:

```
Tópico:  colombia/valle/tulua/univalle/sensor/out
```

Cada ~2 segundos deberías recibir algo así:

```json
{"device":"vitalguard-AABBCC","bpm":78.0,"spo2":97.0,"finger":true,"beat":true,"ir":54213,"ts":1718045000}
```

Para **enviar un comando** al ESP32, publica (como el usuario `backend`) en:

```
Tópico:  colombia/valle/tulua/univalle/sensor/in
```

---

## 6. Actualización OTA (opcional, automatizada con GitHub Actions)

### Cómo funciona

1. Creas un tag de versión y lo subes a GitHub:

```bash
git tag v1.0.1
git push origin v1.0.1
```

2. GitHub Actions (`.github/workflows/ci.yml`):
   - compila el firmware,
   - sube `firmware_v1.0.1.bin` a **S3**,
   - publica por MQTT en `.../sensor/in` el comando:
     ```json
     {"cmd":"ota","version":"v1.0.1","url":"https://firmware.lermita.freeddns.org/firmware_v1.0.1.bin"}
     ```
3. El ESP32 recibe el comando, descarga el `.bin` y se reprograma solo.

> El workflow publica con el usuario **`backend`** porque, según el ACL, el
> usuario `sensor` **no** puede publicar en `.../sensor/in` (solo escucharlo).

### Secrets a configurar en GitHub (Settings → Secrets and variables → Actions)

| Secret | Valor sugerido |
|---|---|
| `COUNTRY` / `STATE` / `CITY` / `INSTITUTION` | `colombia` / `valle` / `tulua` / `univalle` |
| `MQTT_SERVER` | `lermita.freeddns.org` |
| `MQTT_PORT` / `MQTT_TLS` | `1883` / `0` (o `8883` / `1`) |
| `MQTT_USER` / `MQTT_PASSWORD` | credenciales del `sensor` (para compilar el firmware) |
| `WIFI_SSID` / `WIFI_PASSWORD` | red WiFi de producción |
| `BACKEND_MQTT_USER` / `BACKEND_MQTT_PASSWORD` | credenciales del usuario `backend` |
| `AWS_ACCESS_KEY_ID` / `AWS_SECRET_ACCESS_KEY` | claves S3 |
| `AWS_REGION` | `sa-east-1` |
| `S3_BUCKET` | nombre del bucket (p. ej. `firmware.lermita.freeddns.org`) |
| `S3_PUBLIC_BASE_URL` | `https://firmware.lermita.freeddns.org` |

> ⚠️ **Seguridad:** las credenciales que compartiste en el chat (claves AWS,
> contraseña del dashboard) quedaron expuestas. Es muy recomendable **rotarlas**
> y guardarlas únicamente como GitHub Secrets / en el `.env` local.

---

## 7. Diferencias clave frente al VitalGuard original

| Antes (VitalGuard) | Ahora (VitalGuard IoT) |
|---|---|
| WiFi en modo Access Point (red propia, sin internet) | WiFi en modo estación (se une a una red con internet) |
| Datos solo en el dashboard local | Datos también publicados por MQTT a la nube |
| Sin comandos remotos | Escucha comandos del backend por `.../sensor/in` |
| Sin actualización remota | OTA desde S3 vía GitHub Actions |
| Credenciales en el código | Credenciales en `.env`, inyectadas al compilar |

---

## 8. Problemas comunes

| Síntoma | Causa probable / solución |
|---|---|
| `[WIFI] No se pudo conectar` | SSID/clave incorrectos, o la red es 5GHz (el ESP32 solo usa 2.4GHz) |
| `[MQTT] Falló (state=5)` | Usuario/contraseña MQTT incorrectos, o el usuario no es `sensor` |
| `[MQTT] Falló (state=-2)` | No alcanza el broker: puerto/host mal, o TLS mal configurado |
| No llega nada a `.../sensor/out` | Revisa que el ACL permita publicar y que `MQTT_TLS`/puerto coincidan |
| OTA no inicia | El JSON debe traer `url` con un `.bin`; revisa el CA si la URL es `https` |
| La OLED dice `ERROR: MAX30102` | Revisa cableado I2C (SDA=8, SCL=9, VIN=3V3) |
```
