"""Builds certs/x509_crt_bundle.bin, the root certificates the notifications check HTTPS against.

    python tools/gen_ca_bundle.py [cacert.pem]

Without an argument it reads the Mozilla list that ships with certifi (PlatformIO's Python has it).
Run it again now and then to pick up new and retired root certificates, and commit the result.

The format is the one ESP-IDF's esp_crt_bundle reads (WiFiClientSecure::setCACertBundle):
  [number of certificates, 2 bytes big-endian]
  per certificate: [subject length, 2 bytes][public key length, 2 bytes][subject DER][SubjectPublicKeyInfo DER]
sorted by subject, because the ESP32 finds the issuer of a server certificate with a binary search.
Only the subject and the public key are kept: that's all the signature check needs.
"""
import base64
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "certs", "x509_crt_bundle.bin")


def read_tlv(data, pos):
    """One DER element at pos: (tag, start of the element, start of its content, end of the element)."""
    tag = data[pos]
    length = data[pos + 1]
    content = pos + 2
    if length & 0x80:
        count = length & 0x7F
        length = int.from_bytes(data[content:content + count], "big")
        content += count
    return tag, pos, content, content + length


def subject_and_key(der):
    """The raw subject and SubjectPublicKeyInfo of a certificate."""
    _, _, cert_content, _ = read_tlv(der, 0)                 # Certificate SEQUENCE
    _, _, tbs_content, tbs_end = read_tlv(der, cert_content)  # tbsCertificate SEQUENCE
    fields = []
    pos = tbs_content
    while pos < tbs_end:
        tag, start, _, end = read_tlv(der, pos)
        fields.append((tag, der[start:end]))
        pos = end
    if fields[0][0] == 0xA0:                                 # [0] version is optional
        fields = fields[1:]
    # serialNumber, signature, issuer, validity, subject, subjectPublicKeyInfo
    return fields[4][1], fields[5][1]


def pem_certificates(text):
    for body in re.findall(r"-----BEGIN CERTIFICATE-----(.*?)-----END CERTIFICATE-----", text, re.S):
        yield base64.b64decode("".join(body.split()))


def bundle(pem_text):
    entries = sorted(subject_and_key(der) for der in pem_certificates(pem_text))
    out = bytearray(len(entries).to_bytes(2, "big"))
    for subject, key in entries:
        out += len(subject).to_bytes(2, "big") + len(key).to_bytes(2, "big") + subject + key
    return bytes(out), len(entries)


def default_pem():
    import certifi
    return certifi.where()


def main():
    source = sys.argv[1] if len(sys.argv) > 1 else default_pem()
    with open(source, encoding="ascii") as f:
        data, count = bundle(f.read())
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(data)
    print("%d certificates, %d bytes -> %s" % (count, len(data), os.path.relpath(OUT, ROOT)))


if __name__ == "__main__":
    main()
