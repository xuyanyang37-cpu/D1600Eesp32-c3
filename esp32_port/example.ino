#include <Arduino.h>
#include <Wire.h>
#include "pmbus.h"

// 请按实际接线修改 ESP32 的 SDA/SCL 引脚。
// 严禁将电源 12V 或非 3.3V 逻辑直接接到 ESP32 GPIO。
static const int I2C_SDA_PIN = 8; // 按实际接线修改
static const int I2C_SCL_PIN = 9; // 按实际接线修改
static const uint8_t PSU_I2C_ADDRESS = 0x58; // 已观测到的 7 位地址，接线后仍需确认

PmbusPeek pmbus(Wire);

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  pmbus.begin(PSU_I2C_ADDRESS);

  // 可选：启用软件 PEC（CRC-8/0x07）。只有确认电源要求 PEC 后才取消下一行注释。
  // pmbus.setPECEnabled(true);

  Serial.println("D1600E PMBus 诊断程序");
  Serial.println("默认只读，所有写操作均已锁定。");
  if (!pmbus.probe()) {
    Serial.print("I2C 探测失败，错误码=");
    Serial.println(pmbus.lastError());
    return;
  }
  pmbus.scan(false); // 仅进行基础探测，不自动对全部命令执行 QUERY
  pmbus.showAll(Serial, true, true);

  uint16_t raw = 0;
  if (pmbus.readWord(0x8B, raw)) {
    Serial.print("READ_VOUT 原始值=0x"); Serial.println(raw, HEX);
  } else {
    Serial.print("READ_VOUT 读取失败，错误码="); Serial.println(pmbus.lastError());
  }

  // Extended command API is available, but byte order is device-specific.
  // After confirming the PSU protocol, examples are:
  // uint8_t extByte = 0;
  // uint16_t extWord = 0;
  // pmbus.readByte(0xFE00, extByte); // transmits bytes FE 00, then reads one byte
  // pmbus.readWord(0xFF12, extWord); // transmits bytes FF 12, then reads a word

  // Optional deeper discovery: queries every command descriptor.
  // This can trigger PSU error/alert handling if unsupported. Call only deliberately:
  // pmbus.scan(true); pmbus.showCommands(Serial);

  // Explicit write use requires this opt-in AND an explicit write call.
  // Verify target support and electrical setup before enabling:
  // pmbus.setWritesEnabled(true);
  // pmbus.setPage(0);       // writes PAGE
  // pmbus.clearFaults();    // sends CLEAR_FAULTS
}
void loop() { delay(1000); }
