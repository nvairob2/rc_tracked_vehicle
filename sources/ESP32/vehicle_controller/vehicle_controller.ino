#include <Arduino.h>

// Реальные номера пинов для физической маркировки RX/TX на чипе ESP32-S3
#define RX_PIN 44
#define TX_PIN 43

void setup() {
  Serial.begin(115200); // Логи в компьютер через USB
  
  // Инициализируем порт связи с STM32 на родных пинах S3
  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  
  Serial.println("ESP32-S3 (N16R8) инициализирована на пинах 43/44!");
}

void loop() {
  // Команда 1: Едем вперед
  Serial.println("Отправка на STM32: Прямо вперед");
  Serial1.println("150,0"); 
  delay(4000);

  // Команда 2: Поворот направо
  Serial.println("Отправка на STM32: Поворот направо");
  Serial1.println("150,60"); 
  delay(4000);

  // Команда 3: Разворот на месте влево
  Serial.println("Отправка на STM32: Разворот влево");
  Serial1.println("0,-100"); 
  delay(4000);

  // Команда 4: Полный стоп
  Serial.println("Отправка на STM32: Тормоз");
  Serial1.println("0,0"); 
  delay(4000);
}
