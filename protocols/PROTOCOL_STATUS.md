# Protocol status — D1600E / 095HR5 A05

Last reviewed: 2026-10-11

## Confirmed from current notes

- Target: Dell/Delta D1600E-S0, 1600 W, part family 95HR5 / 095HR5A, firmware label 00.3D.67.
- Observed I²C/PMBus address in prior bench notes: 0x58. Treat this as a bench observation until corroborated from firmware.
- Observed READ_VOUT raw word: command 0x8B returned 0x18DF while output was approximately 12.06 V.
- VOUT_MODE (0x20) read as 0xFF and VOUT_COMMAND (0x21) / VOUT_MAX (0x24) read as 0xFFFF in the notes. These values do not establish standard PMBus support or a valid decoding mode.
- Historical tests reported E1 and E3 readback following writes while output voltage stayed broadly unchanged. This does not prove either command controls output voltage.

## Not yet confirmed

- Exact command dispatcher in the A05 payload.
- Whether standard VOUT_COMMAND, OCP, OVP, UVP or vendor commands are implemented.
- Any unlock sequence or verified voltage-control frame.
- RAM variable addresses, flash/configuration offsets, ADC/PWM register mappings.
- Mapping from payload file offsets to dsPIC program addresses and symbols.

## Safety / analysis rule

Do not repeat the historical writes. Do not write candidate values to the live PSU. First resolve the firmware instruction encoding and trace the command dispatcher and its read/write handlers in an offline copy. Mark a mapping Verified only when supported by instruction-level evidence and independent corroboration.
