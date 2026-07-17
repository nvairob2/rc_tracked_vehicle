#include <Arduino.h>

// БЫЛО:
// const int RPWM_1 = PA6; 
// const int LPWM_1 = PA7; 

// СТАЛО (поменяли пины местами):
const int RPWM_1 = PA7; // Теперь это ШИМ "Вперед" для Борта 1
const int LPWM_1 = PA6; // Теперь это ШИМ "Назад" для Борта 1

const int EN_1   = PA2;   
HardwareTimer *encoderTimer1 = new HardwareTimer(TIM5); 

// --- БОРТ 2 (ШИМ: TIM2 -> PA15/PB3, Энкодер: TIM4 -> PB6/PB7) ---
const int RPWM_2 = PA15; // Переключили с PA8
const int LPWM_2 = PB3;  // Переключили с PA9
const int EN_2   = PA3;   
HardwareTimer *encoderTimer2 = new HardwareTimer(TIM4); 

void setup() {
  Serial.begin(115200); 
  delay(2000); // Стабилизация USB для Mac
  
  // Устанавливаем единую фиксированную частоту ШИМ для всех пинов моторов (например, 2 кГц)
  // Это уберет писк и заставит BTS7960B работать максимально эффективно
  analogWriteFrequency(2000);

  // Активация логики обоих драйверов
  pinMode(EN_1, OUTPUT);
  pinMode(EN_2, OUTPUT);
  digitalWrite(EN_1, HIGH); 
  digitalWrite(EN_2, HIGH); 
  
  // Инициализация пинов ШИМ скорости
  pinMode(RPWM_1, OUTPUT); pinMode(LPWM_1, OUTPUT);
  pinMode(RPWM_2, OUTPUT); pinMode(LPWM_2, OUTPUT);

  // НАСТРОЙКА ЭНКОДЕРА 1 (Таймер 5, пины PA0 и PA1)
  pinMode(PA0, INPUT_PULLUP);
  pinMode(PA1, INPUT_PULLUP);
  encoderTimer1->setMode(1, (TimerModes_t)TIM_ENCODERMODE_TI12, PA0); 
  encoderTimer1->setMode(2, (TimerModes_t)TIM_ENCODERMODE_TI12, PA1); 
  encoderTimer1->resume(); 

  // НАСТРОЙКА ЭНКОДЕРА 2 (Таймер 4, пины PB6 и PB7)
  pinMode(PB6, INPUT_PULLUP);
  pinMode(PB7, INPUT_PULLUP);
  encoderTimer2->setMode(1, (TimerModes_t)TIM_ENCODERMODE_TI12, PB6); 
  encoderTimer2->setMode(2, (TimerModes_t)TIM_ENCODERMODE_TI12, PB7); 
  encoderTimer2->resume(); 
}

void loop() {
  // ---- СИНХРОННЫЙ ПЛАВНЫЙ СТАРТ ВПЕРЕД (Стартуем сразу с 50 для преодоления трения) ----
  analogWrite(LPWM_1, 0);
  analogWrite(LPWM_2, 0);
  
  Serial.println(">>> СИНХРОННЫЙ РАЗГОН ВПЕРЕД <<<");
  for (int speed = 50; speed <= 255; speed++) {
    analogWrite(RPWM_1, speed);
    analogWrite(RPWM_2, speed);
    delay(10);
  }
  
  // Вывод тиков обоих бортов в Монитор порта (с инверсией Борта 1)
  for (int i = 0; i < 20; i++) {
    // Борт 1 инвертируем знаком минус, чтобы оба счетчика росли в плюс при движении вперед
    int32_t pos_1 = -((int32_t)encoderTimer1->getCount()); 
    int32_t pos_2 = (int32_t)encoderTimer2->getCount();

    Serial.print("Борт 1 [Тики]: "); Serial.print(pos_1);
    Serial.print("  |  Борт 2 [Тики]: "); Serial.println(pos_2);
    delay(100);
  }

  // ---- СИНХРОННОЕ ПЛАВНОЕ ТОРМОЖЕНИЕ ----
  Serial.println(">>> ТОРМОЖЕНИЕ <<<");
  for (int speed = 255; speed >= 0; speed--) {
    analogWrite(RPWM_1, speed);
    analogWrite(RPWM_2, speed);
    delay(10);
  }
  delay(2000); 
}
