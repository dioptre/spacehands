#!/usr/bin/env python3
"""Tiny UDP broadcast discovery for Spacehands Pi/projector roles.

Pi role:
  python3 scripts/discover.py announce

Projector role:
  python3 scripts/discover.py find
"""
import json
import socket
import sys
import time

PORT = 45545
MAGIC = "spacehands-v1"


def local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"
    finally:
        s.close()


def announce():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind(("", PORT))
    sock.settimeout(0.2)
    ip = local_ip()
    host = socket.gethostname()
    print(f"[discover] announcing Spacehands Pi at {ip}:{PORT}", flush=True)
    last_broadcast = 0.0
    while True:
        now = time.time()
        if now - last_broadcast >= 1.0:
            msg = json.dumps({
                "magic": MAGIC,
                "role": "pi",
                "host": host,
                "ip": ip,
                "http_port": 8080,
                "mjpeg_port": 8082,
            }).encode("utf-8")
            sock.sendto(msg, ("255.255.255.255", PORT))
            last_broadcast = now
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            continue
        except KeyboardInterrupt:
            break
        if data == b"SPACEHANDS_DISCOVER":
            reply = json.dumps({
                "magic": MAGIC,
                "role": "pi",
                "host": host,
                "ip": ip,
                "http_port": 8080,
                "mjpeg_port": 8082,
            }).encode("utf-8")
            sock.sendto(reply, addr)


def find(timeout=5.0):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind(("", 0))
    sock.settimeout(0.25)
    deadline = time.time() + timeout
    while time.time() < deadline:
        sock.sendto(b"SPACEHANDS_DISCOVER", ("255.255.255.255", PORT))
        try:
            data, _ = sock.recvfrom(2048)
        except socket.timeout:
            continue
        try:
            msg = json.loads(data.decode("utf-8"))
        except Exception:
            continue
        if msg.get("magic") == MAGIC and msg.get("role") == "pi" and msg.get("ip"):
            print(msg["ip"])
            return 0
    print("", end="")
    return 1


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "find"
    if cmd == "announce":
        announce()
    elif cmd == "find":
        timeout = float(sys.argv[2]) if len(sys.argv) > 2 else 5.0
        raise SystemExit(find(timeout))
    else:
        print("usage: discover.py announce|find [timeout]", file=sys.stderr)
        raise SystemExit(2)
