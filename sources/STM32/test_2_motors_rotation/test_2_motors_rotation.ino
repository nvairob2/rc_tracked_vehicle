#include <Arduino.h>

// --- БОРТ 1 (ШИМ: PA7/PA6, Энкодер: PB0/PB1) ---
const int RPWM_1 = PA7; 
const int LPWM_1 = PA6; 
const int EN_1   = PA2;   
const int ENCODER_A1 = PB0; 
const int ENCODER_B1 = PB1; 

// --- БОРТ 2 (ШИМ: PA15/PB3, Энкодер: PB12/PB13) ---
const int RPWM_2 = PA15; 
const int LPWM_2 = PB3;  
const int EN_2   = PA3;   
const int ENCODER_A2 = PB12; 
const int ENCODER_B2 = PB13; 

// Переменные прерываний
volatile int32_t encoderTicks1 = 0;
volatile int32_t encoderTicks2 = 0;

// Переменные для расчета скорости
int32_t lastPos1 = 0;
int32_t lastPos2 = 0;
unsigned long lastTime = 0;
unsigned long stateTime = 0;
int currentBlock = 0;

// --- ПЕРЕМЕННЫЕ УПРАВЛЕНИЯ (Сюда потом будут прилетать данные с пульта) ---
int moveTarget = 0; // Направление движения: от -255 (назад) до 255 (вперед)
int turnTarget = 0; // Направление поворота: от -127 (влево) до 127 (вправо)

// Мощности ШИМ
float currentPWM_1 = 0; 
float Kp = 0.05; // Коэффициент автопилота

void encoderISR1() {
  if (digitalRead(ENCODER_B1) > 0) encoderTicks1++;
  else encoderTicks1--;
}

void encoderISR2() {
  if (digitalRead(ENCODER_B2) > 0) encoderTicks2++;
  else encoderTicks2--;
}

// Функция для подачи физического ШИМ на мосты драйвера с учетом знака
void setMotorPWM(int rpwm_pin, int lpwm_pin, int pwm_value) {
  if (pwm_value >= 0) {
    analogWrite(lpwm_pin, 0);
    analogWrite(rpwm_pin, pwm_value);
  } else {
    analogWrite(rpwm_pin, 0);
    analogWrite(lpwm_pin, abs(pwm_value));
  }
}

void setup() {
  Serial.begin(115200); 
  delay(2000); 
  
  analogWriteFrequency(2000); // Беззвучный ШИМ

  pinMode(EN_1, OUTPUT); pinMode(EN_2, OUTPUT);
  digitalWrite(EN_1, HIGH); digitalWrite(EN_2, HIGH); 
  
  pinMode(RPWM_1, OUTPUT); pinMode(LPWM_1, OUTPUT);
  pinMode(RPWM_2, OUTPUT); pinMode(LPWM_2, OUTPUT);

  pinMode(ENCODER_A1, INPUT_PULLUP); pinMode(ENCODER_B1, INPUT_PULLUP);
  pinMode(ENCODER_A2, INPUT_PULLUP); pinMode(ENCODER_B2, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A1), encoderISR1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A2), encoderISR2, CHANGE);

  lastTime = millis();
  stateTime = millis();
}

void loop() {
  // --- ДЕМОНСТРАЦИОННЫЙ СЦЕНАРИЙ СМЕНЫ ДВИЖЕНИЯ (Каждые 4 секунды) ---
  if (millis() - stateTime >= 4000) {
    stateTime = millis();
    currentBlock++;
    if (currentBlock > 3) currentBlock = 0;
  }

  if (currentBlock == 0) { // 1. Едем прямо вперед
    moveTarget = 150;
    turnTarget = 0;
  } else if (currentBlock == 1) { // 2. Плавно поворачиваем направо на ходу
    moveTarget = 150;
    turnTarget = 60; 
  } else if (currentBlock == 2) { // 3. Разворот на месте ("по-танковому") влево
    moveTarget = 0;
    turnTarget = -100;
  } else if (currentBlock == 3) { // 4. Стоим на месте (тормоз)
    moveTarget = 0;
    turnTarget = 0;
  }

  // --- РАБОТА АВТОПИЛОТА (Каждые 20 мс) ---
  if (millis() - lastTime >= 20) {
    lastTime = millis();

    noInterrupts();
    int32_t actPos1 = encoderTicks1;
    int32_t actPos2 = encoderTicks2;
    interrupts();

    // Вычисляем реальные скорости (с сохранением знака направления!)
    int32_t speed1 = actPos1 - lastPos1;
    int32_t speed2 = actPos2 - lastPos2;

    lastPos1 = actPos1;
    lastPos2 = actPos2;

    // Расчет идеальных целевых скоростей для каждого борта
    int targetSpeed2 = moveTarget - turnTarget;
    int targetSpeed1 = moveTarget + turnTarget;

    // В режиме полного стопа обнуляем регулятор
    if (moveTarget == 0 && turnTarget == 0) {
      currentPWM_1 = 0;
      setMotorPWM(RPWM_1, LPWM_1, 0);
      setMotorPWM(RPWM_2, LPWM_2, 0);
      Serial.println(" СТОП ");
      return;
    }

    // Вычисляем, насколько Борт 1 отклонился от своей индивидуальной цели относительно Борта 2
    // Находим разницу реальных соотношений скоростей и целевых
    int32_t realDifference = speed2 - speed1;
    int32_t targetDifference = targetSpeed2 - targetSpeed1;
    int32_t error = realDifference - targetDifference;

    // Корректируем ШИМ Борта 1
    currentPWM_1 += (float)error * Kp;

    // Рассчитываем итоговую мощность на моторы
    int finalPWM_2 = targetSpeed2;
    int finalPWM_1 = targetSpeed1 + (int)currentPWM_1;

    // Ограничение диапазонов ШИМ от -255 до 255
    finalPWM_1 = constrain(finalPWM_1, -255, 255);
    finalPWM_2 = constrain(finalPWM_2, -255, 255);

    // Физическая отправка ШИМ на драйверы
    setMotorPWM(RPWM_1, LPWM_1, finalPWM_1);
    setMotorPWM(RPWM_2, LPWM_2, finalPWM_2);

    // Мониторинг маневров
    Serial.print("Режим: "); Serial.print(currentBlock);
    Serial.print(" | Скорость Б1: "); Serial.print(speed1);
    Serial.print(" | Скорость Б2: "); Serial.print(speed2);
    Serial.print(" | ШИМ Б1: "); Serial.print(finalPWM_1);
    Serial.print(" | ШИМ Б2: "); Serial.println(finalPWM_2);
  }
}
