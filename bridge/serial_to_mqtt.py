#!/usr/bin/env python3
"""Puente serie -> MQTT (TagoIO).

Lee el puerto serie de la placa y publica en el broker cada línea JSON que
empiece con '[' (las inferencias), como máximo una por minuto (--interval). La
placa sigue infiriendo y mostrando todo por serie; el resto de las líneas solo
se muestran por pantalla.

Credenciales del laboratorio fijas en el código (TOKEN). Si existe la variable de
entorno TAGO_TOKEN, tiene prioridad.

Uso:
    python tools/serial_to_mqtt.py COM7
    python tools/serial_to_mqtt.py COM7 --tls        # puerto 8883

Dependencias:  pip install pyserial paho-mqtt
"""
import argparse
import json
import os
import time

import paho.mqtt.client as mqtt
import serial

TOKEN = "6d714d22-f8f2-452f-9af3-187380c9d79f"


def make_client(args, token):
    # paho-mqtt 2.x exige indicar la versión de la API de callbacks; 1.x no la tiene.
    if hasattr(mqtt, "CallbackAPIVersion"):
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    else:
        client = mqtt.Client()
    client.username_pw_set("Token", token)
    if args.tls:
        client.tls_set()  # valida el certificado con las CA del sistema
    client.on_connect = lambda c, u, f, rc, *a: print(f"[mqtt] conectado (rc={rc})")
    client.on_disconnect = lambda c, u, *a: print("[mqtt] desconectado, reintentando...")
    client.connect(args.host, 8883 if args.tls else args.mqtt_port, keepalive=60)
    client.loop_start()  # hilo en segundo plano; reconecta solo
    return client


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("serial_port", help="puerto serie de la placa, ej. COM7")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--host", default="mqtt.tago.io")
    p.add_argument("--mqtt-port", type=int, default=1883)
    p.add_argument("--tls", action="store_true", help="usar TLS (puerto 8883)")
    p.add_argument("--topic", default="data")
    p.add_argument("--interval", type=float, default=60, help="segundos entre publicaciones (default 60)")
    args = p.parse_args()

    token = os.environ.get("TAGO_TOKEN", TOKEN)

    client = make_client(args, token)
    last_publish = None  # instante (monotonic) de la última publicación
    with serial.Serial(args.serial_port, args.baud, timeout=1) as ser:
        print(f"[serie] escuchando {args.serial_port} a {args.baud} baudios")
        while True:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue
            print(line)
            if not line.startswith("["):
                continue
            try:
                json.loads(line)  # no publicar líneas truncadas o corruptas
            except json.JSONDecodeError:
                print("[mqtt] JSON inválido, se descarta")
                continue
            now = time.monotonic()
            if last_publish is not None and now - last_publish < args.interval:
                continue  # todavía no pasó el intervalo; se descarta esta inferencia
            last_publish = now
            info = client.publish(args.topic, line, qos=1)
            info.wait_for_publish(timeout=5)
            print(f"[mqtt] publicado en '{args.topic}' ({time.strftime('%H:%M:%S')})")


if __name__ == "__main__":
    main()
