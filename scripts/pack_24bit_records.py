#!/usr/bin/env python3
"""Convert validated 4-byte records [3-byte word + 0x00 padding] to packed 24-bit bytes."""
import sys
from pathlib import Path

src, dst = map(Path, sys.argv[1:3])
start = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x144
end = int(sys.argv[4], 0) if len(sys.argv) > 4 else None
data = src.read_bytes()
records = data[start:end]
if len(records) % 4:
    raise SystemExit(f"region length {len(records)} is not divisible by 4")
out = bytearray()
for off in range(0, len(records), 4):
    rec = records[off:off+4]
    if rec[3] != 0:
        raise SystemExit(f"unexpected padding {rec[3]:02x} at file offset {start+off+3:#x}")
    out.extend(rec[:3])
dst.parent.mkdir(parents=True, exist_ok=True)
dst.write_bytes(out)
print(f"Wrote {len(out)} bytes to {dst}")
