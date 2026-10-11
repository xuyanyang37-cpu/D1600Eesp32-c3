# Protocol status

No runtime protocol is declared identified yet. Targets:

- PMBus/SMBus over I2C — user hardware observation: address 0x58; firmware proof pending.
- Vendor-private I2C — command table/state machine pending.
- Firmware update/bootloader transport — pending.
- ICSP programming interface — MCU programming/debug interface, not a runtime PSU protocol.

Evidence required before marking a protocol as identified: peripheral initialization, ISR, RX/TX state machine, command dispatch, and checksum/PEC implementation.