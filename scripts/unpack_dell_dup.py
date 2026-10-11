#!/usr/bin/env python3
"""Inspect/extract a payload from a Dell DUP container.

This script intentionally avoids guessing offsets. It prints candidate occurrences
of common binary markers and leaves final payload boundaries to evidence-based review.
"""
import sys
from pathlib import Path

p = Path(sys.argv[1])
data = p.read_bytes()
print(f"file={p} size={len(data)}")
print(f"first_64={data[:64].hex(' ')}")
for marker in (b"095HR5", b"190AD", b"VRTX", b"Dell"):
    offsets = []
    start = 0
    while True:
        pos = data.find(marker, start)
        if pos < 0:
            break
        offsets.append(pos)
        start = pos + 1
    if offsets:
        print(f"{marker!r}: " + ", ".join(hex(x) for x in offsets))
print("Use verified container structure/length fields before extracting any payload.")
