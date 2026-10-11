#include <Arduino.h>

/*
 * Контроллер моторов гусеничной платформы 10 кг
 * Плата: WeAct Black Pill STM32F411CEU6
 *
 * Принимает по UART1 от ESP32 строки "move,turn\n" (−255…255),
 * считает arcade-mix по бортам, держит скорость по энкодерам (PI-trim)
 * и измеряет ток драйверов BTS7960B через пины IS.
 */

// =============================================================================
// Пины борта 1 (левый)
// =============================================================================
const int RPWM_1 = PA7;   // ШИМ «вперёд» → RPWM драйвера 1
const int LPWM_1 = PA6;   // ШИМ «назад»  → LPWM драйвера 1
const int EN_1   = PA2;   // R_EN+L_EN драйвера 1 (включение моста)
const int ENC1_A = PA0;   // Энкодер, фаза A (провод 3 шлейфа)
const int ENC1_B = PA1;   // Энкодер, фаза B (провод 4 шлейфа)
const int IS_1   = PA4;   // Ток драйвера 1: R_IS и L_IS замкнуты вместе → ADC

// =============================================================================
// Пины борта 2 (правый)
// =============================================================================
const int RPWM_2 = PA15;
const int LPWM_2 = PB3;
const int EN_2   = PA3;
const int ENC2_A = PA8;
const int ENC2_B = PA9;
const int IS_2   = PA5;   // Ток драйвера 2: R_IS и L_IS замкнуты вместе → ADC

// =============================================================================
// Энкодеры: знак направления
// «Вперёд» должно давать положительные тики на обоих бортах.
// По калибровке: борт 1 зеркальный → ENC_SIGN_1 = −1.
// =============================================================================
const int8_t ENC_SIGN_1 = -1;
const int8_t ENC_SIGN_2 = 1;

// =============================================================================
// Контур скорости (feedforward cmd + ограниченный PI-trim)
// =============================================================================
const bool SPEED_CLOSED_LOOP = true;
const unsigned long CONTROL_DT_MS = 20;  // период контура, мс

// Сколько тиков за CONTROL_DT ожидать на единицу |cmd|.
// Калибровка: установившийся |spd| при cmd=150 → SCALE = spd/150 ≈ 0.333.
float CMD_TO_TICKS = 0.333f;
float Kp = 0.25f;
float Ki = 0.015f;
const float INTEGRAL_MAX = 40.0f;   // anti-windup интегратора
const int CORRECTION_MAX = 40;      // потолок trim, чтобы не ломать open-loop базу
const int SPEED_CLAMP = 200;        // отсечка мусорных дельт энкодера
const int CMD_DEADZONE = 5;         // ниже — считаем «стоп»
const int MIN_TICKS_FOR_LOOP = 0;   // 0: PI и на малых оборотах (1–2 тика / 20 мс)

// =============================================================================
// Ток BTS7960B (IS)
//
// Пин IS — источник тока IIS ≈ IL / kILIS (kILIS типично 8500).
// На модуле IBT-2 уже стоит резистор RIS к GND → VIS = IIS × RIS.
// IL ≈ (VIS / RIS) × kILIS.
//
// Схема на breadboard (на каждый драйвер):
//   R_IS ──┬──► PA4 или PA5 (STM32)
//   L_IS ──┘
// Общая GND драйвера ↔ GND STM32 обязательна.
//
// ВАЖНО: в аварии IS может дать до ~4.5–7 мА → при RIS=1 кΩ до 7 В.
// АЦП STM32 максимум 3.3 В. Рекомендуется делитель или TVS/стабилитрон 3.3 В
// на линии IS. Пока VIS в норме (холостой ход <1 В) можно без делителя,
// но для защиты АЦП делитель предпочтителен.
// Если ставите делитель 1:2 — скорректируйте RIS_OHMS (калибровка амперметром).
// =============================================================================
const float ADC_VREF = 3.3f;          // опорное АЦП BlackPill
const int   ADC_MAX  = 4095;          // 12 бит
const float K_ILIS   = 8500.0f;       // IL / IIS по datasheet BTS7960 (типично)
// Эффективное сопротивление sense: штатный RIS модуля, с учётом делителя.
// Типичный IBT-2: 1 кΩ или 10 кΩ — уточнить мультиметром RIS↔GND на модуле.
// Стартовое значение 1000 Ом; калибровать амперметром при известной нагрузке.
float RIS_OHMS = 1000.0f;

// Нулевое смещение по СЫРОМУ току на STOP (до вычитания).
// Ошибка прошлой калибровки: брали уже обрезанный лог STOP — оффсет занизился.
// Сейчас: отображаемый STOP 0.12/0.15 при оффсетах 0.08/0.04 → сырой ≈ 0.20/0.19.
float I_OFFSET_1_A = 0.20f;
float I_OFFSET_2_A = 0.19f;

const int CURRENT_AVG_SAMPLES = 8;    // усреднение (ШИМ 2 кГц даёт шум на IS)
const float CURRENT_EMA_ALPHA = 0.25f; // сглаживание для лога/лимитов

// Мягкая защита по току (А). Stall мотора JGB37-520 ≈ 3.2–3.9 А.
const float I_SOFT_LIMIT_A = 3.5f;    // выше — пропорционально режем ШИМ
const float I_HARD_LIMIT_A = 5.0f;    // выше — полный стоп борта
const bool  CURRENT_LIMIT_ENABLE = true;

// =============================================================================
// Состояние энкодеров / управления
// =============================================================================
volatile int32_t encTicks1 = 0;
volatile int32_t encTicks2 = 0;
volatile uint8_t prevQuad1 = 0;
volatile uint8_t prevQuad2 = 0;

int moveTarget = 0;   // от ESP32: продольная команда
int turnTarget = 0;   // от ESP32: дифференциал поворота
String inputBuffer = "";

int32_t lastPos1 = 0;
int32_t lastPos2 = 0;
unsigned long lastControlMs = 0;
float integral1 = 0.0f;
float integral2 = 0.0f;
float speedEma1 = 0.0f;
float speedEma2 = 0.0f;
const float SPEED_EMA_ALPHA = 0.35f;  // сглаживание тиков — меньше квантования на малой скорости

float currentEma1 = 0.0f;  // сглаженный ток борта 1, А
float currentEma2 = 0.0f;

// Таблица квадратурного декодера: индекс = (prev<<2)|curr → шаг −1/0/+1
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
  // На BTS7960 активен только один канал ШИМ; второй удерживаем в 0.
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
  speedEma1 = 0.0f;
  speedEma2 = 0.0f;
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

// Ошибка и измерение — в тиках; к cmd добавляется только ограниченный trim.
// На малой скорости энкодер даёт 0–2 тика за 20 мс — это не «молчание», а реальное
// измерение; trim должен работать, иначе борта разъезжаются.
int speedTrim(int cmd, float measuredTicks, float &integral) {
  if (abs(cmd) <= CMD_DEADZONE) {
    integral = 0.0f;
    return 0;
  }
  if (MIN_TICKS_FOR_LOOP > 0 && abs(measuredTicks) < (float)MIN_TICKS_FOR_LOOP) {
    integral *= 0.5f;
    return 0;
  }

  float error = (float)cmd * CMD_TO_TICKS - measuredTicks;
  integral = constrain(integral + error, -INTEGRAL_MAX, INTEGRAL_MAX);
  return constrain((int)(Kp * error + Ki * integral), -CORRECTION_MAX, CORRECTION_MAX);
}

// АЦП на пине IS → ток мотора в амперах.
// Цепочка: raw(0…4095) → VIS(В) → IIS=VIS/RIS → IL=IIS*kILIS → минус оффсет нуля.
float readMotorCurrentA(int isPin, float offsetA) {
  // RIS задаётся калибровкой (обычно сотни–тысячи Ом). Значение <1 Ом —
  // явная ошибка конфигурации. Раньше здесь был return 0.0 — это маскировало
  // проблему под «тока нет». Правильнее зажать RIS снизу для безопасного деления
  // и один раз предупредить в Serial.
  float ris = RIS_OHMS;
  if (ris < 1.0f) {
    static bool warned = false;
    if (!warned) {
      Serial.println("WARN: RIS_OHMS < 1, используем 1 Ом (проверьте калибровку)");
      warned = true;
    }
    ris = 1.0f;
  }

  uint32_t sum = 0;
  for (int i = 0; i < CURRENT_AVG_SAMPLES; i++) {
    sum += analogRead(isPin);
  }
  float adcCounts = (float)sum / (float)CURRENT_AVG_SAMPLES;
  float vis = adcCounts * (ADC_VREF / (float)ADC_MAX);  // напряжение на sense-резисторе
  float amps = (vis / ris) * K_ILIS - offsetA;           // IL ≈ (VIS/RIS)*8500
  return (amps > 0.0f) ? amps : 0.0f;                    // ток физически не отрицательный
}

// Снижает |pwm|, если ток выше мягкого/жёсткого порога. Возвращает итоговый ШИМ.
int applyCurrentLimit(int pwm, float currentA) {
  if (!CURRENT_LIMIT_ENABLE) {
    return pwm;
  }
  if (currentA >= I_HARD_LIMIT_A) {
    return 0;
  }
  if (currentA <= I_SOFT_LIMIT_A) {
    return pwm;
  }
  // Линейно режем от soft до hard: на soft — 100%, на hard — 0%.
  float span = I_HARD_LIMIT_A - I_SOFT_LIMIT_A;
  float scale = 1.0f - (currentA - I_SOFT_LIMIT_A) / span;
  scale = constrain(scale, 0.0f, 1.0f);
  return (int)((float)pwm * scale);
}

void setup() {
  Serial.begin(115200);

  // UART к ESP32 (перекрёст: TX STM32 → RX ESP32)
  Serial1.setTx(PB6);
  Serial1.setRx(PB7);
  Serial1.begin(115200);

  delay(500);
  analogWriteFrequency(2000);   // 2 кГц — без слышимого писка обмоток
  analogReadResolution(12);     // 0…4095 на АЦП F411

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

  pinMode(IS_1, INPUT);
  pinMode(IS_2, INPUT);

  prevQuad1 = readQuad(ENC1_A, ENC1_B);
  prevQuad2 = readQuad(ENC2_A, ENC2_B);

  // Квадратура: прерывания по обеим фазам (4× разрешение относительно одного канала)
  attachInterrupt(digitalPinToInterrupt(ENC1_A), enc1A_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC1_B), enc1B_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC2_A), enc2A_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC2_B), enc2B_ISR, CHANGE);

  lastControlMs = millis();
  Serial.println("motor_controller: speed trim + IS current sense");
  Serial.println("Подключите R_IS+L_IS драйвера1→PA4, драйвера2→PA5 (общая GND)");
}

void loop() {
  // --- Приём команд от ESP32: "move,turn\n" ---
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

  // Arcade / tank-drive: дифференциал по бортам
  int cmd1 = constrain(moveTarget + turnTarget, -255, 255);
  int cmd2 = constrain(moveTarget - turnTarget, -255, 255);

  // Ток читаем всегда (и на стопе — для контроля утечек/шума нуля)
  float i1 = readMotorCurrentA(IS_1, I_OFFSET_1_A);
  float i2 = readMotorCurrentA(IS_2, I_OFFSET_2_A);
  currentEma1 = CURRENT_EMA_ALPHA * i1 + (1.0f - CURRENT_EMA_ALPHA) * currentEma1;
  currentEma2 = CURRENT_EMA_ALPHA * i2 + (1.0f - CURRENT_EMA_ALPHA) * currentEma2;

  if (abs(cmd1) <= CMD_DEADZONE && abs(cmd2) <= CMD_DEADZONE) {
    stopMotors();
    readSpeed(encTicks1, lastPos1);
    readSpeed(encTicks2, lastPos2);
    Serial.print("STOP I=");
    Serial.print(currentEma1, 2);
    Serial.print(',');
    Serial.println(currentEma2, 2);
    return;
  }

  int32_t speed1 = readSpeed(encTicks1, lastPos1);
  int32_t speed2 = readSpeed(encTicks2, lastPos2);
  speedEma1 += SPEED_EMA_ALPHA * ((float)speed1 - speedEma1);
  speedEma2 += SPEED_EMA_ALPHA * ((float)speed2 - speedEma2);

  int pwm1 = cmd1;
  int pwm2 = cmd2;
  if (SPEED_CLOSED_LOOP) {
    pwm1 = constrain(cmd1 + speedTrim(cmd1, speedEma1, integral1), -255, 255);
    pwm2 = constrain(cmd2 + speedTrim(cmd2, speedEma2, integral2), -255, 255);
  }

  pwm1 = applyCurrentLimit(pwm1, currentEma1);
  pwm2 = applyCurrentLimit(pwm2, currentEma2);

  setMotorPWM(RPWM_1, LPWM_1, pwm1);
  setMotorPWM(RPWM_2, LPWM_2, pwm2);

  noInterrupts();
  int32_t tot1 = encTicks1;
  int32_t tot2 = encTicks2;
  interrupts();

  // Лог: ток в амперах с одним знаком после запятой
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
  Serial.print(" I=");
  Serial.print(currentEma1, 2);
  Serial.print(',');
  Serial.print(currentEma2, 2);
  Serial.print(" pwm=");
  Serial.print(pwm1);
  Serial.print(',');
  Serial.println(pwm2);
}
