# ESP32 port of pmbus_peek (read-side subset)

Inspired by [jktjkt/pmbus_peek](https://github.com/jktjkt/pmbus_peek), originally a Linux userspace diagnostic utility by David Brownell. The original uses Linux `i2c-dev`/SMBus ioctls and is not directly compilable on Arduino ESP32.

## Files
- `pmbus.h`, `pmbus.cpp`: Arduino `TwoWire` implementation for PMBus byte, word and block reads, a curated read-only command probe, and an optional QUERY process-call helper.
- `example.ino`: minimal serial probe example.

## Safety and limitations
- This is a read-side subset, not a complete port of the 2,165-line Linux utility. It does not implement Linux ioctl behavior, automatic coefficient discovery/decoding, all command types, PEC, or extended 16-bit PMBus commands.
- Default probe only reads a curated list of standard telemetry/status commands. It does not scan every command number because some command codes are write-only or may have device-specific side effects.
- `queryCommand()` sends a PMBus QUERY process-call transaction to command 0x1A. It is never called automatically; use only after confirming the target firmware supports QUERY.
- D1600E-S0 PMBus command support is not proven merely because a command is in the PMBus standard. Test against the PSU with writes disabled.
- `0x58` is the user's observed 7-bit I2C address. Change SDA/SCL GPIOs in the example to match the actual board.
- Never connect PSU output or non-3.3 V logic directly to ESP32 pins. Use proper open-drain level shifting and a suitable signal ground.
- This repository currently contains firmware-reverse-engineering notes rather than an existing PlatformIO/Arduino application; these files are a standalone port module and have not been integrated into a pre-existing `pmbus.cpp` or hardware-tested here.

## API
```cpp
PmbusPeek pmbus(Wire);
pmbus.begin(0x58);
pmbus.readByte(0x19, value);
pmbus.readWord(0x8B, rawVout);
pmbus.readBlock(0x99, buffer, sizeof(buffer), length);
pmbus.printKnownReadOnly(Serial);
```

The original project is GPL-3.0-or-later; review and preserve applicable licensing obligations when distributing a derived port.
