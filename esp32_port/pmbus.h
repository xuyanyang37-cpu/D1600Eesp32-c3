#pragma once
#include <Arduino.h>
#include <Wire.h>

/*
 * PMBus 诊断模块：将 jktjkt/pmbus_peek 的诊断流程移植到 Arduino/ESP32。
 * 底层使用 Arduino TwoWire，不使用 Linux 的 i2c-dev 接口。
 *
 * 安全说明：
 * 1. begin()、scan()、showAll() 不会主动向电源写入设置。
 * 2. 写操作默认禁用；只有显式调用 setWritesEnabled(true) 才允许写入。
 * 3. PEC 默认关闭；需确认电源协议后再启用。
 * 4. 扩展命令 0xFExx/0xFFxx 的编码方式尚需目标设备协议验证。
 */
class PmbusPeek {
public:
    enum CommandType : uint8_t {
        CMD_UNKNOWN=0, CMD_RW1, CMD_RW2, CMD_RWB, CMD_RWB14,
        CMD_QUERY, CMD_COEFFICIENTS, CMD_APP_PROFILE, CMD_ENERGY,
        CMD_W0, CMD_W1, CMD_R1, CMD_R2
    };
    enum UnitType : uint8_t {
        UNIT_NONE=0, UNIT_VOLTS, UNIT_AMPERES, UNIT_MILLISECONDS,
        UNIT_DEGREES_C, UNIT_WATTS, UNIT_BITS, UNIT_STRING
    };
    struct Coefficients {
        bool valid;
        int8_t R;
        int16_t m;
        int16_t b;
    };
    struct CommandInfo {
        uint16_t command;
        const char *tag;
        CommandType type;
        UnitType units;
        uint8_t flags;
        uint8_t query;
        Coefficients writeCoefficients;
        Coefficients readCoefficients;
        bool supportedKnown;
        bool supported;
    };

    explicit PmbusPeek(TwoWire &bus = Wire);
    bool begin(uint8_t address = 0x58); // 先调用 Wire.begin(SDA_PIN, SCL_PIN) 初始化总线
    // 显式启用/禁用写操作；默认禁用，避免意外修改电源参数。
    void setWritesEnabled(bool enabled) { _writesEnabled = enabled; }
    bool writesEnabled() const { return _writesEnabled; }

    // 开启/关闭软件 PEC 校验（CRC-8，多项式 0x07）；默认关闭。
    // 只有确认目标电源要求 PEC 后才开启。
    void setPECEnabled(bool enabled) { _pecEnabled = enabled; }
    bool pecEnabled() const { return _pecEnabled; }

    // 判断是否属于 0xFExx / 0xFFxx 扩展命令。
    // 扩展命令的传输字节顺序仍需结合目标电源协议确认。
    bool isExtendedCommand(uint16_t command) const { return (command & 0xFE00U) == 0xFE00U; }
    uint8_t address() const { return _address; }
    int lastError() const { return _lastError; }
    uint8_t capability() const { return _capability; }
    uint8_t revision() const { return _revision; }
    bool queryAvailable() const { return _queryAvailable; }

    // 底层 PMBus/I2C 操作。扩展命令采用两字节前缀传输约定，使用前需验证目标协议。
    bool probe();
    bool readByte(uint16_t command, uint8_t &value);
    bool readWord(uint16_t command, uint16_t &value);
    bool readBlock(uint16_t command, uint8_t *buffer, size_t capacity, size_t &length);
    bool writeByte(uint16_t command, uint8_t value);
    bool writeWord(uint16_t command, uint16_t value);
    bool writeBlock(uint16_t command, const uint8_t *buffer, size_t length);
    bool sendByte(uint8_t command); // 发送命令但不附带数据字节，例如 CLEAR_FAULTS
    bool queryCommand(uint8_t command, uint8_t &queryFlags);
    bool readCoefficients(uint8_t command, bool forRead, Coefficients &coeff);
    bool readInventoryString(uint8_t command, char *out, size_t outSize);
    bool setPage(uint8_t page);
    bool clearFaults();
    bool issueManufacturerNoData(uint8_t index); // 厂商无数据命令；必须先显式启用写入

    // 对应 pmbus_peek 的高级诊断流程：扫描、列出命令、显示状态与遥测值。
    bool scan(bool queryAllCommands = false);
    void showDeviceInfo(Stream &out);
    void showStatus(Stream &out);
    void showValues(Stream &out);
    void showCommands(Stream &out);
    void showAll(Stream &out, bool values = true, bool commands = true);
    void printKnownReadOnly(Stream &out); // convenience alias for showValues()

    // 数据格式转换辅助函数，供测试及调用方使用。
    static double decodeLinear11(uint16_t raw);
    static double decodeLinear16(uint16_t raw, int8_t exponent);
    static double decodeDirect(int16_t raw, const Coefficients &coeff);
    static const char *unitName(UnitType unit);

private:
    TwoWire &_wire;
    uint8_t _address, _capability, _revision;
    int _lastError;
    bool _writesEnabled, _pecEnabled, _queryAvailable, _scanComplete, _queryAllRequested;
    uint8_t _pendingPecCrc;
    static uint8_t crc8Update(uint8_t crc, uint8_t data);
    bool writeCommandPrefix(uint16_t command);
    uint8_t commandByteCount(uint16_t command) const;
    CommandInfo _commands[180];
    size_t _commandCount;

    bool selectCommand(uint16_t command);
    bool readRaw(uint8_t *buffer, size_t count);
    bool writeAllowed() const;
    CommandInfo *findCommand(uint16_t command);
    const CommandInfo *findCommand(uint16_t command) const;
    void addCommand(uint16_t cmd, const char *tag, CommandType type,
                    UnitType unit = UNIT_NONE, uint8_t flags = 0);
    bool queryInternal(CommandInfo &info);
    bool readInventory(Stream &out, uint8_t cmd, const char *label);
    bool statusByte(Stream &out, uint8_t cmd, const char *label, const char *const bits[8]);
    void initializeCommandTable();
};
