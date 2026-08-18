#!/usr/bin/env python3
import os

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa

BASE = os.path.dirname(os.path.abspath(__file__))

key = rsa.generate_private_key(public_exponent=65537, key_size=2048)

priv_path = os.path.join(BASE, "private_key.pem")
with open(priv_path, "wb") as f:
    f.write(key.private_bytes(
        serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8,
        serialization.NoEncryption(),
    ))
os.chmod(priv_path, 0o600)

with open(os.path.join(BASE, "public_key.pem"), "wb") as f:
    f.write(key.public_key().public_bytes(
        serialization.Encoding.PEM,
        serialization.PublicFormat.SubjectPublicKeyInfo,
    ))

print(f"wrote {priv_path} (keep secret) and typer/public_key.pem")
