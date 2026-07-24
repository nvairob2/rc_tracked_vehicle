#include <Arduino.h>

// --- БОРТ 1 (ШИМ: PA7/PA6, EN: PA2) ---
const int RPWM_1 = PA7; const int LPWM_1 = PA6; const int EN_1 = PA2;   

// --- БОРТ 2 (ШИМ: PA15/PB3, EN: PA3) ---
const int RPWM_2 = PA15; const int LPWM_2 = PB3; const int EN_2 = PA3;   

int moveTarget = 0; 
int turnTarget = 0; 

// Строка-буфер для посимвольного чтения UART
String inputBuffer = "";

void setMotorPWM(int rpwm_pin, int lpwm_pin, int pwm_value) {
  if (pwm_value >= 0) { analogWrite(lpwm_pin, 0); analogWrite(rpwm_pin, pwm_value); }
  else { analogWrite(rpwm_pin, 0); analogWrite(lpwm_pin, abs(pwm_value)); }
}

void setup() {
  Serial.begin(115200);  // Логи на Mac
  
  // Переназначаем Serial1 на чистые пины PB6 и PB7
  Serial1.setTx(PB6);
  Serial1.setRx(PB7);
  Serial1.begin(115200); 
  
  delay(2000); 
  analogWriteFrequency(2000); // Беззвучный ШИМ

  pinMode(EN_1, OUTPUT); pinMode(EN_2, OUTPUT);
  digitalWrite(EN_1, HIGH); digitalWrite(EN_2, HIGH); 
  
  pinMode(RPWM_1, OUTPUT); pinMode(LPWM_1, OUTPUT);
  pinMode(RPWM_2, OUTPUT); pinMode(LPWM_2, OUTPUT);
}

void loop() {
  // --- БЫСТРЫЙ НЕБЛОКИРУЮЩИЙ ПРИЕМ ДАННЫХ ПО СИМВОЛАМ ---
  while (Serial1.available() > 0) {
    char c = Serial1.read(); // Читаем ровно один символ
    
    if (c == '\n') { // Если дошли до конца строки — парсим данные
      inputBuffer.trim();
      int commaIndex = inputBuffer.indexOf(',');
      if (commaIndex > 0) {
        moveTarget = inputBuffer.substring(0, commaIndex).toInt();
        turnTarget = inputBuffer.substring(commaIndex + 1).toInt();
        
        // РЕАКТИВНОЕ НАЗНАЧЕНИЕ СКОРОСТЕЙ (Без автопилота)
        int finalPWM_1 = moveTarget + turnTarget;
        int finalPWM_2 = moveTarget - turnTarget;

        finalPWM_1 = constrain(finalPWM_1, -255, 255);
        finalPWM_2 = constrain(finalPWM_2, -255, 255);

        setMotorPWM(RPWM_1, LPWM_1, finalPWM_1);
        setMotorPWM(RPWM_2, LPWM_2, finalPWM_2);
        
        // Выводим подтверждение
        Serial.print("Мгновенно выполнено -> Move="); Serial.print(moveTarget);
        Serial.print(" Turn="); Serial.println(turnTarget);
      }
      inputBuffer = ""; // Очищаем буфер строки
    } else {
      inputBuffer += c; // Копим символы в строку
    }
  }
}
