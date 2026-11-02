// Automatic check of all actuators
// Turns each actuator ON for 3 s in turn (with a 1 s pause between), and repeats.
// The current action is printed to the serial monitor (115200 bps).
//
//   Solenoid valve 1 -> Solenoid valve 2 -> Water pump -> Motor forward -> Motor reverse -> (repeat)

// MOSFET
const int VALVE1 = 6;
// const int VALVE2 = 4;  // Disabled: not enough current to drive both valves
const int PUMP = 2;

// Motor driver (channel A only)
const int PWMA = 3;
const int AIN1 = 8;
const int AIN2 = 9;
const int STBY = 10;

const unsigned long ON_MS = 3000;
const unsigned long OFF_MS = 1000;
const int MOTOR_SPEED = 255;  // 0-255 (255 = full)

void setup() {
  pinMode(VALVE1, OUTPUT);
  // pinMode(VALVE2, OUTPUT);
  pinMode(PUMP, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(STBY, OUTPUT);

  allOff();

  Serial.begin(115200);
  Serial.println("Actuator check start");
}

void loop() {
  testPin("Valve1 (D6)", VALVE1);
  // testPin("Valve2 (D4)", VALVE2);
  testPin("Pump (D2)", PUMP);
  testMotor("Motor FWD", 1);
  testMotor("Motor REV", -1);
  Serial.println("---- cycle done ----");
}

void testPin(const char *name, int pin) {
  Serial.print(name);
  Serial.println(" ON");
  digitalWrite(pin, HIGH);
  delay(ON_MS);
  digitalWrite(pin, LOW);
  Serial.print(name);
  Serial.println(" OFF");
  delay(OFF_MS);
}

void testMotor(const char *name, int dir) {
  Serial.print(name);
  Serial.println(" ON");
  digitalWrite(STBY, HIGH);
  digitalWrite(AIN1, dir > 0 ? HIGH : LOW);
  digitalWrite(AIN2, dir > 0 ? LOW : HIGH);
  analogWrite(PWMA, MOTOR_SPEED);
  delay(ON_MS);
  motorStop();
  Serial.print(name);
  Serial.println(" OFF");
  delay(OFF_MS);
}

void motorStop() {
  analogWrite(PWMA, 0);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(STBY, LOW);
}

void allOff() {
  digitalWrite(VALVE1, LOW);
  // digitalWrite(VALVE2, LOW);
  digitalWrite(PUMP, LOW);
  motorStop();
}
