#!/usr/bin/env python3
"""
Carga las variables del archivo .env al entorno y ejecuta PlatformIO.

PlatformIO lee esas variables en platformio.ini con ${sysenv.VARIABLE}
y las inyecta como #define al compilar (build_flags). Así las credenciales
nunca quedan escritas en el código fuente.

Uso:
    python scripts/build_with_env.py run               # compilar
    python scripts/build_with_env.py run -t upload      # compilar + subir
    python scripts/build_with_env.py run -t monitor     # monitor serie
    python scripts/build_with_env.py run -t upload -t monitor

Cualquier argumento extra se pasa tal cual a `pio`.
"""
import os
import subprocess
import sys

# Valores por defecto: garantizan que la compilación nunca falle por una
# variable faltante (aunque la conexión real no funcione hasta completarlas).
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

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENV_PATH = os.path.join(PROJECT_ROOT, ".env")


def load_env(path):
    values = {}
    if not os.path.exists(path):
        print(f"[build_with_env] AVISO: no se encontró {path}. Usando valores por defecto.")
        return values
    with open(path, "r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, val = line.partition("=")
            key = key.strip()
            val = val.strip().strip('"').strip("'")
            values[key] = val
    return values


def main():
    env_values = load_env(ENV_PATH)

    # Mezclar: primero defaults, luego lo que venga del .env (tiene prioridad).
    merged = dict(DEFAULTS)
    merged.update({k: v for k, v in env_values.items() if v != ""})

    new_env = dict(os.environ)
    new_env.update(merged)

    print("[build_with_env] Variables aplicadas:")
    for key in DEFAULTS:
        shown = merged.get(key, "")
        if "PASSWORD" in key:
            shown = "***" if shown else "(vacío)"
        print(f"    {key} = {shown}")

    pio_args = sys.argv[1:] if len(sys.argv) > 1 else ["run"]
    cmd = ["pio"] + pio_args
    print(f"[build_with_env] Ejecutando: {' '.join(cmd)}\n")

    try:
        result = subprocess.run(cmd, env=new_env, cwd=PROJECT_ROOT)
    except FileNotFoundError:
        print("[build_with_env] ERROR: no se encontró 'pio'. Instala PlatformIO Core "
              "o usa el botón de PlatformIO en VS Code.")
        sys.exit(1)
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
