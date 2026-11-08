// Ventricular fibrillation demo (ignores the pulse sensor)
//   Normal beat 60 BPM for 20 s -> stop 3 s -> ventricular fibrillation (~120 BPM) for 7 s -> (repeat)
//
// Air: parallel (pump -> VALVE1 -> atrium, pump -> VALVE2 -> ventricle).
// The air pump runs whenever either valve is open. Air leaks out of the chambers naturally.
//
// Normal: atrium and ventricle inflate alternately (atrium -> pause -> ventricle -> rest) to emphasize
//         the phase difference; water pump runs during ventricular expansion.
// VF: like the real thing, the ventricle only quivers, irregularly and weakly, with no coordinated
//     contraction (no atrium-ventricle phase relation, no output -> water pump off).

// AIR PUMP (MOSFET)
const int AIR_PUMP = 2;

// WATER (KOOLANCE PMP-420 202572, motor driver channel A)
const int PWMA = 3;
const int AIN1 = 8;
const int AIN2 = 9;
const int STBY = 10;
const int WATER_PUMP_SPEED = 255;
const bool WATER_ALWAYS_ON = true;  // true: water pump always at full speed (ignores the beat timing)

// AIR VALVES
const int VALVE1 = 6;  // Atrium
const int VALVE2 = 4;  // Ventricle
const int VALVE_OPEN = HIGH;
const int VALVE_CLOSE = LOW;

// SCENARIO
const unsigned long NORMAL_MS = 20000;
const unsigned long STOP_MS = 3000;
const unsigned long VF_MS = 7000;
const bool REPEAT = true;  // false: stay stopped after the VF

// NORMAL BEAT
const float NORMAL_BPM = 60.0;
const float ATRIUM_RATIO = 0.25;     // Fraction of the period spent on atrial expansion
const float AV_GAP_RATIO = 0.15;     // Pause between atrium and ventricle (both closed: atrium deflates first)
const float VENTRICLE_RATIO = 0.40;  // Fraction of the period spent on ventricular expansion (the rest is rest)
const unsigned long ATRIUM_FILL_MAX_MS = 250;  // Atrium bursts at 300 ms or more
const float WATER_ON_RATIO = 0.3;    // Fraction of the period the water pump is ON

// VENTRICULAR FIBRILLATION
const float VF_BPM = 120.0;          // Average quiver rate
const float VF_JITTER = 0.4;         // Interval varies randomly by +-40%
const unsigned long VF_PUFF_MIN_MS = 60;   // Each quiver is a short, weak puff of air
const unsigned long VF_PUFF_MAX_MS = 160;

enum Mode { MODE_NORMAL, MODE_STOP, MODE_VF, MODE_END };
Mode mode = MODE_NORMAL;
unsigned long modeStart = 0;

// Normal beat
unsigned long cycleStart = 0;
unsigned long periodMs = 0;
unsigned long atriumMs = 0;
unsigned long ventricleStartMs = 0;
unsigned long ventricleEndMs = 0;
unsigned long waterOnMs = 0;
bool beatSent = false;

// VF
unsigned long vfNextMs = 0;
unsigned long vfPuffEndMs = 0;

bool airOn = false;
bool waterOn = true;

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
  randomSeed(analogRead(A2));
  enterMode(MODE_NORMAL, millis());
}

void loop() {
  unsigned long now = millis();
  unsigned long t = now - modeStart;

  switch (mode) {
    case MODE_NORMAL:
      if (t >= NORMAL_MS) enterMode(MODE_STOP, now);
      else updateNormal(now);
      break;
    case MODE_STOP:
      if (t >= STOP_MS) enterMode(MODE_VF, now);
      break;
    case MODE_VF:
      if (t >= VF_MS) enterMode(REPEAT ? MODE_NORMAL : MODE_END, now);
      else updateVF(now);
      break;
    case MODE_END:
      break;
  }
}

void enterMode(Mode m, unsigned long now) {
  mode = m;
  modeStart = now;
  setAir(false, false);
  setWater(false);
  switch (m) {
    case MODE_NORMAL:
      Serial.println("# normal 60 BPM");
      startCycle(now);
      break;
    case MODE_STOP:
      Serial.println("# stop");
      break;
    case MODE_VF:
      Serial.println("# ventricular fibrillation");
      vfNextMs = now;
      vfPuffEndMs = now;
      break;
    case MODE_END:
      Serial.println("# end");
      break;
  }
}

// ---- Normal beat: atrium -> pause -> ventricle -> rest ----
void startCycle(unsigned long now) {
  cycleStart = now;
  periodMs = 60000.0 / NORMAL_BPM;
  atriumMs = min((unsigned long)(periodMs * ATRIUM_RATIO), ATRIUM_FILL_MAX_MS);
  ventricleStartMs = atriumMs + periodMs * AV_GAP_RATIO;
  ventricleEndMs = ventricleStartMs + periodMs * VENTRICLE_RATIO;
  waterOnMs = periodMs * WATER_ON_RATIO;
  beatSent = false;
}

void updateNormal(unsigned long now) {
  unsigned long t = now - cycleStart;
  if (t >= periodMs) {
    startCycle(now);
    t = 0;
  }

  if (t < atriumMs) {
    setAir(true, false);   // Atrial expansion
  } else if (t < ventricleStartMs) {
    setAir(false, false);  // A-V pause
  } else if (t < ventricleEndMs) {
    setAir(false, true);   // Ventricular expansion
    if (!beatSent) {       // Heart sound trigger for heartbeat_sound.py
      Serial.print("BEAT ");
      Serial.println(NORMAL_BPM);
      beatSent = true;
    }
  } else {
    setAir(false, false);  // Rest
  }
  setWater(t >= ventricleStartMs && t - ventricleStartMs < waterOnMs);
}

// ---- VF: irregular, short ventricular quivers; atrium and water pump idle ----
void updateVF(unsigned long now) {
  if ((long)(now - vfNextMs) >= 0) {
    float meanMs = 60000.0 / VF_BPM;
    float jitter = (random(-1000, 1001) / 1000.0) * VF_JITTER;
    unsigned long puffMs = random(VF_PUFF_MIN_MS, VF_PUFF_MAX_MS + 1);
    vfPuffEndMs = now + puffMs;
    Serial.print("QUIVER ");  // Weak quiver sound trigger for heartbeat_sound.py
    Serial.println(puffMs);
    vfNextMs = now + (unsigned long)(meanMs * (1.0 + jitter));
  }
  setAir(false, (long)(now - vfPuffEndMs) < 0);
}

// The air pump runs whenever either valve is open
void setAir(bool atriumOpen, bool ventricleOpen) {
  bool pump = atriumOpen || ventricleOpen;
  digitalWrite(VALVE1, atriumOpen ? VALVE_OPEN : VALVE_CLOSE);
  digitalWrite(VALVE2, ventricleOpen ? VALVE_OPEN : VALVE_CLOSE);
  if (pump != airOn) {
    digitalWrite(AIR_PUMP, pump ? HIGH : LOW);
    airOn = pump;
  }
}

void setWater(bool on) {
  if (WATER_ALWAYS_ON) on = true;
  if (on != waterOn) {
    digitalWrite(STBY, on ? HIGH : LOW);
    digitalWrite(AIN1, on ? HIGH : LOW);
    digitalWrite(AIN2, LOW);
    analogWrite(PWMA, on ? WATER_PUMP_SPEED : 0);
    waterOn = on;
  }
}
