import os
import re
import unittest

import gen_ca_bundle

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_entries(data):
    count = int.from_bytes(data[:2], "big")
    pos, entries = 2, []
    for _ in range(count):
        name_len = int.from_bytes(data[pos:pos + 2], "big")
        key_len = int.from_bytes(data[pos + 2:pos + 4], "big")
        start = pos + 4
        entries.append((data[start:start + name_len], data[start + name_len:start + name_len + key_len]))
        pos = start + name_len + key_len
    return entries, pos


class GenCaBundleTest(unittest.TestCase):
    def github_roots(self):
        # The updater's pinned roots are a handy set of real certificates
        with open(os.path.join(ROOT, "lib", "Updater", "GitHubRoots.h"), encoding="utf-8") as f:
            return f.read()

    def test_entries_are_subject_and_key_sorted_by_subject(self):
        pem = self.github_roots()
        data, count = gen_ca_bundle.bundle(pem)
        self.assertEqual(count, len(re.findall("BEGIN CERTIFICATE", pem)))
        entries, end = read_entries(data)
        self.assertEqual(end, len(data))
        self.assertEqual(len(entries), count)
        self.assertEqual([subject for subject, _ in entries], sorted(subject for subject, _ in entries))
        for subject, key in entries:
            self.assertEqual(subject[0], 0x30)   # Name is a SEQUENCE
            self.assertEqual(key[0], 0x30)       # SubjectPublicKeyInfo is a SEQUENCE
            self.assertIn(b"\x06", key[:8])       # the algorithm OID comes first

    def test_committed_bundle_is_well_formed(self):
        with open(os.path.join(ROOT, "certs", "x509_crt_bundle.bin"), "rb") as f:
            data = f.read()
        entries, end = read_entries(data)
        self.assertEqual(end, len(data))
        self.assertGreater(len(entries), 100)
        self.assertEqual([subject for subject, _ in entries], sorted(subject for subject, _ in entries))


if __name__ == "__main__":
    unittest.main()
