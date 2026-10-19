# ESP32 port of jktjkt/pmbus_peek

Source reference: https://github.com/jktjkt/pmbus_peek (David Brownell / Jan Kundrat; GPL-3.0-or-later). This is a functional Arduino/ESP32 reimplementation of the diagnostic workflow, not a literal source-level conversion of Linux ioctl code.

## Implemented feature areas
- Byte, word, block reads; byte, word, block writes; no-data SEND BYTE.
- PMBus QUERY process-call and COEFFICIENTS process-call attempts.
- Capability / PMBus revision reads; manufacturer inventory strings; application-profile block.
- Command catalogue for common standard PMBus commands, telemetry, status, manufacturer commands and user-data ranges.
- Status-word/status-byte and status subregister reporting.
- Telemetry values, LINEAR11 / VOUT_MODE LINEAR16 / DIRECT decoding helpers, coefficient display.
- PAGE selection, CLEAR_FAULTS, manufacturer no-data command.
- Scan, inventory, status, values and command-list display methods.
- Explicit write gate: writes disabled by default and all write methods fail until caller invokes setWritesEnabled(true).

## Important differences / limits
- Linux CLI switches, /dev/i2c adapter discovery, ioctl capability flags, kernel PEC negotiation and Linux errno behavior are replaced by class methods and Arduino Wire.
- Standard I2C extended command encodings 0xFE00/0xFFxx are catalogued but not transported; PMBus does not define a universally portable encoding for all extended commands.
- Arduino Wire buffer limits mean block APIs currently cap payload at 32 bytes, unlike the Linux original's 255-byte block path.
- PEC calculation/transport is not implemented. Arduino Wire does not automatically enable SMBus PEC.
- The source's command metadata is represented in a compact table, and does not reproduce every duplicate/quirky descriptor entry byte-for-byte.
- QUERY and COEFFICIENTS use process-call wire sequences. Actual support by Dell/Delta D1600E-S0 firmware is unconfirmed; do not assume PMBus-standard command presence proves the PSU implements it.
- The example performs a basic I2C probe and reads/prints status, inventory, values and command listing. Use the deeper scan intentionally: it may issue many QUERY transactions and unsupported commands can produce PSU communication alerts.
- Default address is the user's observed 7-bit 0x58. Change SDA/SCL pins to actual hardware. Use proper level shifting/open-drain pull-ups and common signal ground.
- Not hardware-tested here; compile against the exact ESP32 Arduino core and board before connecting to the PSU.

## Basic use
```cpp
Wire.begin(SDA_PIN, SCL_PIN);
PmbusPeek pmbus(Wire);
pmbus.begin(0x58);
pmbus.scan(false);
pmbus.showAll(Serial, true, true);

// Explicit writes are locked until deliberately enabled:
pmbus.setWritesEnabled(true);
pmbus.writeWord(0x21, rawValue);
```
