"""Check uptime publications and 50 rendered HTTP log rows concurrently."""

import argparse
import concurrent.futures
import re
import socket
import struct
import time
import urllib.request
import uuid


TOPIC = b"DataSoft/dscan/uptime"


def mqtt_string(value):
    return struct.pack("!H", len(value)) + value


def packet(header, body):
    length = len(body)
    result = bytearray([header])
    while True:
        digit = length % 128
        length //= 128
        result.append(digit | (128 if length else 0))
        if not length:
            return bytes(result) + body


def read_exact(connection, size):
    result = bytearray()
    while len(result) < size:
        chunk = connection.recv(size - len(result))
        if not chunk:
            raise RuntimeError("MQTT connection closed unexpectedly")
        result.extend(chunk)
    return bytes(result)


def receive_packet(connection):
    header = read_exact(connection, 1)[0]
    length = 0
    multiplier = 1
    for _ in range(4):
        digit = read_exact(connection, 1)[0]
        length += (digit & 127) * multiplier
        if not digit & 128:
            if length > 4096:
                raise RuntimeError("Unexpectedly large MQTT packet")
            return header, read_exact(connection, length)
        multiplier *= 128
    raise RuntimeError("Invalid MQTT remaining length")


def check_mqtt(broker, port):
    samples = []
    with socket.create_connection((broker, port), timeout=5) as connection:
        connection.settimeout(15)
        client_id = ("dscan-check-" + uuid.uuid4().hex[:12]).encode("ascii")
        connection.sendall(
            packet(0x10, b"\x00\x04MQTT\x04\x02\x00\x78" + mqtt_string(client_id))
        )
        if receive_packet(connection) != (0x20, b"\x00\x00"):
            raise RuntimeError("MQTT CONNACK rejected")
        connection.sendall(packet(0x82, b"\x00\x01" + mqtt_string(TOPIC) + b"\x00"))
        if receive_packet(connection) != (0x90, b"\x00\x01\x00"):
            raise RuntimeError("MQTT SUBACK rejected")
        for _ in range(7):
            header, body = receive_packet(connection)
            if header != 0x30 or len(body) < 2:
                raise RuntimeError("Expected a non-retained QoS 0 publication")
            topic_length = int.from_bytes(body[:2], "big")
            topic = body[2 : 2 + topic_length]
            payload = body[2 + topic_length :]
            if topic != TOPIC or not payload.isdigit():
                raise RuntimeError(f"Unexpected MQTT topic/payload: {topic!r} {payload!r}")
            sample = (time.monotonic(), int(payload))
            if samples:
                interval = sample[0] - samples[-1][0]
                uptime_delta = sample[1] - samples[-1][1]
                if not (9 <= interval <= 12 and 9 <= uptime_delta <= 12):
                    raise RuntimeError(
                        f"MQTT interval {interval:.2f}s, uptime delta {uptime_delta}s"
                    )
            samples.append(sample)
        connection.sendall(b"\xe0\x00")
    return samples


def check_http(device, path):
    with urllib.request.urlopen(f"http://{device}{path}", timeout=5) as response:
        if response.status != 200:
            raise RuntimeError(f"HTTP {response.status} for {path}")
        text = response.read().decode("utf-8")
    rows = re.findall(r"<!--#w(\d+)-->(.*?)<br>", text, re.S)
    if [int(number) for number, _ in rows] != list(range(1, 51)):
        raise RuntimeError(f"{path}: expected log rows w1..w50, got {len(rows)}")
    if "unknown tag" in text.lower() or "<title>STM32 F407</title>" not in text:
        raise RuntimeError(f"{path}: incorrect SSI rendering or page title")
    if not any(row.strip() for _, row in rows):
        raise RuntimeError(f"{path}: all rendered log rows are empty")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default="192.168.1.249")
    parser.add_argument("--broker", default="192.168.1.2")
    parser.add_argument("--port", type=int, default=1883)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")

    start = time.monotonic()
    http_checks = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
        mqtt = pool.submit(check_mqtt, args.broker, args.port)
        while time.monotonic() - start < 65:
            if mqtt.done():
                mqtt.result()
            for path in ("/", "/index.shtml"):
                check_http(args.device, path)
                http_checks += 1
            time.sleep(5)
        samples = mqtt.result(timeout=15)

    intervals = [
        round(samples[i][0] - samples[i - 1][0], 2) for i in range(1, len(samples))
    ]
    print("PASS: MQTT uptime values:", [sample[1] for sample in samples])
    print("PASS: MQTT intervals (seconds):", intervals)
    print(f"PASS: {http_checks} HTTP 200 responses, 50 rendered log rows each")
    print(f"Elapsed: {time.monotonic() - start:.1f}s")


if __name__ == "__main__":
    main()
