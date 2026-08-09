#!/usr/bin/env python3
import os
import socket
import subprocess
import time

BASE = os.path.dirname(os.path.abspath(__file__))


def load_env(path):
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, _, v = line.partition("=")
                os.environ.setdefault(k.strip(), v.strip())
    except FileNotFoundError:
        pass


load_env(os.path.join(BASE, ".env"))

HOST, PORT = "0.0.0.0", 4444
TOKEN = os.environ.get("AUTH_TOKEN", "TOKEN")
KEY = os.environ.get("XOR_KEY", "xor_decrypt").encode()

last_msg = ""
last_time = 0.0


def xor_decrypt(data):
    return bytes(b ^ KEY[i % len(KEY)] for i, b in enumerate(data)).decode(errors="replace")


def main():
    global last_msg, last_time
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    print(f"[typer] listening udp :{PORT}", flush=True)
    while True:
        try:
            data, addr = s.recvfrom(512)
        except Exception as e:
            print(f"[typer] recv error: {e}", flush=True)
            continue
        msg = xor_decrypt(data).strip()
        now = time.time()
        if msg == last_msg and now - last_time < 1.0:
            continue
        last_msg, last_time = msg, now
        tok, _, pwd = msg.partition(" ")
        if tok != TOKEN:
            continue
        try:
            if pwd:
                subprocess.run(["wtype", "--", pwd])
            subprocess.run(["wtype", "-k", "Return"])
        except Exception as e:
            print(f"[typer] wtype failed: {e}", flush=True)


if __name__ == "__main__":
    main()
