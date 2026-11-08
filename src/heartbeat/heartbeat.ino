#include "pulse_sensor.h"  // MAX30102 heart-rate sensor on A0/A1

// CONSTANTS
// AIR PUMP (MOSFET)
const int AIR_PUMP = 2;

// WATER (KOOLANCE PMP-420 202572, motor driver channel A)
const int PWMA = 3;
const int AIN1 = 8;
const int AIN2 = 9;
const int STBY = 10;
const int WATER_PUMP_SPEED = 255;

// AIR VALVES (parallel: pump -> VALVE1 -> atrium, pump -> VALVE2 -> ventricle)
// The air pump runs whenever either valve is open and stops only when both are closed
const int VALVE1 = 6;  // Atrium
const int VALVE2 = 4;  // Ventricle

// AIR VALVES DEFAULT SETTING
const int VALVE_OPEN = HIGH;
const int VALVE_CLOSE = LOW;

// SETTINGS
const float BPM_MIN = 40.0;
const float BPM_MAX = 120.0;

// VARIABLES
float bpm = 80.0;
const float ATRIUM_RATIO = 0.25;     // Fraction of the period spent on atrial expansion
const float AV_GAP_RATIO = 0.15;     // Pause between atrium and ventricle (both closed: atrium deflates first)
const float VENTRICLE_RATIO = 0.40;  // Fraction of the period spent on ventricular expansion (the rest is rest)
const unsigned long ATRIUM_FILL_MAX_MS = 250;  // Max time air may flow into the atrium (atrium bursts at 300 ms or more)
const float WATER_ON_RATIO = 0.3;  // Fraction of the period the water pump is ON

enum Phase { PHASE_STOP, PHASE_ATRIUM, PHASE_AV_GAP, PHASE_VENTRICLE, PHASE_RELAX };
Phase phase = PHASE_STOP;
bool running = true;
bool manualStop = false;  // Stopped with the 's' command (the finger does not restart it until 'g')

// Rest while no finger is on the pulse sensor (only when the sensor is found)
const unsigned long FINGER_ON_MS = 500;    // Start beating once the finger has been on for this long
const unsigned long FINGER_OFF_MS = 2000;  // Rest once the finger has been off for this long

unsigned long cycleStart = 0;
unsigned long periodMs = 0;
unsigned long atriumMs = 0;
unsigned long ventricleStartMs = 0;
unsigned long ventricleEndMs = 0;
unsigned long waterOnMs = 0;

bool airOn = false;
bool valve1Open = false;
bool valve2Open = false;
unsigned long atriumFillStart = 0;
bool waterOn = true;

char cmdBuf[16];
int cmdLen = 0;

void setup() {
  pinMode(VALVE1, OUTPUT);
  pinMode(VALVE2, OUTPUT);
  pinMode(AIR_PUMP, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(STBY, OUTPUT);

  setAir(false, false);
  setWater(false);

  Serial.begin(115200);
  Serial.print("# heartbeat start, bpm ");
  Serial.println(bpm);

  if (pulse::begin()) {
    Serial.print("# pulse sensor found (");
    Serial.print(pulse::pins());
    Serial.println(")");
    running = false;  // Rest until a finger is placed
    Serial.println("# rest (waiting for finger)");
  } else {
    Serial.println("# pulse sensor not found: BPM is set over serial only");
    pulse::diag(Serial);
  }
}

void loop() {
  readSerial();
  updatePulse();

  unsigned long now = millis();
  if (running) {
    updatePhase(now);
    updateWater(now);
  }
  safetyCheck(millis());  // Re-read the time: atriumFillStart is set with millis() inside setAir(), so an older now would underflow
}

// Sync the BPM to the heart rate measured by the sensor (applied from the next beat on).
// While no finger is on the sensor, keep the last value (or the serial-set value)
void updatePulse() {
  float hr = pulse::update();
  if (hr > 0) {
    bpm = constrain(hr, BPM_MIN, BPM_MAX);
    Serial.print("# pulse ");
    Serial.print(hr);
    Serial.print(" -> bpm ");
    Serial.println(bpm);
  }

  // Finger present -> beat, finger absent -> rest (with debouncing)
  if (!pulse::found) return;
  static bool lastFinger = false;
  static unsigned long changedAt = 0;
  unsigned long now = millis();
  if (pulse::finger != lastFinger) {
    lastFinger = pulse::finger;
    changedAt = now;
  }
  if (pulse::finger && !running && !manualStop && now - changedAt >= FINGER_ON_MS) {
    running = true;
    Serial.println("# finger on: beat");
  } else if (!pulse::finger && running && now - changedAt >= FINGER_OFF_MS) {
    stopAll();
    Serial.println("# finger off: rest");
  }
}

void startCycle(unsigned long now) {
  cycleStart = now;
  periodMs = 60000.0 / bpm;
  atriumMs = min((unsigned long)(periodMs * ATRIUM_RATIO), ATRIUM_FILL_MAX_MS);
  ventricleStartMs = atriumMs + periodMs * AV_GAP_RATIO;
  ventricleEndMs = ventricleStartMs + periodMs * VENTRICLE_RATIO;
  waterOnMs = periodMs * WATER_ON_RATIO;

  phase = PHASE_ATRIUM;
  setAir(true, false);  // Atrial expansion (P wave): air into the atrium
}

void updatePhase(unsigned long now) {
  unsigned long t = now - cycleStart;

  switch (phase) {
    case PHASE_STOP:
      startCycle(now);
      break;

    case PHASE_ATRIUM:
      if (t >= atriumMs) {
        phase = PHASE_AV_GAP;
        setAir(false, false);  // A-V pause: both valves closed, air pump off
      }
      break;

    case PHASE_AV_GAP:
      if (t >= ventricleStartMs) {
        phase = PHASE_VENTRICLE;
        setAir(false, true);  // Ventricular expansion (QRS): air into the ventricle
        Serial.print("BEAT ");
        Serial.println(bpm);
      }
      break;

    case PHASE_VENTRICLE:
      if (t >= ventricleEndMs) {
        phase = PHASE_RELAX;
        setAir(false, false);  // Rest: both valves closed, air pump off
      }
      break;

    case PHASE_RELAX:
      if (t >= periodMs) {
        startCycle(now);
      }
      break;
  }
}

void updateWater(unsigned long now) {
  unsigned long t = now - cycleStart;
  setWater(phase != PHASE_ATRIUM && phase != PHASE_AV_GAP && t - ventricleStartMs < waterOnMs);
}

void stopAll() {
  running = false;
  phase = PHASE_STOP;
  setWater(false);
  setAir(false, false);
}

// Open/close the valves; the air pump runs whenever either valve is open
void setAir(bool v1Open, bool v2Open) {
  bool pump = v1Open || v2Open;
  bool wasFilling = valve1Open;
  if (v1Open && !wasFilling) {
    atriumFillStart = millis();
  }

  digitalWrite(VALVE1, v1Open ? VALVE_OPEN : VALVE_CLOSE);
  digitalWrite(VALVE2, v2Open ? VALVE_OPEN : VALVE_CLOSE);

  if (pump != airOn) {
    digitalWrite(AIR_PUMP, pump ? HIGH : LOW);
  }

  airOn = pump;
  valve1Open = v1Open;
  valve2Open = v2Open;
}

void setWater(bool on) {
  if (on != waterOn) {
    digitalWrite(STBY, on ? HIGH : LOW);
    digitalWrite(AIN1, on ? HIGH : LOW);
    digitalWrite(AIN2, LOW);
    analogWrite(PWMA, on ? WATER_PUMP_SPEED : 0);
    waterOn = on;
  }
}

void safetyCheck(unsigned long now) {
  if (valve1Open && now - atriumFillStart >= ATRIUM_FILL_MAX_MS) {
    setAir(false, valve2Open);
    Serial.println("# SAFETY: atrium valve open too long, closed");
  }
}

void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        handleCommand(cmdBuf);
        cmdLen = 0;
      }
    } else if (cmdLen < (int)sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = c;
    }
  }
}

void handleCommand(const char *s) {
  if (s[0] == 'r') {
    pulse::rawStream = !pulse::rawStream;
    return;
  }
  if (s[0] == 's') {
    manualStop = true;
    stopAll();
    Serial.println("# stop");
    return;
  }
  if (s[0] == 'g') {
    manualStop = false;
    running = true;
    Serial.println("# go");
    return;
  }

  float v = atof(s);
  if (v > 0) {
    bpm = constrain(v, BPM_MIN, BPM_MAX);
    Serial.print("# bpm ");
    Serial.println(bpm);
  }
}
