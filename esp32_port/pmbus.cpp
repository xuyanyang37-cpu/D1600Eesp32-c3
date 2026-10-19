#include "pmbus.h"

PmbusPeek::PmbusPeek(TwoWire &bus)
    : _wire(bus), _address(0x58), _lastError(0) {}

bool PmbusPeek::begin(uint8_t address) {
    _address = address & 0x7F; // Arduino Wire uses a 7-bit address
    _lastError = 0;
    _wire.begin();
    return true;
}

bool PmbusPeek::selectCommand(uint8_t command) {
    _wire.beginTransmission(_address);
    if (_wire.write(command) != 1) {
        _lastError = -1001;
        return false;
    }
    uint8_t rc = _wire.endTransmission(false); // repeated-start before read
    if (rc != 0) {
        _lastError = rc;
        return false;
    }
    return true;
}

bool PmbusPeek::readBytes(uint8_t *buffer, size_t count) {
    if (buffer == nullptr || count == 0 || count > 32) {
        _lastError = -1002;
        return false;
    }
    size_t received = _wire.requestFrom((uint8_t)_address, count, (uint8_t)true);
    if (received != count) {
        while (_wire.available()) (void)_wire.read();
        _lastError = -1003;
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!_wire.available()) {
            _lastError = -1004;
            return false;
        }
        buffer[i] = (uint8_t)_wire.read();
    }
    _lastError = 0;
    return true;
}

bool PmbusPeek::readByte(uint8_t command, uint8_t &value) {
    if (!selectCommand(command)) return false;
    return readBytes(&value, 1);
}

bool PmbusPeek::readWord(uint8_t command, uint16_t &value) {
    uint8_t bytes[2];
    if (!selectCommand(command) || !readBytes(bytes, sizeof(bytes))) return false;
    // SMBus/PMBus word data is transmitted low byte first.
    value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    return true;
}

bool PmbusPeek::readBlock(uint8_t command, uint8_t *buffer, size_t capacity, size_t &length) {
    length = 0;
    if (buffer == nullptr || capacity == 0 || capacity > 32) {
        _lastError = -1005;
        return false;
    }
    if (!selectCommand(command)) return false;

    // PMBus block read returns count byte followed by payload.
    // Read the protocol maximum (count + 32 data bytes), then validate count.
    uint8_t raw[33];
    size_t received = _wire.requestFrom((uint8_t)_address, (size_t)sizeof(raw), (bool)true);
    if (received < 1 || !_wire.available()) {
        _lastError = -1006;
        while (_wire.available()) (void)_wire.read();
        return false;
    }
    size_t pos = 0;
    while (_wire.available() && pos < sizeof(raw)) raw[pos++] = (uint8_t)_wire.read();
    if (pos < 1) {
        _lastError = -1007;
        return false;
    }
    size_t announced = raw[0];
    if (announced > 32 || pos < announced + 1 || announced > capacity) {
        _lastError = -1008;
        return false;
    }
    for (size_t i = 0; i < announced; ++i) buffer[i] = raw[i + 1];
    length = announced;
    _lastError = 0;
    return true;
}

bool PmbusPeek::queryCommand(uint8_t command, uint8_t &queryFlags) {
    // PMBus QUERY is an SMBus Process Call: command 0x1A, word 0x01xx.
    // This sends a transaction to the PSU. It is opt-in and is NOT called by default.
    _wire.beginTransmission(_address);
    _wire.write((uint8_t)0x1A);
    _wire.write((uint8_t)0x01); // low byte of process-call word
    _wire.write(command);       // high byte of process-call word
    uint8_t rc = _wire.endTransmission(false);
    if (rc != 0) {
        _lastError = rc;
        return false;
    }
    uint8_t response[2];
    if (!readBytes(response, sizeof(response))) return false;
    if (response[0] != 0x01) {
        _lastError = -1009; // unexpected QUERY process-call echo
        return false;
    }
    queryFlags = response[1];
    _lastError = 0;
    return true;
}

void PmbusPeek::printKnownReadOnly(Stream &out) {
    struct WordCmd { uint8_t code; const char *name; };
    struct ByteCmd { uint8_t code; const char *name; };
    static const ByteCmd bytes[] = {
        {0x19, "CAPABILITY"}, {0x20, "VOUT_MODE"},
        {0x78, "STATUS_BYTE"}, {0x7A, "STATUS_VOUT"},
        {0x7B, "STATUS_IOUT"}, {0x7C, "STATUS_INPUT"},
        {0x7D, "STATUS_TEMPERATURE"}, {0x7E, "STATUS_CML"},
        {0x7F, "STATUS_OTHER"}, {0x80, "STATUS_MFR_SPECIFIC"},
        {0x81, "STATUS_FANS_1_2"}, {0x82, "STATUS_FANS_3_4"}
    };
    static const WordCmd words[] = {
        {0x79, "STATUS_WORD"}, {0x8B, "READ_VOUT"}, {0x8C, "READ_IOUT"},
        {0x8D, "READ_TEMPERATURE_1"}, {0x8E, "READ_TEMPERATURE_2"},
        {0x8F, "READ_TEMPERATURE_3"}, {0x90, "READ_FAN_SPEED_1"},
        {0x91, "READ_FAN_SPEED_2"}, {0x92, "READ_FAN_SPEED_3"},
        {0x93, "READ_FAN_SPEED_4"}, {0x94, "READ_DUTY_CYCLE"},
        {0x95, "READ_FREQUENCY"}, {0x96, "READ_POUT"}, {0x97, "READ_PIN"}
    };

    out.println(F("PMBus read-only known-command probe"));
    out.print(F("7-bit I2C address: 0x"));
    if (_address < 16) out.print('0');
    out.println(_address, HEX);

    for (size_t i = 0; i < sizeof(bytes) / sizeof(bytes[0]); ++i) {
        uint8_t v = 0;
        out.print(bytes[i].name); out.print(F(" [0x"));
        out.print(bytes[i].code, HEX); out.print(F("]: "));
        if (readByte(bytes[i].code, v)) {
            out.print(F("0x")); if (v < 16) out.print('0'); out.println(v, HEX);
        } else {
            out.print(F("read failed, Wire error=")); out.println(_lastError);
        }
        delay(3);
    }
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
        uint16_t v = 0;
        out.print(words[i].name); out.print(F(" [0x"));
        out.print(words[i].code, HEX); out.print(F("]: "));
        if (readWord(words[i].code, v)) {
            out.print(F("0x")); if (v < 0x1000) out.print('0'); if (v < 0x100) out.print('0'); if (v < 0x10) out.print('0');
            out.println(v, HEX);
        } else {
            out.print(F("read failed, Wire error=")); out.println(_lastError);
        }
        delay(3);
    }
}
