# Dell / Delta D1600E 95HR5 Firmware Reverse Engineering

This repository contains the analysis workspace for Dell/Delta D1600E-S0 / 95HR5 firmware version 00.3D.67.

## Current evidence
- Extracted payload expected at `firmware/extracted/095HR5_190AD_00.3D.67.bin` (86,340 bytes; SHA-256 `eedae0cd8d92e87ca12516c5cebfd80feb23462c424ec25eee10dc5ff40deed0`).
- Packed 24-bit candidate image expected at `firmware/packed24/095HR5_190AD_00.3D.67.packed24.bin` (64,512 bytes; SHA-256 `89f6e55f292e085907139f930356e5a28dea3ce5ffd0f4059d0fee3d5233f92f`).
- Observed payload layout: 0x144 onward is a 4-byte record containing 3 bytes of candidate instruction data followed by 0x00 padding.
- User's hardware observation: I2C address 0x58. Firmware-level proof of PMBus command support is pending.

## Important upload status
The text analysis files and scripts are being added to this repository. The binary payloads are not yet present unless the paths above are visible in the GitHub tree; do not infer binary upload success from this README.

## Reverse engineering
See `docs/DECOMPILATION_PLAN.md`, `docs/PROTOCOL_FINDINGS.md`, `protocols/PROTOCOL_STATUS.md`, and `analysis/raw/PAYLOAD_FORMAT.md`.

Do not treat standard PMBus command numbers as evidence of implemented firmware behavior until command dispatch, peripheral setup, ISR and transaction logic are verified.