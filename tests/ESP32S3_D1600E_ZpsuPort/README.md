# D1600E zpsu controller port experiment

Arduino ESP32-S3 sketch to test whether the custom DPS1200-style I2C framing used by `darwinbeing/zpsu` is compatible with Dell/Delta D1600E-S0.

## Status

**Experimental / unverified.** This is a protocol test harness, not a confirmed D1600E driver.

The upstream zpsu controller uses:
- 7-bit I2C address derived from `0x58 + address_index`
- logical register mapped to wire register `reg << 1`
- request checksum `-(address<<1 + wire_reg)`
- 3-byte response: little-endian data plus two's-complement checksum
- logical registers 7, 8, 9, 0x1F, 0x20, 0x21 for measurements, CC setpoint, fan and status/mode/output bits

The D1600E repository currently records an observed address of 0x58 and a prior raw READ_VOUT observation, but firmware-level proof that the D1600E supports the zpsu custom protocol/register map is pending. Do not assume PMBus and this custom protocol are interchangeable.

## Build

1. Open `D1600E_ZpsuPort_Test.ino` in Arduino IDE.
2. Select your ESP32-S3 board.
3. Confirm SDA/SCL pin assignments for your board before connecting.
4. Set Serial Monitor to 115200 baud.
5. Type `SCAN`, then `STATUS`, then test individual logical registers with `READ 07`, `READ 08`, `READ 09`, `READ 1F`, `READ 20`, `READ 21`.

## Commands

- `HELP`, `SCAN`, `STATUS`, `READ <hex>`
- Write commands `ON`, `OFF`, `MODE CV`, `MODE CC`, `CC <amps>`, `FAN <rpm>` are included as API ports but intentionally blocked by default.

## Write safety

`ENABLE_UNVERIFIED_WRITES` is set to `0`. Do not enable it merely because a device ACKs or returns a checksum-valid frame. First verify the D1600E firmware's I2C transaction handler, register dispatch, checksum rules, access controls, register semantics, and hardware behavior. No writes occur at startup.
