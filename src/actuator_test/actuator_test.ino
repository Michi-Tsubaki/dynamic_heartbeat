// 全アクチュエータ自動チェック
// 各アクチュエータを順番に3秒ずつONにする (間に1秒停止)。これを繰り返す。
// 現在の動作はシリアルモニタ (115200bps) に表示される。
//
//   電磁弁1 -> 電磁弁2 -> 水冷ポンプ -> モータ正転 -> モータ逆転 -> (繰り返し)

// MOSFET
const int VALVE1 = 6;
const int VALVE2 = 4;
const int PUMP = 2;

// モータドライバ (Aチャンネルのみ使用)
const int PWMA = 3;
const int AIN1 = 8;
const int AIN2 = 9;
const int STBY = 10;

const unsigned long ON_MS = 3000;
const unsigned long OFF_MS = 1000;
const int MOTOR_SPEED = 255;  // 0-255 (フル)

void setup() {
  pinMode(VALVE1, OUTPUT);
  pinMode(VALVE2, OUTPUT);
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
  testPin("Valve2 (D4)", VALVE2);
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
  digitalWrite(VALVE2, LOW);
  digitalWrite(PUMP, LOW);
  motorStop();
}
