#include <Arduino.h>

// --- БОРТ 1 (ШИМ / EN) ---
const int RPWM_1 = PA7;
const int LPWM_1 = PA6;
const int EN_1   = PA2;
const int ENC1_A = PA0;
const int ENC1_B = PA1;

// --- БОРТ 2 ---
const int RPWM_2 = PA15;
const int LPWM_2 = PB3;
const int EN_2   = PA3;
const int ENC2_A = PA8;
const int ENC2_B = PA9;

// По логу FORWARD: борт1 давал spd<0, борт2 spd>0 → инверсия знака борта 1
const int8_t ENC_SIGN_1 = -1;
const int8_t ENC_SIGN_2 = 1;

const bool SPEED_CLOSED_LOOP = true;

const unsigned long CONTROL_DT_MS = 20;
// Калибровка по FORWARD/REVERSE cmd=±150: установившийся |spd|≈50 → 50/150
float CMD_TO_TICKS = 0.333f;
float Kp = 0.25f;
float Ki = 0.015f;
const float INTEGRAL_MAX = 40.0f;
const int CORRECTION_MAX = 40;
const int SPEED_CLAMP = 200;
const int CMD_DEADZONE = 5;
const int MIN_TICKS_FOR_LOOP = 3;

volatile int32_t encTicks1 = 0;
volatile int32_t encTicks2 = 0;
volatile uint8_t prevQuad1 = 0;
volatile uint8_t prevQuad2 = 0;

int moveTarget = 0;
int turnTarget = 0;
String inputBuffer = "";

int32_t lastPos1 = 0;
int32_t lastPos2 = 0;
unsigned long lastControlMs = 0;
float integral1 = 0.0f;
float integral2 = 0.0f;

static const int8_t QUAD_TABLE[16] = {
  0, +1, -1, 0,
  -1, 0, 0, +1,
  +1, 0, 0, -1,
  0, -1, +1, 0
};

uint8_t readQuad(int pinA, int pinB) {
  return (uint8_t)((digitalRead(pinA) << 1) | digitalRead(pinB));
}

void handleEncoder(int pinA, int pinB, volatile uint8_t &prev,
                   volatile int32_t &ticks, int8_t sign) {
  uint8_t curr = readQuad(pinA, pinB);
  int8_t step = QUAD_TABLE[(prev << 2) | curr];
  if (step != 0) {
    ticks += (int32_t)sign * step;
  }
  prev = curr;
}

void enc1A_ISR() { handleEncoder(ENC1_A, ENC1_B, prevQuad1, encTicks1, ENC_SIGN_1); }
void enc1B_ISR() { handleEncoder(ENC1_A, ENC1_B, prevQuad1, encTicks1, ENC_SIGN_1); }
void enc2A_ISR() { handleEncoder(ENC2_A, ENC2_B, prevQuad2, encTicks2, ENC_SIGN_2); }
void enc2B_ISR() { handleEncoder(ENC2_A, ENC2_B, prevQuad2, encTicks2, ENC_SIGN_2); }

void setMotorPWM(int rpwm_pin, int lpwm_pin, int pwm_value) {
  if (pwm_value >= 0) {
    analogWrite(lpwm_pin, 0);
    analogWrite(rpwm_pin, pwm_value);
  } else {
    analogWrite(rpwm_pin, 0);
    analogWrite(lpwm_pin, abs(pwm_value));
  }
}

void stopMotors() {
  integral1 = 0.0f;
  integral2 = 0.0f;
  setMotorPWM(RPWM_1, LPWM_1, 0);
  setMotorPWM(RPWM_2, LPWM_2, 0);
}

int32_t readSpeed(volatile int32_t &ticks, int32_t &lastPos) {
  noInterrupts();
  int32_t pos = ticks;
  interrupts();
  int32_t delta = pos - lastPos;
  lastPos = pos;
  return constrain(delta, -SPEED_CLAMP, SPEED_CLAMP);
}

int speedTrim(int cmd, int32_t measuredTicks, float &integral) {
  if (abs(cmd) <= CMD_DEADZONE) {
    integral = 0.0f;
    return 0;
  }
  if (abs(measuredTicks) < MIN_TICKS_FOR_LOOP) {
    integral *= 0.5f;
    return 0;
  }

  float error = (float)cmd * CMD_TO_TICKS - (float)measuredTicks;
  integral = constrain(integral + error, -INTEGRAL_MAX, INTEGRAL_MAX);
  return constrain((int)(Kp * error + Ki * integral), -CORRECTION_MAX, CORRECTION_MAX);
}

void setup() {
  Serial.begin(115200);

  Serial1.setTx(PB6);
  Serial1.setRx(PB7);
  Serial1.begin(115200);

  delay(500);
  analogWriteFrequency(2000);

  pinMode(EN_1, OUTPUT);
  pinMode(EN_2, OUTPUT);
  digitalWrite(EN_1, HIGH);
  digitalWrite(EN_2, HIGH);

  pinMode(RPWM_1, OUTPUT);
  pinMode(LPWM_1, OUTPUT);
  pinMode(RPWM_2, OUTPUT);
  pinMode(LPWM_2, OUTPUT);

  pinMode(ENC1_A, INPUT_PULLUP);
  pinMode(ENC1_B, INPUT_PULLUP);
  pinMode(ENC2_A, INPUT_PULLUP);
  pinMode(ENC2_B, INPUT_PULLUP);

  prevQuad1 = readQuad(ENC1_A, ENC1_B);
  prevQuad2 = readQuad(ENC2_A, ENC2_B);

  attachInterrupt(digitalPinToInterrupt(ENC1_A), enc1A_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC1_B), enc1B_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC2_A), enc2A_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC2_B), enc2B_ISR, CHANGE);

  lastControlMs = millis();
  Serial.println("motor_controller: signs calibrated, speed trim on");
}

void loop() {
  while (Serial1.available() > 0) {
    char c = Serial1.read();
    if (c == '\n') {
      inputBuffer.trim();
      int commaIndex = inputBuffer.indexOf(',');
      if (commaIndex > 0) {
        moveTarget = inputBuffer.substring(0, commaIndex).toInt();
        turnTarget = inputBuffer.substring(commaIndex + 1).toInt();
      }
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }

  unsigned long now = millis();
  if (now - lastControlMs < CONTROL_DT_MS) {
    return;
  }
  lastControlMs = now;

  int cmd1 = constrain(moveTarget + turnTarget, -255, 255);
  int cmd2 = constrain(moveTarget - turnTarget, -255, 255);

  if (abs(cmd1) <= CMD_DEADZONE && abs(cmd2) <= CMD_DEADZONE) {
    stopMotors();
    readSpeed(encTicks1, lastPos1);
    readSpeed(encTicks2, lastPos2);
    return;
  }

  int32_t speed1 = readSpeed(encTicks1, lastPos1);
  int32_t speed2 = readSpeed(encTicks2, lastPos2);

  int pwm1 = cmd1;
  int pwm2 = cmd2;
  if (SPEED_CLOSED_LOOP) {
    pwm1 = constrain(cmd1 + speedTrim(cmd1, speed1, integral1), -255, 255);
    pwm2 = constrain(cmd2 + speedTrim(cmd2, speed2, integral2), -255, 255);
  }

  setMotorPWM(RPWM_1, LPWM_1, pwm1);
  setMotorPWM(RPWM_2, LPWM_2, pwm2);

  noInterrupts();
  int32_t tot1 = encTicks1;
  int32_t tot2 = encTicks2;
  interrupts();

  Serial.print("cmd=");
  Serial.print(cmd1);
  Serial.print(',');
  Serial.print(cmd2);
  Serial.print(" spd=");
  Serial.print(speed1);
  Serial.print(',');
  Serial.print(speed2);
  Serial.print(" tot=");
  Serial.print(tot1);
  Serial.print(',');
  Serial.print(tot2);
  Serial.print(" pwm=");
  Serial.print(pwm1);
  Serial.print(',');
  Serial.println(pwm2);
}
