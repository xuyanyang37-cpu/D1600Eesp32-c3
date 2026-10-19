#include <Arduino.h>
#include <Wire.h>
#include "pmbus.h"

// Adjust to your ESP32 board wiring before use.
// Do not connect a 12 V PSU signal directly to ESP32 GPIO/I2C pins.
// Use a suitable level shifter/open-drain interface and common signal ground.
static const int I2C_SDA_PIN = 8; // CHANGE to your actual wiring
static const int I2C_SCL_PIN = 9; // CHANGE to your actual wiring
static const uint8_t PSU_I2C_ADDRESS = 0x58; // user's observed 7-bit address

PmbusPeek pmbus(Wire);

void setup() {
    Serial.begin(115200);
    delay(500);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000);

    pmbus.begin(PSU_I2C_ADDRESS);
    Serial.println();
    Serial.println("D1600E PMBus read-only probe");
    pmbus.printKnownReadOnly(Serial);

    uint16_t raw = 0;
    if (pmbus.readWord(0x8B, raw)) {
        Serial.print("READ_VOUT raw=0x");
        Serial.println(raw, HEX);
    } else {
        Serial.print("READ_VOUT failed, error=");
        Serial.println(pmbus.lastError());
    }
}

void loop() {
    delay(1000);
}
