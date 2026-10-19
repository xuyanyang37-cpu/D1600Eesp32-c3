/*
 * Dell/Delta D1600E-S0 - zpsu controller port experiment
 * Target: ESP32-S3 / Arduino IDE
 *
 * IMPORTANT:
 * - This ports the zpsu custom DPS1200-style register framing, NOT standard PMBus.
 * - D1600E firmware protocol compatibility is NOT yet proven.
 * - All writes are disabled by default. Do not enable writes until read framing,
 *   register mapping, checksum and command semantics are confirmed from D1600E firmware.
 * - No automatic writes at boot.
 */

#include <Arduino.h>
#include <Wire.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define PSU_ADDR 0x58
#define I2C_CLOCK_HZ 100000
#define POLL_MS 3000

// Keep 0 for safe read-only testing. This does not mean D1600E supports this protocol.
#define ENABLE_UNVERIFIED_WRITES 0

namespace ZpsuPort {
static constexpr uint8_t REG_VOLTS = 7;
static constexpr uint8_t REG_AMPS = 8;
static constexpr uint8_t REG_WATTS = 9;
static constexpr uint8_t REG_CC_SETPOINT = 0x1F;
static constexpr uint8_t REG_FAN_RPM = 0x20;
static constexpr uint8_t REG_STATUS = 0x21;
static constexpr uint16_t OUTPUT_ENABLE_BIT = 1u << 15;
static constexpr uint16_t CONSTANT_CURRENT_BIT = 1u << 0;
static constexpr float CC_SCALE = 265.0f;
static constexpr float AMPS_SCALE = 64.0f;
static constexpr float VOLTS_SCALE = 256.0f;

struct Measurement {
  float volts = NAN;
  float amps = NAN;
  float watts = NAN;
};
enum class Mode { CV, CC };

static uint8_t negChecksum(uint16_t sum) {
  return static_cast<uint8_t>(-sum);
}

// zpsu read frame candidate: write [logical_reg<<1, checksum], then read 3 bytes.
bool readRegister(uint8_t reg, uint16_t &value, uint8_t &error) {
  const uint8_t wireReg = static_cast<uint8_t>(reg << 1);
  const uint8_t txChecksum = negChecksum((PSU_ADDR << 1) + wireReg);

  Wire.beginTransmission(PSU_ADDR);
  Wire.write(wireReg);
  Wire.write(txChecksum);
  error = Wire.endTransmission(false);
  if (error != 0) return false;

  const uint8_t got = Wire.requestFrom((uint8_t)PSU_ADDR, (uint8_t)3, (bool)true);
  if (got != 3 || Wire.available() < 3) {
    error = 0xFE; // software-defined: wrong response length
    while (Wire.available()) Wire.read();
    return false;
  }

  const uint8_t lo = Wire.read();
  const uint8_t hi = Wire.read();
  const uint8_t rxChecksum = Wire.read();
  const uint8_t expected = negChecksum((uint16_t)lo + hi);

  if (expected != rxChecksum) {
    error = 0xFD; // software-defined: response checksum mismatch
    Serial.printf("  checksum mismatch: RX=%02X expected=%02X\n", rxChecksum, expected);
    return false;
  }

  value = (uint16_t)lo | ((uint16_t)hi << 8);
  error = 0;
  return true;
}

// zpsu write frame candidate. Compiled only when ENABLE_UNVERIFIED_WRITES=1.
bool writeRegister(uint8_t reg, uint16_t value, uint8_t &error) {
#if ENABLE_UNVERIFIED_WRITES
  const uint8_t wireReg = static_cast<uint8_t>(reg << 1);
  const uint8_t lo = value & 0xFF;
  const uint8_t hi = (value >> 8) & 0xFF;
  const uint8_t checksum = negChecksum((PSU_ADDR << 1) + wireReg + lo + hi);

  Wire.beginTransmission(PSU_ADDR);
  Wire.write(wireReg);
  Wire.write(lo);
  Wire.write(hi);
  Wire.write(checksum);
  error = Wire.endTransmission(true);
  return error == 0;
#else
  (void)reg; (void)value;
  error = 0xFC; // software-defined: writes intentionally disabled
  return false;
#endif
}

bool readMeasurement(Measurement &m) {
  bool ok = true;
  uint16_t raw = 0;
  uint8_t e = 0;
  if (readRegister(REG_VOLTS, raw, e)) m.volts = raw / VOLTS_SCALE;
  else { ok = false; Serial.printf("READ volts failed err=0x%02X\n", e); }

  if (readRegister(REG_AMPS, raw, e)) m.amps = raw / AMPS_SCALE;
  else { ok = false; Serial.printf("READ amps failed err=0x%02X\n", e); }

  if (readRegister(REG_WATTS, raw, e)) m.watts = raw;
  else { ok = false; Serial.printf("READ watts failed err=0x%02X\n", e); }
  return ok;
}

bool getOutputEnabled(bool &enabled) {
  uint16_t raw; uint8_t e;
  if (!readRegister(REG_STATUS, raw, e)) {
    Serial.printf("STATUS read failed err=0x%02X\n", e); return false;
  }
  enabled = (raw & OUTPUT_ENABLE_BIT) != 0;
  return true;
}

bool getMode(Mode &mode) {
  uint16_t raw; uint8_t e;
  if (!readRegister(REG_STATUS, raw, e)) {
    Serial.printf("STATUS read failed err=0x%02X\n", e); return false;
  }
  mode = (raw & CONSTANT_CURRENT_BIT) ? Mode::CC : Mode::CV;
  return true;
}

bool getCurrentSetpoint(float &amps) {
  uint16_t raw; uint8_t e;
  if (!readRegister(REG_CC_SETPOINT, raw, e)) {
    Serial.printf("CC setpoint read failed err=0x%02X\n", e); return false;
  }
  amps = raw / CC_SCALE;
  return true;
}

bool getFanRaw(uint16_t &rpm) {
  uint8_t e;
  if (!readRegister(REG_FAN_RPM, rpm, e)) {
    Serial.printf("FAN read failed err=0x%02X\n", e); return false;
  }
  return true;
}

// Write-side API ported from zpsu; disabled by default for D1600E.
bool setCurrent(float amps) {
  if (!isfinite(amps) || amps < 0.0f || amps > 100.0f) {
    Serial.println("Rejected: CC value outside generic test guard 0..100 A.");
    return false;
  }
  uint8_t e;
  bool ok = writeRegister(REG_CC_SETPOINT, (uint16_t)(amps * CC_SCALE), e);
  if (!ok) Serial.printf("CC write rejected/failed err=0x%02X\n", e);
  return ok;
}

bool setFan(uint16_t rpm) {
  uint8_t e;
  bool ok = writeRegister(REG_FAN_RPM, rpm, e);
  if (!ok) Serial.printf("FAN write rejected/failed err=0x%02X\n", e);
  return ok;
}

bool setOutput(bool on) {
  uint16_t raw; uint8_t e;
  if (!readRegister(REG_STATUS, raw, e)) {
    Serial.printf("STATUS read failed err=0x%02X\n", e); return false;
  }
  const uint16_t target = on ? (raw | OUTPUT_ENABLE_BIT) : (raw & ~OUTPUT_ENABLE_BIT);
  if (!writeRegister(REG_STATUS, target, e)) {
    Serial.printf("OUTPUT write rejected/failed err=0x%02X\n", e); return false;
  }
  uint16_t verify;
  if (!readRegister(REG_STATUS, verify, e)) return false;
  const bool actual = (verify & OUTPUT_ENABLE_BIT) != 0;
  Serial.printf("Output readback=%s\n", actual ? "ON" : "OFF");
  return actual == on;
}

bool setMode(Mode mode) {
  uint16_t raw; uint8_t e;
  if (!readRegister(REG_STATUS, raw, e)) {
    Serial.printf("STATUS read failed err=0x%02X\n", e); return false;
  }
  const uint16_t target = (mode == Mode::CC)
      ? (raw | CONSTANT_CURRENT_BIT)
      : (raw & ~CONSTANT_CURRENT_BIT);
  if (!writeRegister(REG_STATUS, target, e)) {
    Serial.printf("MODE write rejected/failed err=0x%02X\n", e); return false;
  }
  uint16_t verify;
  if (!readRegister(REG_STATUS, verify, e)) return false;
  const Mode actual = (verify & CONSTANT_CURRENT_BIT) ? Mode::CC : Mode::CV;
  Serial.printf("Mode readback=%s\n", actual == Mode::CC ? "CC" : "CV");
  return actual == mode;
}
} // namespace ZpsuPort

static uint32_t lastPoll = 0;

void scanBus() {
  Serial.println("\nI2C scan:");
  int count = 0;
  for (uint8_t a = 0x08; a <= 0x77; ++a) {
    Wire.beginTransmission(a);
    uint8_t e = Wire.endTransmission(true);
    if (e == 0) {
      Serial.printf("  device 0x%02X%s\n", a, a == PSU_ADDR ? " [configured PSU]" : "");
      ++count;
    }
  }
  if (!count) Serial.println("  no ACK devices found");
}

void showStatus() {
  using namespace ZpsuPort;
  Measurement m;
  Serial.println("\n--- zpsu protocol candidate status (NOT PMBus) ---");
  bool measurementsOk = readMeasurement(m);
  Serial.printf("V=%s", isnan(m.volts) ? "N/A" : "");
  if (!isnan(m.volts)) Serial.printf("%.3f V", m.volts);
  Serial.printf(" | I=");
  if (isnan(m.amps)) Serial.print("N/A"); else Serial.printf("%.3f A", m.amps);
  Serial.printf(" | W=");
  if (isnan(m.watts)) Serial.print("N/A"); else Serial.printf("%.1f W", m.watts);
  Serial.printf(" | measurement reads %s\n", measurementsOk ? "OK" : "PARTIAL/FAILED");

  bool enabled;
  if (getOutputEnabled(enabled)) Serial.printf("Output bit interpretation: %s\n", enabled ? "ON" : "OFF");
  else Serial.println("Output state: unavailable");

  Mode mode;
  if (getMode(mode)) Serial.printf("Mode bit interpretation: %s\n", mode == Mode::CC ? "CC" : "CV");
  else Serial.println("Mode: unavailable");

  float cc;
  if (getCurrentSetpoint(cc)) Serial.printf("CC setpoint interpretation: %.3f A\n", cc);
  else Serial.println("CC setpoint: unavailable");

  uint16_t fan;
  if (getFanRaw(fan)) Serial.printf("Fan register raw: %u (not yet confirmed RPM units)\n", fan);
  else Serial.println("Fan register: unavailable");
}

void help() {
  Serial.println("\nCommands:");
  Serial.println("  HELP       - this help");
  Serial.println("  SCAN       - scan I2C addresses");
  Serial.println("  STATUS     - read zpsu-style registers 7,8,9,0x1F,0x20,0x21");
  Serial.println("  READ <hex> - read one logical register, e.g. READ 21");
  Serial.println("  ON / OFF   - write output enable bit (disabled by default)");
  Serial.println("  MODE CV/CC - write mode bit (disabled by default)");
  Serial.println("  CC <amps>  - write CC setpoint (disabled by default)");
  Serial.println("  FAN <rpm>  - write fan register (disabled by default)");
#if ENABLE_UNVERIFIED_WRITES
  Serial.println("WARNING: experimental writes compiled IN. Protocol is still unverified.");
#else
  Serial.println("WRITE COMMANDS ARE LOCKED: ENABLE_UNVERIFIED_WRITES=0.");
#endif
  Serial.println("No automatic write occurs at startup.");
}

void processCommand(String line) {
  line.trim();
  line.toUpperCase();
  if (!line.length()) return;

  if (line == "HELP") { help(); return; }
  if (line == "SCAN") { scanBus(); return; }
  if (line == "STATUS") { showStatus(); return; }

  if (line.startsWith("READ ")) {
    unsigned long n = strtoul(line.substring(5).c_str(), nullptr, 16);
    if (n > 0xFF) { Serial.println("Invalid logical register (00..FF)."); return; }
    uint16_t value; uint8_t e;
    if (ZpsuPort::readRegister((uint8_t)n, value, e))
      Serial.printf("REG 0x%02lX = 0x%04X (%u)\n", n, value, value);
    else
      Serial.printf("Read failed, err=0x%02X\n", e);
    return;
  }

  if (line == "ON" || line == "OFF") {
#if ENABLE_UNVERIFIED_WRITES
    Serial.println("WARNING: sending unverified output-control write.");
    Serial.println(ZpsuPort::setOutput(line == "ON") ? "OK" : "FAILED");
#else
    Serial.println("BLOCKED: writes disabled. Protocol/register map not proven for D1600E.");
#endif
    return;
  }

  if (line == "MODE CV" || line == "MODE CC") {
#if ENABLE_UNVERIFIED_WRITES
    Serial.println("WARNING: sending unverified CV/CC mode write.");
    Serial.println(ZpsuPort::setMode(line == "MODE CC" ? ZpsuPort::Mode::CC : ZpsuPort::Mode::CV) ? "OK" : "FAILED");
#else
    Serial.println("BLOCKED: writes disabled. Protocol/register map not proven for D1600E.");
#endif
    return;
  }

  if (line.startsWith("CC ")) {
#if ENABLE_UNVERIFIED_WRITES
    float a = line.substring(3).toFloat();
    Serial.println(ZpsuPort::setCurrent(a) ? "OK" : "FAILED");
#else
    Serial.println("BLOCKED: writes disabled. Protocol/register map not proven for D1600E.");
#endif
    return;
  }

  if (line.startsWith("FAN ")) {
#if ENABLE_UNVERIFIED_WRITES
    long rpm = line.substring(4).toInt();
    if (rpm < 0 || rpm > 65535) { Serial.println("Invalid RPM."); return; }
    Serial.println(ZpsuPort::setFan((uint16_t)rpm) ? "OK" : "FAILED");
#else
    Serial.println("BLOCKED: writes disabled. Protocol/register map not proven for D1600E.");
#endif
    return;
  }

  Serial.println("Unknown command. Type HELP.");
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\nD1600E zpsu feature-port experiment");
  Serial.println("WARNING: this is an unverified protocol hypothesis, not a confirmed D1600E driver.");
  Serial.printf("I2C SDA=%d SCL=%d address=0x%02X clock=%d Hz\n",
                I2C_SDA_PIN, I2C_SCL_PIN, PSU_ADDR, I2C_CLOCK_HZ);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setTimeOut(100);
  scanBus();
  help();
  Serial.println("\nType STATUS to begin read-only probing.");
  lastPoll = millis();
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    processCommand(line);
  }
  // Deliberately no automatic register polling to keep logs manageable.
  delay(5);
}
