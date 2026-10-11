# Payload format validation

- Payload size: 86340 bytes
- Payload SHA-256: eedae0cd8d92e87ca12516c5cebfd80feb23462c424ec25eee10dc5ff40deed0
- Firmware record region: file offset 0x144 through 0x15143.
- Record width: 4 bytes.
- Record layout observed across the complete region: 3-byte word + 0x00 padding.
- Packed 24-bit candidate image: 64512 bytes.
- Number of 4-byte records: 21504.
- Long 0xFFFFFF padding runs are present near the end, consistent with an MCU memory image containing unused words.

This supersedes the earlier 0x104 hypothesis. The earlier offset was incorrect.

## Decoder status

Ghidra/dsPIC decoder validation is still pending. Do not invent instruction mnemonics or pseudocode. Validate endianness, reset/interrupt vectors, and program-address units before function recovery.