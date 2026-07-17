#include <Arduino.h>

// Новые пины ШИМ для мотора (Таймер 3)
const int RPWM_PIN = PA6; 
const int LPWM_PIN = PA7; 
const int EN_PIN = PA2;   

// Создаем объект аппаратного Таймера 5 для энкодера (Пины PA0 и PA1)
HardwareTimer *encoderTimer = new HardwareTimer(TIM5);

void setup() {
  Serial.begin(115200); 
  delay(1000); // Задержка для стабилизации USB-порта на Mac
  
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH); // Активируем драйвер мотора
  
  pinMode(RPWM_PIN, OUTPUT);
  pinMode(LPWM_PIN, OUTPUT);

  // ЯВНАЯ НАСТРОЙКА ПИНОВ ДЛЯ АППАРАТНОГО ТАЙМЕРА ENCODER_MODE
  // Конфигурируем PA0 и PA1 как альтернативные входы с подтяжкой
  pinMode(PA0, INPUT_PULLUP);
  pinMode(PA1, INPUT_PULLUP);

  // Настраиваем Таймер 5 в режим аппаратного счетчика энкодера (по двум фазам)
  encoderTimer->setMode(1, (TimerModes_t)TIM_ENCODERMODE_TI12, PA0); 
  encoderTimer->setMode(2, (TimerModes_t)TIM_ENCODERMODE_TI12, PA1); 
  
  encoderTimer->resume(); // Запускаем аппаратный счетчик
}


void loop() {
  // Плавный ход ВПЕРЕД
  analogWrite(LPWM_PIN, 0);
  for (int speed = 0; speed <= 255; speed++) {
    analogWrite(RPWM_PIN, speed);
    delay(10);
  }
  
  // Выводим данные аппаратного счетчика в монитор порта
  for (int i = 0; i < 20; i++) {
    // Получаем значение регистра счетчика таймера
    uint32_t raw_ticks = encoderTimer->getCount();
    // Приводим к знаковому типу, чтобы корректно отображался минус при реверсе
    int32_t current_position = (int32_t)raw_ticks;

    Serial.print("Аппаратное положение мотора: ");
    Serial.println(current_position);
    delay(100);
  }

  // Торможение
  for (int speed = 255; speed >= 0; speed--) {
    analogWrite(RPWM_PIN, speed);
    delay(10);
  }
  delay(1000);

  // Плавный ход НАЗАД
  analogWrite(RPWM_PIN, 0);
  for (int speed = 0; speed <= 255; speed++) {
    analogWrite(LPWM_PIN, speed);
    delay(10);
  }

  // Выводим данные при реверсе
  for (int i = 0; i < 20; i++) {
    int32_t current_position = (int32_t)encoderTimer->getCount();
    Serial.print("Аппаратное положение мотора: ");
    Serial.println(current_position);
    delay(100);
  }

  // Торможение из реверса
  for (int speed = 255; speed >= 0; speed--) {
    analogWrite(LPWM_PIN, speed);
    delay(10);
  }
  delay(1000);
}
