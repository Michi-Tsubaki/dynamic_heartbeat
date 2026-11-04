// CONSTANTS
// WATER (KOOLANCE PMP-420 202572)
const int WATER_PUMP = 2;

// AIR (DC MOTOR)
const int PWMA = 3;
const int AIN1 = 8;
const int AIN2 = 9;
const int STBY = 10;

// AIR VALVES
const int VALVE1 = 6;
// const int VALVE2 = 4;  // Disabled: not enough current to drive both valves
const int AIR_PUMP_SPEED = 255;

// AIR VALVES DEFAULT SETTING
const int VALVE_OPEN = HIGH;
const int VALVE_CLOSE = LOW;

// SETTINGS
const float BPM_MIN = 40.0;
const float BPM_MAX = 120.0;

// VARIABLES
float bpm = 80.0;
const float ATRIUM_RATIO = 0.3;      // Fraction of the period spent on atrial expansion
const float VENTRICLE_RATIO = 0.60;  // Fraction of the period spent on ventricular expansion
const unsigned long ATRIUM_FILL_MAX_MS = 250;  // Max time valve 1 may stay closed (atrium bursts at 300 ms or more)
const float VENTRICLE_AIR_RATIO = 0.9;  // Fraction of ventricular expansion with the air pump running (for the rest, air moves from atrium to ventricle and the atrium deflates)
const float WATER_ON_RATIO = 0.3;  // Fraction of the period the water pump is ON
const unsigned long CHARGE_INTERVAL_MS = 30000;  // Charge air at start and then at this interval
const unsigned long CHARGE_MS = 1000;  // Charge time with both valves closed
const unsigned long CHARGE_FILL_MAX_MS = 1100;  // Max time valve 1 may stay closed while charging (replaces ATRIUM_FILL_MAX_MS)

enum Phase { PHASE_STOP, PHASE_ATRIUM, PHASE_VENTRICLE, PHASE_RELAX };
Phase phase = PHASE_STOP;
bool running = true;

unsigned long cycleStart = 0;
unsigned long periodMs = 0;
unsigned long atriumMs = 0;
unsigned long ventricleEndMs = 0;
unsigned long ventricleAirEndMs = 0;
unsigned long waterOnMs = 0;
unsigned long lastChargeMs = 0;
bool needCharge = true;
bool charging = false;

bool airOn = false;
bool valve1Open = false;
unsigned long atriumFillStart = 0;
bool waterOn = true;

char cmdBuf[16];
int cmdLen = 0;

void setup() {
  pinMode(VALVE1, OUTPUT);
  // pinMode(VALVE2, OUTPUT);
  pinMode(WATER_PUMP, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(STBY, OUTPUT);

  setAir(false, false, false);
  setWater(false);

  Serial.begin(115200);
  Serial.print("# heartbeat start, bpm ");
  Serial.println(bpm);
}

void loop() {
  readSerial();

  unsigned long now = millis();
  if (running) {
    updatePhase(now);
    updateWater(now);
  }
  safetyCheck(millis());  // Re-read the time: atriumFillStart is set with millis() inside setAir(), so an older now would underflow
}

void startCycle(unsigned long now) {
  cycleStart = now;
  periodMs = 60000.0 / bpm;
  atriumMs = min((unsigned long)(periodMs * ATRIUM_RATIO), ATRIUM_FILL_MAX_MS);

  // Charge: replace this beat's atrial phase with pumping while both valves are closed
  bool charge = needCharge || now - lastChargeMs >= CHARGE_INTERVAL_MS;
  unsigned long chargeExtraMs = 0;
  if (charge) {
    chargeExtraMs = CHARGE_MS - atriumMs;
    atriumMs = CHARGE_MS;
    lastChargeMs = now;
    needCharge = false;
    Serial.println("# charge");
  }

  ventricleEndMs = atriumMs + periodMs * VENTRICLE_RATIO;
  ventricleAirEndMs = atriumMs + (ventricleEndMs - atriumMs) * VENTRICLE_AIR_RATIO;
  waterOnMs = periodMs * WATER_ON_RATIO;
  periodMs += chargeExtraMs;  // A charge only lengthens the atrial phase; keep the other phases at their normal length

  phase = PHASE_ATRIUM;
  charging = charge;
  setAir(true, false, !charge);  // Atrial expansion (P wave)
}

void updatePhase(unsigned long now) {
  unsigned long t = now - cycleStart;

  switch (phase) {
    case PHASE_STOP:
      startCycle(now);
      break;

    case PHASE_ATRIUM:
      if (t >= atriumMs) {
        phase = PHASE_VENTRICLE;
        charging = false;
        setAir(ventricleAirEndMs > atriumMs, true, false);  // Ventricular expansion (QRS)
        Serial.print("BEAT ");
        Serial.println(bpm);
      }
      break;

    case PHASE_VENTRICLE:
      if (t >= ventricleEndMs) {
        phase = PHASE_RELAX;
        setAir(false, true, true);  // Vent (rest)
      } else if (airOn && t >= ventricleAirEndMs) {
        setAir(false, true, false);
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
  setWater(phase != PHASE_ATRIUM && t - atriumMs < waterOnMs);
}

void stopAll() {
  running = false;
  phase = PHASE_STOP;
  needCharge = true;
  charging = false;
  setWater(false);
  setAir(false, true, true);
  delay(1000);
  setAir(false, false, false);
}

void setAir(bool pump, bool v1Open, bool v2Open) {
  bool wasFilling = airOn && !valve1Open;
  bool filling = pump && !v1Open;
  if (filling && !wasFilling) {
    atriumFillStart = millis();
  }

  digitalWrite(VALVE1, v1Open ? VALVE_OPEN : VALVE_CLOSE);
  // digitalWrite(VALVE2, v2Open ? VALVE_OPEN : VALVE_CLOSE);

  if (pump != airOn) {
    digitalWrite(STBY, pump ? HIGH : LOW);
    digitalWrite(AIN1, pump ? HIGH : LOW);
    digitalWrite(AIN2, LOW);
    analogWrite(PWMA, pump ? AIR_PUMP_SPEED : 0);
  }

  airOn = pump;
  valve1Open = v1Open;
}

void setWater(bool on) {
  if (on != waterOn) {
    digitalWrite(WATER_PUMP, on ? HIGH : LOW);
    waterOn = on;
  }
}

void safetyCheck(unsigned long now) {
  unsigned long limitMs = charging ? CHARGE_FILL_MAX_MS : ATRIUM_FILL_MAX_MS;
  if (airOn && !valve1Open && now - atriumFillStart >= limitMs) {
    charging = false;
    setAir(false, true, true);
    Serial.println("# SAFETY: valve1 closed too long, air pump stopped and vented");
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
  if (s[0] == 's') {
    stopAll();
    Serial.println("# stop");
    return;
  }
  if (s[0] == 'g') {
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
