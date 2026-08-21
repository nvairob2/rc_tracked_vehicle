#include <WiFi.h>
#include <WebServer.h>

// PS2-геймпад (см. schema/ps2_gamepad.md).
// Библиотека лежит рядом со скетчем: форк ESP32
// https://github.com/MyArduinoLib/Arduino-PS2X-ESP32
#define ENABLE_PS2_GAMEPAD 1

#if ENABLE_PS2_GAMEPAD
#include "PS2X_lib.h"
#endif

// Если USB CDC On Boot = Disabled, Serial — это UART0 (GPIO43/44, линия STM32).
// Логи тогда идут в USBSerial (нативный USB).
#if ARDUINO_USB_CDC_ON_BOOT
#define LOG Serial
#else
#define LOG USBSerial
#endif

// UART → STM32 (перекрёст: TX ESP32 → RX STM32)
#define RX_PIN 44
#define TX_PIN 43

#if ENABLE_PS2_GAMEPAD
#define PS2_CLK  7
#define PS2_CMD  5
#define PS2_ATT  6
#define PS2_DATA 4

const int BASE_SPEED_STEP = 10;
const int STICK_DEADZONE = 12;
const unsigned long GAMEPAD_POLL_MS = 20;
const unsigned long GAMEPAD_RETRY_MS = 2000;

PS2X ps2x;
int baseSpeed = 0;  // −255…255, задний ход через ↓
bool gamepadOk = false;
byte lastPs2InitErr = 255;
unsigned long lastGamepadPoll = 0;
unsigned long lastGamepadRetry = 0;
int lastSentMove = 0;
int lastSentTurn = 0;
int lastReadOk = 0;
byte lastControllerType = 0;
int lastLogBase = -1;
int lastLogLY = -1;
int lastLogRY = -1;
uint16_t lastLogButtons = 0xFFFF;
#endif

const char* ssid = "HeavyTank_10KG";
const char* password = "password123";

WebServer server(80);

const String html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>
    <title>Tank Control</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background: #222; color: #fff; margin: 0; padding: 20px; user-select: none; }
        h2 { color: #ff9900; }
        .btn { width: 80%; max-width: 300px; padding: 25px; margin: 10px; font-size: 24px; font-weight: bold; background: #444; color: #fff; border: 3px solid #ff9900; border-radius: 15px; cursor: pointer; }
        .btn:active { background: #ff9900; color: #000; }
        .stop { border-color: #ff3333; background: #552222; }
        .stop:active { background: #ff3333; }
    </style>
</head>
<body>
    <h2>🤖 Tank 10 KG: CONTROLLER</h2>
    <button class="btn" onmousedown="sendCmd(150,0)" ontouchstart="sendCmd(150,0)">FORWARD</button><br>
    <button class="btn" onmousedown="sendCmd(120,-80)" ontouchstart="sendCmd(120,-80)">LEFT</button>
    <button class="btn" onmousedown="sendCmd(120,80)" ontouchstart="sendCmd(120,80)">RIGHT</button><br>
    <button class="btn" onmousedown="sendCmd(-150,0)" ontouchstart="sendCmd(-150,0)">REVERSE</button><br>
    <button class="btn stop" onmousedown="sendCmd(0,0)" ontouchstart="sendCmd(0,0)">STOP (BRAKE)</button>

    <script>
        function sendCmd(move, turn) {
            fetch('/control?move=' + move + '&turn=' + turn);
        }
    </script>
</body>
</html>
)rawliteral";

void sendMotorCmd(int move, int turn, const char* source) {
  move = constrain(move, -255, 255);
  turn = constrain(turn, -255, 255);

  Serial1.printf("%d,%d\n", move, turn);

  static int lastLoggedMove = 0x7FFF;
  static int lastLoggedTurn = 0x7FFF;
  if (move != lastLoggedMove || turn != lastLoggedTurn) {
    LOG.printf("[%s] STM32 -> move:%d turn:%d\n", source, move, turn);
    lastLoggedMove = move;
    lastLoggedTurn = turn;
  }

#if ENABLE_PS2_GAMEPAD
  lastSentMove = move;
  lastSentTurn = turn;
#endif
}

void handleControl() {
  if (server.hasArg("move") && server.hasArg("turn")) {
    sendMotorCmd(server.arg("move").toInt(), server.arg("turn").toInt(), "WiFi");
  }
  server.send(200, "text/plain", "OK");
}

void handleRoot() {
  server.send(200, "text/html", html_page);
}

#if ENABLE_PS2_GAMEPAD

int centerStick(uint8_t raw) {
  if (abs((int)raw - 128) <= STICK_DEADZONE) {
    return 128;
  }
  return raw;
}

// Стик: вперёд → +255, центр → baseSpeed (−255…255), назад → −255.
int mapStickY(int stickVal, int base) {
  stickVal = centerStick((uint8_t)stickVal);
  base = constrain(base, -255, 255);
  if (stickVal < 128) {
    return base + (255 - base) * (128 - stickVal) / 128;
  }
  if (stickVal > 128) {
    return base + (-255 - base) * (stickVal - 128) / 127;
  }
  return base;
}

void pollGamepad() {
  lastReadOk = ps2x.read_gamepad(false, 0);
  if (!lastReadOk) {
    return;
  }

  if (ps2x.ButtonPressed(PSB_PAD_UP) || ps2x.ButtonPressed(PSB_TRIANGLE)) {
    baseSpeed = min(255, baseSpeed + BASE_SPEED_STEP);
  }
  if (ps2x.ButtonPressed(PSB_PAD_DOWN) || ps2x.ButtonPressed(PSB_CROSS)) {
    baseSpeed = max(-255, baseSpeed - BASE_SPEED_STEP);
  }

  int ly = centerStick(ps2x.Analog(PSS_LY));
  int ry = centerStick(ps2x.Analog(PSS_RY));

  int leftCmd;
  int rightCmd;

  if (ps2x.Button(PSB_L2) || ps2x.Button(PSB_R2)) {
    leftCmd = 0;
    rightCmd = 0;
  } else {
    leftCmd = constrain(mapStickY(ly, baseSpeed), -255, 255);
    rightCmd = constrain(mapStickY(ry, baseSpeed), -255, 255);
  }

  int move = (leftCmd + rightCmd) / 2;
  int turn = (leftCmd - rightCmd) / 2;

  bool changed = (move != lastSentMove) || (turn != lastSentTurn);
  if (changed) {
    sendMotorCmd(move, turn, "GP");
  }

  uint16_t buttons = ps2x.ButtonDataByte();
  if (baseSpeed != lastLogBase || ly != lastLogLY || ry != lastLogRY || buttons != lastLogButtons) {
    LOG.printf("GP base=%d LY=%d RY=%d L=%d R=%d UP=%d DN=%d L2=%d R2=%d\n",
               baseSpeed, ly, ry, leftCmd, rightCmd,
               ps2x.Button(PSB_PAD_UP) ? 1 : 0,
               ps2x.Button(PSB_PAD_DOWN) ? 1 : 0,
               ps2x.Button(PSB_L2) ? 1 : 0,
               ps2x.Button(PSB_R2) ? 1 : 0);
    lastLogBase = baseSpeed;
    lastLogLY = ly;
    lastLogRY = ry;
    lastLogButtons = buttons;
  }
}

void initGamepad() {
  delay(100);
  // Беспроводные клоны часто не принимают analog-pressures — сначала без них.
  lastPs2InitErr = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_ATT, PS2_DATA, false, false);
  if (lastPs2InitErr == 0) {
    gamepadOk = true;
    lastControllerType = ps2x.readType();
    LOG.printf("PS2: приёмник OK (GPIO DATA=4 CMD=5 ATT=6 CLK=7) type=%d\n", lastControllerType);
    return;
  }

  delay(200);
  lastPs2InitErr = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_ATT, PS2_DATA, true, false);
  if (lastPs2InitErr == 0) {
    gamepadOk = true;
    lastControllerType = ps2x.readType();
    LOG.printf("PS2: приёмник OK (analog pressures) type=%d\n", lastControllerType);
    return;
  }

  gamepadOk = false;
  LOG.printf(
      "PS2: init=%d (1=нет контроллера, 2=не принимает команды, 3=нет analog). "
      "Включи геймпад и подожди повтор.\n",
      lastPs2InitErr);
}

#endif

void setup() {
  LOG.begin(115200);

  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.begin();
  LOG.println("Wi-Fi: HeavyTank_10KG  http://192.168.4.1/");
}

void loop() {
  server.handleClient();

#if ENABLE_PS2_GAMEPAD
  unsigned long now = millis();

  if (!gamepadOk && (now - lastGamepadRetry >= GAMEPAD_RETRY_MS)) {
    lastGamepadRetry = now;
    initGamepad();
  }

  if (gamepadOk && (now - lastGamepadPoll >= GAMEPAD_POLL_MS)) {
    lastGamepadPoll = now;
    pollGamepad();
  }
#endif
}
