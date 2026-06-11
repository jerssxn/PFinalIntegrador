"""
inject_env.py — Inyecta las credenciales del .env como #define al compilar.

PlatformIO ejecuta este script (extra_scripts = pre:...) ANTES de compilar.
Lee el archivo .env del proyecto y agrega los valores como macros del
preprocesador usando env.StringifyMacro(), que escapa correctamente espacios
y caracteres especiales (p. ej. un SSID como "Starlink 5g").

Ventajas frente a inyectar con build_flags + ${sysenv.VAR}:
  - Soporta espacios y símbolos en SSID/contraseñas sin romper las comillas.
  - Funciona aunque compiles desde el botón de PlatformIO en el IDE (lee el
    .env directamente, no depende de variables de entorno).

Las credenciales siguen SIN quedar escritas en el código fuente: viven en
.env (ignorado por git).
"""
Import("env")  # noqa: F821  (lo provee PlatformIO)
import os

PROJECT_DIR = env["PROJECT_DIR"]
ENV_PATH = os.path.join(PROJECT_DIR, ".env")

# Valores por defecto: la compilación nunca falla por una variable faltante
# (aunque la conexión real no funcione hasta completarlas en el .env).
DEFAULTS = {
    "COUNTRY": "colombia",
    "STATE": "valle",
    "CITY": "tulua",
    "INSTITUTION": "univalle",
    "MQTT_SERVER": "lermita.freeddns.org",
    "MQTT_PORT": "1883",
    "MQTT_USER": "sensor",
    "MQTT_PASSWORD": "changeme",
    "MQTT_TLS": "0",
    "WIFI_SSID": "changeme",
    "WIFI_PASSWORD": "changeme",
}

# Macros que son cadenas de texto (se inyectan como "..." con comillas).
STRING_KEYS = [
    "COUNTRY", "STATE", "CITY", "INSTITUTION",
    "MQTT_SERVER", "MQTT_USER", "MQTT_PASSWORD",
    "WIFI_SSID", "WIFI_PASSWORD",
]
# Macros numéricas (se inyectan como entero, sin comillas).
INT_KEYS = ["MQTT_PORT", "MQTT_TLS"]


def load_env(path):
    values = {}
    if not os.path.exists(path):
        print(f"[inject_env] AVISO: no se encontro {path}. Usando valores por defecto.")
        return values
    with open(path, "r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, val = line.partition("=")
            values[key.strip()] = val.strip().strip('"').strip("'")
    return values


file_vals = load_env(ENV_PATH)

merged = dict(DEFAULTS)
for key in DEFAULTS:
    # Prioridad: .env del proyecto > variable de entorno > valor por defecto.
    if file_vals.get(key):
        merged[key] = file_vals[key]
    elif os.environ.get(key):
        merged[key] = os.environ[key]

defines = []
for key in STRING_KEYS:
    defines.append((key, env.StringifyMacro(merged[key])))
for key in INT_KEYS:
    try:
        defines.append((key, int(str(merged[key]).strip())))
    except (ValueError, TypeError):
        defines.append((key, 0))

env.Append(CPPDEFINES=defines)

print("[inject_env] Credenciales inyectadas desde .env:")
for key in DEFAULTS:
    shown = "***" if "PASSWORD" in key else merged[key]
    print(f"    {key} = {shown}")
