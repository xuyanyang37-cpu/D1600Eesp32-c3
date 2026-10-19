#include <Arduino.h>
#include <Wire.h>
#include "pmbus.h"

// Set these pins to match your actual ESP32 wiring.
// Never connect PSU 12 V or non-3.3 V logic directly to ESP32 GPIO.
static const int I2C_SDA_PIN = 8; // CHANGE to your wiring
static const int I2C_SCL_PIN = 9; // CHANGE to your wiring
static const uint8_t PSU_I2C_ADDRESS = 0x58; // user's observed 7-bit address

PmbusPeek pmbus(Wire);

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  pmbus.begin(PSU_I2C_ADDRESS);

  // Optional software PEC (CRC-8/0x07). Keep off unless target PSU requires it.
  // pmbus.setPECEnabled(true);

  Serial.println("D1600E PMBus full-feature diagnostic port");
  Serial.println("READ ONLY by default. No writes are enabled.");
  if (!pmbus.probe()) {
    Serial.print("I2C probe failed, error=");
    Serial.println(pmbus.lastError());
    return;
  }
  pmbus.scan(false); // initial query probe only; avoids querying every command automatically
  pmbus.showAll(Serial, true, true);

  uint16_t raw = 0;
  if (pmbus.readWord(0x8B, raw)) {
    Serial.print("READ_VOUT raw=0x"); Serial.println(raw, HEX);
  } else {
    Serial.print("READ_VOUT failed, error="); Serial.println(pmbus.lastError());
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
