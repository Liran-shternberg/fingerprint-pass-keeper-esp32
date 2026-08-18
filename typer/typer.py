#!/usr/bin/env python3
import os
import socket
import subprocess
import time

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

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

PRIVATE_KEY = serialization.load_pem_private_key(
    open(os.path.join(BASE, "private_key.pem"), "rb").read(), password=None)

last_msg = ""
last_time = 0.0


def rsa_decrypt(data):
    try:
        return PRIVATE_KEY.decrypt(
            data,
            padding.OAEP(mgf=padding.MGF1(hashes.SHA256()),
                         algorithm=hashes.SHA256(), label=None),
        ).decode(errors="replace")
    except ValueError:
        return ""


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
        msg = rsa_decrypt(data).strip()
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
