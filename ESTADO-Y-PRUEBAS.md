# Proyecto VitalGuard IoT — Estado y entrega

## 1. Qué se hizo

Se tomó el firmware **VitalGuard** (ESP32-S3 + sensor MAX30102 que mide pulso `BPM`,
oxígeno `SpO2` y la onda `PPG`, con un dashboard web) y se le añadió **conectividad
a la nube**, siguiendo como guía el proyecto *Integrador*. El resultado es un proyecto
nuevo llamado **`vitalguard-iot/`** que:

- Se conecta a un **WiFi real** (modo estación) en vez de crear su propia red.
- **Publica los datos biométricos por MQTT cifrado (TLS)** al broker EMQX cada ~2 segundos.
- **Escucha comandos** del backend por MQTT.
- Soporta **actualización de firmware por aire (OTA)** descargando el binario desde **S3**,
  automatizado con **GitHub Actions**.
- Conserva el **dashboard web local** original (gráfica del latido en tiempo real).

El ESP32 actúa como el usuario **`sensor`** del broker: publica en `.../sensor/out` y
escucha en `.../sensor/in`, respetando el ACL definido.

---

## 2. Qué ya se probó y FUNCIONA (verificado contra la instancia real)

| Prueba | Resultado |
|---|---|
| Compilación del firmware | ✅ Compila sin errores (usa 51% del espacio, queda margen para OTA) |
| Conexión MQTT cifrada (TLS, puerto 8883) | ✅ Conecta como `sensor` y `backend` |
| Sensor → nube: publica en `.../sensor/out` y el backend lo recibe | ✅ |
| Nube → sensor: el backend manda comando en `.../sensor/in` y el sensor lo recibe | ✅ |
| Subida del firmware a S3 | ✅ |
| Integridad del binario en S3 (descarga verificada byte a byte) | ✅ |
| **OTA de punta a punta** (comando MQTT → recepción → descarga del binario) | ✅ Funciona (falta solo el flasheo físico) |

> Toda la parte de nube (mensajería + OTA) está **probada en vivo**, no en teoría.

---

## 3. Credenciales (como quedaron)

> ⚠️ **Las credenciales reales NO están en este documento público.** Quedan en el
> archivo local `CREDENCIALES-LOCAL.md` (que está en `.gitignore` y no se sube).
> Pídeselas a algún integrante del equipo.

### AWS / S3 (para el OTA)
```
IP instancia      : 56.126.147.67
Dominio principal : lermita.freeddns.org
Dominio S3        : firmware.lermita.freeddns.org
S3 Access Key ID  : (ver CREDENCIALES-LOCAL.md)
S3 Secret Key     : (ver CREDENCIALES-LOCAL.md)
Región            : sa-east-1
```

### Dashboard EMQX (panel web del broker)
```
URL     : http://lermita.freeddns.org:18083
Usuario : admin
Password: (ver CREDENCIALES-LOCAL.md)
```

### Broker MQTT (lo usan el ESP32 y el backend)
```
Host  : lermita.freeddns.org
Puerto: 8883  (TLS obligatorio; el 1883 está cerrado por fuera)
TLS   : SÍ (certificado Let's Encrypt — ya soportado por el firmware)
```

| Usuario MQTT | Contraseña | Uso |
|---|---|---|
| `sensor`  | (ver CREDENCIALES-LOCAL.md) | El ESP32 |
| `backend` | (ver CREDENCIALES-LOCAL.md) | El backend del equipo |

### Tópicos MQTT
```
colombia/valle/tulua/univalle/sensor/out    → el sensor PUBLICA; el backend LEE
colombia/valle/tulua/univalle/sensor/in     → el backend manda comandos; el sensor LEE
colombia/valle/tulua/univalle/backend/out   → canal propio del backend
colombia/valle/tulua/univalle/backend/in    → canal propio del backend
```

### WiFi (lo único de credenciales que falta)
```
WIFI_SSID     : (PENDIENTE — poner la red 2.4 GHz)
WIFI_PASSWORD : (PENDIENTE)
```

---

## 4. Qué falta hacer y quién debe hacerlo

| # | Pendiente | Responsable |
|---|---|---|
| 1 | Poner el **WiFi** (red 2.4 GHz) en `vitalguard-iot/.env` y **grabar el firmware** en el ESP32 con `python3 scripts/build_with_env.py run -t upload -t monitor` | **Tú** (quien tenga la placa) |
| 2 | Probar el **sensor físico**: dedo en el MAX30102, ver BPM/SpO2 en el serie y en el dashboard | **Tú** |
| 3 | Que el **backend** del equipo se suscriba a `.../sensor/out` y consuma los datos (usar credenciales de `backend`) | **Compañero del backend** |
| 4 | **Configurar los GitHub Secrets** del repositorio para que el OTA automático funcione (AWS, MQTT, WiFi, backend, etc.) | **Quien administre el repo** |
| 5 | **Arreglo opcional de AWS**: el dominio `firmware.lermita.freeddns.org` tiene un **bucle de redirección** y los objetos **no son públicos**. Se resolvió por código usando URLs prefirmadas, pero si se quiere usar URLs simples hay que corregir la config del bucket S3 | **Compañero que administra la instancia AWS** |
| 6 | **Borrar el objeto de prueba** `firmware_vTEST-20260611-031142.bin` que quedó en S3 (la llave IAM no tiene permiso de borrado) | **Compañero de AWS** |
| 7 | **Rotar todas las credenciales** al finalizar el proyecto (quedaron expuestas durante el desarrollo) | **El equipo** |

---

## 5. Notas técnicas (hallazgos)

- El broker solo expone **MQTT por TLS en el 8883** (el 1883 está bloqueado por el firewall de AWS).
- El dominio del firmware es un **sitio web estático de S3 mal configurado** (redirección infinita)
  y con objetos privados; por eso el OTA usa **URLs prefirmadas** (válidas 7 días) generadas por el
  workflow, que no necesitan que el bucket sea público.
- Las credenciales del firmware **no van escritas en el código**: se inyectan al compilar desde el
  archivo `.env`, que **no se sube a git**.
