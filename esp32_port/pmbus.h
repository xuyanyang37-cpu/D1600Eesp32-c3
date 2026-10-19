#pragma once
#include <Arduino.h>
#include <Wire.h>

/*
 * ESP32 Arduino read-side port inspired by jktjkt/pmbus_peek.
 * This is a small portable subset, not a line-for-line port of the Linux tool.
 * Default behavior is read-only; QUERY process-call support is opt-in.
 */
class PmbusPeek {
public:
    explicit PmbusPeek(TwoWire &bus = Wire);
    bool begin(uint8_t address = 0x58);
    bool readByte(uint8_t command, uint8_t &value);
    bool readWord(uint8_t command, uint16_t &value);
    bool readBlock(uint8_t command, uint8_t *buffer, size_t capacity, size_t &length);
    bool queryCommand(uint8_t command, uint8_t &queryFlags); // optional PMBus QUERY process call
    void printKnownReadOnly(Stream &out);

    uint8_t address() const { return _address; }
    int lastError() const { return _lastError; }

private:
    TwoWire &_wire;
    uint8_t _address;
    int _lastError;
    bool selectCommand(uint8_t command);
    bool readBytes(uint8_t *buffer, size_t count);
};
