// MAX30102 pulse sensor (heart rate) on A0/A1 via software I2C
//
// Wiring: VIN -> 5V, GND -> GND, SDA/SCL -> A0/A1 (either order: detected at startup by reading PART_ID)
// The lines are open-drain (driven LOW or released with the internal pull-up), so 5 V is never driven into the module.
//
// Measurement: IR at 100 samples/s (400 Hz, 4-sample average). After removing DC, a sharp drop in IR
// (= systolic blood inflow) is detected from its slope (robust to breathing-induced baseline drift).
// Returns the median of the last 5 beats as BPM.
#pragma once
#include <Arduino.h>

namespace pulse {

const uint8_t ADDR = 0x57;
const uint8_t PART_ID = 0x15;
const long FINGER_IR = 50000;      // IR at or above this means a finger is present
const float MIN_SLOPE = 2.0;       // Ignore slope peaks below this [counts/sample] (noise)
const uint8_t RATE_N = 5;          // Number of beats to take the median over
const unsigned long POLL_MS = 10;

uint8_t sdaBit = 0, sclBit = 0;    // PORTC bit masks (A0 = PC0, A1 = PC1)
bool found = false;
bool finger = false;
bool rawStream = false;  // true: send every sample to serial as "R <ir>" (toggle with the 'r' command)

// ---- Software I2C (open drain) ----
inline void sdaLow()  { PORTC &= ~sdaBit; DDRC |= sdaBit; }
inline void sdaHigh() { DDRC &= ~sdaBit; PORTC |= sdaBit; }
inline void sclLow()  { PORTC &= ~sclBit; DDRC |= sclBit; }
inline void sclHigh() { DDRC &= ~sclBit; PORTC |= sclBit; }
inline bool sdaRead() { return PINC & sdaBit; }
inline void hold() { delayMicroseconds(4); }

void i2cStart() {  // Also serves as repeated start
  sdaHigh(); hold();
  sclHigh(); hold();
  sdaLow();  hold();
  sclLow();  hold();
}

void i2cStop() {
  sdaLow();  hold();
  sclHigh(); hold();
  sdaHigh(); hold();
}

bool i2cWrite(uint8_t b) {
  for (uint8_t i = 0; i < 8; i++) {
    if (b & 0x80) sdaHigh(); else sdaLow();
    b <<= 1;
    hold(); sclHigh(); hold(); sclLow();
  }
  sdaHigh(); hold();
  sclHigh(); hold();
  bool ack = !sdaRead();
  sclLow(); hold();
  return ack;
}

uint8_t i2cRead(bool ack) {
  uint8_t b = 0;
  sdaHigh();
  for (uint8_t i = 0; i < 8; i++) {
    hold(); sclHigh(); hold();
    b = (b << 1) | (sdaRead() ? 1 : 0);
    sclLow();
  }
  if (ack) sdaLow(); else sdaHigh();
  hold(); sclHigh(); hold(); sclLow();
  sdaHigh();
  return b;
}

void i2cRecover() {  // Clock out any transfer left mid-byte
  sdaHigh();
  for (uint8_t i = 0; i < 9; i++) { sclLow(); hold(); sclHigh(); hold(); }
  sclLow(); hold();  // Bring SCL LOW before the STOP (SDA falling while SCL is HIGH would be a START, and START+STOP confuses the sensor)
  i2cStop();
}

bool writeReg(uint8_t reg, uint8_t val) {
  i2cStart();
  bool ok = i2cWrite(ADDR << 1) && i2cWrite(reg) && i2cWrite(val);
  i2cStop();
  return ok;
}

bool readRegs(uint8_t reg, uint8_t *buf, uint8_t n) {
  i2cStart();
  bool ok = i2cWrite(ADDR << 1) && i2cWrite(reg);
  if (ok) {
    i2cStart();
    ok = i2cWrite((ADDR << 1) | 1);
  }
  if (ok) {
    for (uint8_t i = 0; i < n; i++) buf[i] = i2cRead(i + 1 < n);
  }
  i2cStop();
  return ok;
}

// ---- Beat detection ----
uint32_t sampleCount = 0, lastBeat = 0;
float dc = 0, lp = 0, prev = 0, amp = 0;
uint8_t settle = 0;
float rates[RATE_N];
uint8_t nRates = 0, rateIdx = 0, rejects = 0;
unsigned long lastPoll = 0;

float average() {  // Median (robust to double-counted or missed beats)
  float v[RATE_N];
  for (uint8_t i = 0; i < nRates; i++) {
    float r = rates[i];
    int8_t j = i - 1;
    for (; j >= 0 && v[j] > r; j--) v[j + 1] = v[j];
    v[j + 1] = r;
  }
  return (nRates & 1) ? v[nRates / 2] : (v[nRates / 2 - 1] + v[nRates / 2]) / 2;
}

// Process one sample. Returns the average BPM when a new beat is detected, otherwise 0
float process(long ir) {
  sampleCount++;
  if (ir < FINGER_IR) {
    finger = false;
    return 0;
  }
  if (!finger) {  // Finger just placed: reset state and wait 1 s for things to settle
    finger = true;
    dc = ir; lp = prev = amp = 0;
    settle = 100;
    lastBeat = 0;
    nRates = rateIdx = rejects = 0;
  }

  dc += (ir - dc) * 0.0125;      // DC (time constant ~0.8 s)
  float lpOld = lp;
  lp += (-(ir - dc) - lp) * 0.1;  // Invert (blood inflow -> positive), then ~1.7 Hz LPF (avoids double-counting the dicrotic notch)
  float d = lp - lpOld;           // Slope (largest during systolic inflow)
  if (settle) {
    settle--;
    prev = d;
    return 0;
  }

  amp = max(amp * 0.995, d);  // Peak envelope of the slope (decays over ~2 s)
  float th = 0.5 * amp;
  uint32_t since = sampleCount - lastBeat;
  uint32_t refractory = nRates ? (uint32_t)(0.75 * 6000.0 / average()) : 30;  // 75% of the previous interval, or 300 ms
  bool beat = prev < th && d >= th && amp > MIN_SLOPE && (lastBeat == 0 || since > refractory);
  prev = d;
  if (!beat) return 0;

  float result = 0;
  if (lastBeat) {
    float r = 6000.0 / since;  // 100 samples/s
    bool outlier = nRates >= 2 && fabs(r - average()) > 0.25 * average();
    if (r >= 40 && r <= 180 && (!outlier || rejects >= 3)) {
      if (outlier) nRates = rateIdx = 0;  // Rejected 3 times in a row: the rate really changed, so start over
      rates[rateIdx] = r;
      rateIdx = (rateIdx + 1) % RATE_N;
      if (nRates < RATE_N) nRates++;
      rejects = 0;
      result = average();
    } else {
      rejects++;
    }
  }
  lastBeat = sampleCount;
  return result;
}

// ---- Public functions ----
bool lastAck = false;
uint8_t lastId = 0;

bool probe(uint8_t sda, uint8_t scl) {
  sdaBit = sda;
  sclBit = scl;
  sdaHigh();
  sclHigh();
  delay(2);
  for (uint8_t i = 0; i < 3; i++) {  // Retry: the probe in the other orientation can leave the sensor confused
    i2cRecover();
    lastId = 0;
    lastAck = readRegs(0xFF, &lastId, 1);
    if (lastAck && lastId == PART_ID) return true;
  }
  return false;
}

// Detect and initialize the sensor. Returns false if it is not found (the system keeps running without it)
bool begin() {
  found = probe(_BV(PC0), _BV(PC1)) || probe(_BV(PC1), _BV(PC0));
  if (!found) {
    sdaHigh();
    sclHigh();
    return false;
  }
  writeReg(0x09, 0x40);  // Reset
  for (uint8_t i = 0; i < 100; i++) {
    uint8_t m = 0x40;
    delay(1);
    if (readRegs(0x09, &m, 1) && !(m & 0x40)) break;
  }
  writeReg(0x02, 0x00);  // Interrupts off
  writeReg(0x03, 0x00);
  writeReg(0x08, 0x50);  // FIFO: 4-sample average, rollover enabled
  writeReg(0x0A, 0x2F);  // ADC 4096 nA, 400 Hz, 411 us (18 bit)
  writeReg(0x0C, 0x1F);  // Red LED ~6.4 mA
  writeReg(0x0D, 0x1F);  // IR LED ~6.4 mA
  writeReg(0x04, 0x00);  // Clear the FIFO
  writeReg(0x05, 0x00);
  writeReg(0x06, 0x00);
  writeReg(0x09, 0x03);  // SpO2 mode (red + IR)
  return true;
}

// Print the A0/A1 voltages for diagnostics when the sensor is not found (no pull-up = the module's own pull-up voltage)
void diag(Print &out) {
  for (uint8_t k = 0; k < 2; k++) {
    probe(k ? _BV(PC1) : _BV(PC0), k ? _BV(PC0) : _BV(PC1));
    out.print(k ? "# pulse diag: SDA=A1,SCL=A0 ack=" : "# pulse diag: SDA=A0,SCL=A1 ack=");
    out.print(lastAck);
    out.print(" id=0x");
    out.print(lastId, HEX);
    out.print(" scan:");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
      i2cStart();
      bool ack = i2cWrite(addr << 1);
      i2cStop();
      if (ack) {
        out.print(" 0x");
        out.print(addr, HEX);
      }
    }
    out.println();
  }
  const uint8_t pin[2] = {A0, A1};
  for (uint8_t i = 0; i < 2; i++) {
    pinMode(pin[i], INPUT);
    delay(2);
    float vFloat = analogRead(pin[i]) * 5.0 / 1023;
    pinMode(pin[i], INPUT_PULLUP);
    delay(2);
    float vPull = analogRead(pin[i]) * 5.0 / 1023;
    out.print("# pulse diag: A");
    out.print(i);
    out.print(" no-pullup ");
    out.print(vFloat, 2);
    out.print(" V, internal-pullup ");
    out.print(vPull, 2);
    out.println(" V");
  }
}

const char *pins() { return sdaBit == _BV(PC0) ? "SDA=A0, SCL=A1" : "SDA=A1, SCL=A0"; }

// Call from loop(). Returns the average BPM when a new beat is detected, otherwise 0. Blocks for at most about 2 ms
float update() {
  unsigned long now = millis();
  if (!found || now - lastPoll < POLL_MS) return 0;
  lastPoll = now;

  uint8_t ptr[3];  // FIFO_WR_PTR, OVF_COUNTER, FIFO_RD_PTR
  if (!readRegs(0x04, ptr, 3)) return 0;
  uint8_t n = (ptr[0] - ptr[2]) & 0x1F;
  if (n == 0 && ptr[1]) n = 32;
  if (n > 4) n = 4;  // Read the rest next time

  float result = 0;
  for (uint8_t i = 0; i < n; i++) {
    uint8_t d[6];
    if (!readRegs(0x07, d, 6)) break;
    long ir = (((long)d[3] << 16) | ((long)d[4] << 8) | d[5]) & 0x3FFFF;
    if (rawStream) {
      Serial.print("R ");
      Serial.println(ir);
    }
    float r = process(ir);
    if (r > 0) result = r;
  }
  return result;
}

}  // namespace pulse
