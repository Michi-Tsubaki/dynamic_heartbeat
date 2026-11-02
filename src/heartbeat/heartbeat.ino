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
const int VALVE2 = 4;
const int AIR_PUMP_SPEED = 255;

// AIR VALVES DEFAULT SETTING
const int VALVE_OPEN = HIGH;
const int VALVE_CLOSE = LOW;

// SETTINGS
const float BPM_MIN = 40.0;
const float BPM_MAX = 120.0;

// VARIABLES
float bpm = 60.0;
const float ATRIUM_RATIO = 0.25;     // Fraction of the period spent on atrial expansion
const float VENTRICLE_RATIO = 0.4;   // Fraction of the period spent on ventricular expansion
const unsigned long ATRIUM_FILL_MAX_MS = 250;  // Max time valve 1 may stay closed (atrium bursts at 300 ms or more)
const float VENTRICLE_AIR_RATIO = 0.5;  // Fraction of ventricular expansion with the air pump running (for the rest, air moves from atrium to ventricle and the atrium deflates)
const float WATER_ON_RATIO = 0.3;  // Fraction of the period the water pump is ON

enum Phase { PHASE_STOP, PHASE_ATRIUM, PHASE_VENTRICLE, PHASE_RELAX };
Phase phase = PHASE_STOP;
bool running = true;

unsigned long cycleStart = 0;
unsigned long periodMs = 0;
unsigned long atriumMs = 0;
unsigned long ventricleEndMs = 0;
unsigned long ventricleAirEndMs = 0;
unsigned long waterOnMs = 0;

bool airOn = false;
bool valve1Open = false;
unsigned long atriumFillStart = 0;
bool waterOn = true;

char cmdBuf[16];
int cmdLen = 0;

void setup() {
  pinMode(VALVE1, OUTPUT);
  pinMode(VALVE2, OUTPUT);
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
  safetyCheck(now);
}

void startCycle(unsigned long now) {
  cycleStart = now;
  periodMs = 60000.0 / bpm;
  atriumMs = min((unsigned long)(periodMs * ATRIUM_RATIO), ATRIUM_FILL_MAX_MS);
  ventricleEndMs = atriumMs + periodMs * VENTRICLE_RATIO;
  ventricleAirEndMs = atriumMs + (ventricleEndMs - atriumMs) * VENTRICLE_AIR_RATIO;
  waterOnMs = periodMs * WATER_ON_RATIO;

  phase = PHASE_ATRIUM;
  setAir(true, false, true);  // Atrial expansion (P wave)
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
  digitalWrite(VALVE2, v2Open ? VALVE_OPEN : VALVE_CLOSE);

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
  if (airOn && !valve1Open && now - atriumFillStart >= ATRIUM_FILL_MAX_MS) {
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
