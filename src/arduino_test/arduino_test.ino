int SensorOutPin = 0;
int LED = 13;
int Signal;
int Threshold = 550;

int STBY = 10;
int PWMB = 5;
int BIN1 = 11;
int BIN2 = 12;

int beatCount = 0;
int state = 0;

unsigned long startTime = 0;
unsigned long motorStartTime = 0;

float BPM = 0;

bool motorRunning = false;

void setup() {
  pinMode(LED, OUTPUT);

  pinMode(STBY, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  Serial.begin(9600);

  digitalWrite(STBY, LOW);

  startTime = millis();
}

void loop() {
  unsigned long now = millis();

  Signal = analogRead(SensorOutPin);

  if (Signal > Threshold) {
    digitalWrite(LED, HIGH);

    if (state == 0) {
      beatCount++;

      if (!motorRunning) {
        motorRunning = true;
        motorStartTime = now;

        digitalWrite(STBY, HIGH);
        analogWrite(PWMB, 255);

        digitalWrite(BIN1, LOW);
        digitalWrite(BIN2, HIGH);
      }
    }

    state = 1;
  } else {
    digitalWrite(LED, LOW);
    state = 0;
  }

  if (motorRunning) {
    unsigned long motorTime = now - motorStartTime;

    if (motorTime >= 2000 && motorTime < 4000) {
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
    }

    if (motorTime >= 4000) {
      digitalWrite(STBY, LOW);
      analogWrite(PWMB, 0);
      motorRunning = false;
    }
  }

  if (now - startTime >= 20000) {
    BPM = (float)beatCount * 3.0;

    Serial.print("Signal:");
    Serial.print(Signal);
    Serial.print("\tBPM:");
    Serial.println(BPM);

    beatCount = 0;
    startTime = now;
  } else {
    Serial.print("Signal:");
    Serial.print(Signal);
    Serial.print("\tBPM:");
    Serial.println(BPM);
  }

  delay(10);
}
