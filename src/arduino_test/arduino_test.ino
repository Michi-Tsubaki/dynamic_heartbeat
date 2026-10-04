const int SENSOR_PIN = A0;
const int LED = 13;

const int STBY = 10;
const int PWMB = 5;
const int BIN1 = 11;
const int BIN2 = 12;

const unsigned long SAMPLE_MS = 10;
const int SIGNAL_CENTER = 512;
const int SIGNAL_MIN = 50;
const int SIGNAL_MAX = 1000;
const unsigned long DROPOUT_MS = 1000;
const unsigned long SETTLE_MS = 1000;
const float BANDPASS[2][5] = {
  {0.010432413, 0.020864827, 0.010432413, -1.717844587, 0.763488563},
  {1.0, -2.0, 1.0, -1.958530943, 0.959707933}
};
const float ENVELOPE_ALPHA = 0.005;
const float THRESHOLD_RATIO = 0.8;
const float MIN_ENVELOPE = 2.0;
const unsigned long REFRACTORY_MS = 400;
const float OUTLIER_RATIO = 0.6;

const unsigned long WARMUP_MS = 5000;
const unsigned long RELEASE_MS = 2500;
const int MIN_IBI_COUNT = 3;
const int IBI_SIZE = 5;

const float DRIVE_RATIO = 0.25;
const float DRIVE_BPM_MIN = 10.0;
const float DRIVE_BPM_MAX = 25.0;
const unsigned long FULL_STROKE_MS = 1500;
const int MOTOR_SPEED = 255;

const int STATE_IDLE = 0;
const int STATE_WARMUP = 1;
const int STATE_ACTIVE = 2;

const int PHASE_STOP = 0;
const int PHASE_PUSH = 1;
const int PHASE_PULL = 2;
const int PHASE_REST = 3;

int state = STATE_IDLE;
int phase = PHASE_STOP;

float bandpassState[2][2];
float pulse = 0;
float envelope = 0;
float threshold = 0;
bool valid = false;
bool above = false;

unsigned long lastSampleTime = 0;
unsigned long lastBeatTime = 0;
unsigned long touchStartTime = 0;
unsigned long cycleStartTime = 0;
unsigned long settleUntil = 0;
unsigned long lastInRangeTime = 0;
unsigned long strokeMs = 0;

unsigned long ibi[IBI_SIZE];
int ibiCount = 0;
int ibiIndex = 0;

float heartBPM = 0;
float driveBPM = 0;

void setup() {
  pinMode(LED, OUTPUT);

  pinMode(STBY, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  Serial.begin(115200);

  motorStop();
}

void loop() {
  unsigned long now = millis();

  updateActuator(now);

  if (now - lastSampleTime < SAMPLE_MS) {
    return;
  }
  lastSampleTime = now;

  int raw = analogRead(SENSOR_PIN);
  updatePulse(raw, now);

  if (detectBeat(now)) {
    onBeat(now);
  }

  if (state != STATE_IDLE && !settling(now) && now - lastBeatTime > RELEASE_MS) {
    release();
  }

  printStatus(raw);
}

bool settling(unsigned long now) {
  return (long)(settleUntil - now) > 0;
}

void updatePulse(int raw, unsigned long now) {
  if (raw >= SIGNAL_MIN && raw <= SIGNAL_MAX) {
    lastInRangeTime = now;
  }

  if (now - lastInRangeTime >= DROPOUT_MS) {
    for (int i = 0; i < 2; i++) {
      bandpassState[i][0] = 0;
      bandpassState[i][1] = 0;
    }
    settleUntil = now + SETTLE_MS;
    valid = false;
    pulse = 0;
    envelope = 0;
    threshold = 0;
    return;
  }

  float x = raw - SIGNAL_CENTER;
  for (int i = 0; i < 2; i++) {
    const float *c = BANDPASS[i];
    float *z = bandpassState[i];
    float y = c[0] * x + z[0];
    z[0] = c[1] * x - c[3] * y + z[1];
    z[1] = c[2] * x - c[4] * y;
    x = y;
  }
  pulse = x;

  valid = !settling(now);
  if (!valid) {
    envelope = 0;
    threshold = 0;
    return;
  }

  envelope += ENVELOPE_ALPHA * (fabs(pulse) - envelope);
  threshold = envelope * THRESHOLD_RATIO;
}

bool detectBeat(unsigned long now) {
  bool beat = false;

  if (!valid) {
    above = false;
  } else if (!above && pulse > threshold) {
    above = true;
    beat = envelope >= MIN_ENVELOPE && now - lastBeatTime >= REFRACTORY_MS;
  } else if (above && pulse < 0) {
    above = false;
  }

  digitalWrite(LED, above ? HIGH : LOW);
  return beat;
}

void onBeat(unsigned long now) {
  if (state == STATE_IDLE) {
    state = STATE_WARMUP;
    touchStartTime = now;
    ibiCount = 0;
    ibiIndex = 0;
  } else {
    unsigned long interval = now - lastBeatTime;
    if (ibiCount >= MIN_IBI_COUNT && interval < OUTLIER_RATIO * medianIbi()) {
      return;
    }
    addIbi(interval);
    heartBPM = 60000.0 / medianIbi();
    driveBPM = constrain(heartBPM * DRIVE_RATIO, DRIVE_BPM_MIN, DRIVE_BPM_MAX);
  }
  lastBeatTime = now;

  if (state == STATE_WARMUP && now - touchStartTime >= WARMUP_MS && ibiCount >= MIN_IBI_COUNT) {
    state = STATE_ACTIVE;
  }
}

void release() {
  state = STATE_IDLE;
  heartBPM = 0;
  driveBPM = 0;
}

void addIbi(unsigned long value) {
  ibi[ibiIndex] = value;
  ibiIndex = (ibiIndex + 1) % IBI_SIZE;
  if (ibiCount < IBI_SIZE) {
    ibiCount++;
  }
}

unsigned long medianIbi() {
  unsigned long sorted[IBI_SIZE];

  for (int i = 0; i < ibiCount; i++) {
    unsigned long value = ibi[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > value) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = value;
  }

  return sorted[ibiCount / 2];
}

unsigned long drivePeriodMs() {
  return 60000.0 / driveBPM;
}

void updateActuator(unsigned long now) {
  unsigned long t = now - cycleStartTime;

  switch (phase) {
    case PHASE_STOP:
      if (state == STATE_ACTIVE) {
        startCycle(now);
      }
      break;

    case PHASE_PUSH:
      if (t >= strokeMs) {
        phase = PHASE_PULL;
        motorPull();
      }
      break;

    case PHASE_PULL:
      if (t >= 2 * strokeMs) {
        phase = PHASE_REST;
        motorStop();
      }
      break;

    case PHASE_REST:
      if (state != STATE_ACTIVE) {
        phase = PHASE_STOP;
      } else if (t >= drivePeriodMs()) {
        startCycle(now);
      }
      break;
  }
}

void startCycle(unsigned long now) {
  cycleStartTime = now;
  strokeMs = min(FULL_STROKE_MS, drivePeriodMs() / 2);
  phase = PHASE_PUSH;
  motorPush();
}

void motorPush() {
  digitalWrite(STBY, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMB, MOTOR_SPEED);
}

void motorPull() {
  digitalWrite(STBY, HIGH);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, MOTOR_SPEED);
}

void motorStop() {
  analogWrite(PWMB, 0);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  digitalWrite(STBY, LOW);
}

void printStatus(int raw) {
  Serial.print("Signal:");
  Serial.print(raw);
  Serial.print("\tPulse:");
  Serial.print(pulse);
  Serial.print("\tThreshold:");
  Serial.print(threshold);
  Serial.print("\tHeartBPM:");
  Serial.print(heartBPM);
  Serial.print("\tDriveBPM:");
  Serial.println(state == STATE_ACTIVE ? driveBPM : 0.0);
}
