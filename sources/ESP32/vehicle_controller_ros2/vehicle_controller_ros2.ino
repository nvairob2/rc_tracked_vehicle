/*
 * ESP32-S3: геймпад + Wi-Fi + micro-ROS (этап 1).
 * Select — Direct/ROS2, Start — стоп. UART к STM32: "move,turn\n".
 */
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>
#include <sensor_msgs/msg/joy.h>
#include <geometry_msgs/msg/twist.h>
#include <std_msgs/msg/string.h>
#include <math.h>
#include <uxr/client/transport.h>
#include "PS2X_lib.h"

#if ARDUINO_USB_CDC_ON_BOOT
#define LOG Serial
#else
#define LOG USBSerial
#endif

// UART к STM32 (протокол "move,turn\n")
#define RX_PIN 44
#define TX_PIN 43

// Приёмник PS2 на ESP32-S3
#define PS2_CLK  7
#define PS2_CMD  5
#define PS2_ATT  6
#define PS2_DATA 4

const char* WIFI_SSID = "HeavyTank_10KG";
const char* WIFI_PASS = "password123";
IPAddress AGENT_IP(192, 168, 4, 2);  // opi5 в сети AP
const uint16_t AGENT_PORT = 8888;

const int BASE_SPEED_STEP = 10;
const int STICK_DEADZONE = 12;
const int PIVOT_SPEED_MIN = 80;
const unsigned long GAMEPAD_POLL_MS = 20;
const unsigned long GAMEPAD_RETRY_MS = 2000;
const unsigned long SPEED_HOLD_MS = 80;
const unsigned long CMD_VEL_TIMEOUT_MS = 200;

// Select: Direct (микшер здесь) ↔ ROS2 (только /joy, UART из /cmd_vel)
enum ControlMode { MODE_DIRECT = 0, MODE_ROS2 = 1 };
volatile ControlMode controlMode = MODE_DIRECT;

const int JOY_AXES = 6;
const int JOY_BTNS = 16;
const int BTN_CROSS = 0;
const int BTN_CIRCLE = 1;
const int BTN_SQUARE = 2;
const int BTN_TRIANGLE = 3;
const int BTN_L1 = 4;
const int BTN_R1 = 5;
const int BTN_L2 = 6;
const int BTN_R2 = 7;
const int BTN_SELECT = 8;
const int BTN_START = 9;
const int BTN_L3 = 10;
const int BTN_R3 = 11;
const int BTN_PAD_UP = 12;
const int BTN_PAD_DOWN = 13;
const int BTN_PAD_LEFT = 14;
const int BTN_PAD_RIGHT = 15;

struct PadState {
  uint8_t lx, ly, rx, ry;
  int32_t buttons[JOY_BTNS];
  bool ok;
};

PadState padNow;
PadState padPrev;
portMUX_TYPE padMux = portMUX_INITIALIZER_UNLOCKED;

PS2X ps2x;
bool gamepadOk = false;
byte lastPs2InitErr = 255;
int baseSpeed = 0;
unsigned long lastHoldStepMs = 0;
int lastSentMove = 0;
int lastSentTurn = 0;
int lastLoggedMove = 0x7FFF;
int lastLoggedTurn = 0x7FFF;
int prevSelect = 0;
int prevStart = 0;

float webMove = 0.0f;
float webTurn = 0.0f;

int rosMove = 0;
int rosTurn = 0;
unsigned long lastCmdVelMs = 0;
bool haveCmdVel = false;

WebServer server(80);
WiFiUDP urosUdp;

rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rcl_publisher_t joyPub;
rcl_publisher_t modePub;
rcl_publisher_t feedbackPub;
rcl_subscription_t cmdSub;
rclc_executor_t executor;
sensor_msgs__msg__Joy joyMsg;
geometry_msgs__msg__Twist cmdMsg;
std_msgs__msg__String modeMsg;
std_msgs__msg__String feedbackMsg;
float joyAxes[JOY_AXES];
int32_t joyButtons[JOY_BTNS];
char modeBuf[16];
char feedbackBuf[160];
char joyFrameBuf[8];
bool urosReady = false;
enum { UROS_WAIT, UROS_CREATE, UROS_OK } urosState = UROS_WAIT;

const String html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>
    <title>Tank Control</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background: #222; color: #fff; margin: 0; padding: 20px; user-select: none; }
        h2 { color: #ff9900; }
        .btn { width: 80%; max-width: 300px; padding: 25px; margin: 10px; font-size: 24px; font-weight: bold; background: #444; color: #fff; border: 3px solid #ff9900; border-radius: 15px; cursor: pointer; touch-action: none; }
        .btn:active { background: #ff9900; color: #000; }
        .stop { border-color: #ff3333; background: #552222; }
    </style>
</head>
<body>
    <h2>Tank 10 KG</h2>
    <button class="btn" onpointerdown="holdCmd(event,150,0)" onpointerup="releaseCmd(event)" onpointercancel="releaseCmd(event)" onpointerleave="releaseCmd(event)">FORWARD</button><br>
    <button class="btn" onpointerdown="holdCmd(event,120,-80)" onpointerup="releaseCmd(event)" onpointercancel="releaseCmd(event)" onpointerleave="releaseCmd(event)">LEFT</button>
    <button class="btn" onpointerdown="holdCmd(event,120,80)" onpointerup="releaseCmd(event)" onpointercancel="releaseCmd(event)" onpointerleave="releaseCmd(event)">RIGHT</button><br>
    <button class="btn" onpointerdown="holdCmd(event,-150,0)" onpointerup="releaseCmd(event)" onpointercancel="releaseCmd(event)" onpointerleave="releaseCmd(event)">REVERSE</button><br>
    <button class="btn stop" onpointerdown="releaseCmd(event)" onpointerup="releaseCmd(event)" onpointercancel="releaseCmd(event)">STOP</button>
    <script>
        // Удержание = команда, отпускание/уход = стоп (иначе webMove залипает в ROS2)
        function holdCmd(ev, move, turn) {
            if (ev.cancelable) ev.preventDefault();
            sendCmd(move, turn);
        }
        function releaseCmd(ev) {
            if (ev && ev.cancelable) ev.preventDefault();
            sendCmd(0, 0);
        }
        function sendCmd(move, turn) {
            fetch('/control?move=' + move + '&turn=' + turn);
        }
    </script>
</body>
</html>
)rawliteral";

// Ограничение команды моторов −255…255
int clampCmd(int v) {
  if (v > 255) return 255;
  if (v < -255) return -255;
  return v;
}

int centerStick(int raw) {
  if (abs(raw - 128) <= STICK_DEADZONE) return 128;
  return raw;
}

void bumpBaseSpeed(int delta) {
  baseSpeed = clampCmd(baseSpeed + delta);
}

int stickDirect(int stickVal) {
  stickVal = centerStick(stickVal);
  if (stickVal < 128) return (128 - stickVal) * 255 / 128;
  if (stickVal > 128) return -(stickVal - 128) * 255 / 127;
  return 0;
}

int stickCruise(int stickVal, int base) {
  stickVal = centerStick(stickVal);
  if (base > 0) {
    if (stickVal < 128) return base + (255 - base) * (128 - stickVal) / 128;
    if (stickVal > 128) return base * (255 - stickVal) / 127;
    return base;
  }
  if (stickVal > 128) return base + (-255 - base) * (stickVal - 128) / 127;
  if (stickVal < 128) return base * stickVal / 128;
  return base;
}

int mapStickY(int stickVal, int base) {
  base = clampCmd(base);
  if (base == 0) return stickDirect(stickVal);
  return stickCruise(stickVal, base);
}

bool btnEdge(const int32_t* nowB, const int32_t* prevB, int idx) {
  return nowB[idx] != 0 && prevB[idx] == 0;
}

// Микшер как в vehicle_controller_ps2 (круиз, L1/R1, разворот)
void mixPad(const PadState& now, const PadState& prev, unsigned long nowMs, int& move, int& turn) {
  if (btnEdge(now.buttons, prev.buttons, BTN_L2) || btnEdge(now.buttons, prev.buttons, BTN_R2)) {
    baseSpeed = 0;
  }
  if (btnEdge(now.buttons, prev.buttons, BTN_PAD_UP) || btnEdge(now.buttons, prev.buttons, BTN_TRIANGLE)) {
    bumpBaseSpeed(BASE_SPEED_STEP);
  }
  if (btnEdge(now.buttons, prev.buttons, BTN_PAD_DOWN) || btnEdge(now.buttons, prev.buttons, BTN_CROSS)) {
    bumpBaseSpeed(-BASE_SPEED_STEP);
  }

  bool l1 = now.buttons[BTN_L1] != 0;
  bool r1 = now.buttons[BTN_R1] != 0;
  if (l1 != r1) {
    if (lastHoldStepMs == 0 || (nowMs - lastHoldStepMs) >= SPEED_HOLD_MS) {
      lastHoldStepMs = nowMs;
      bumpBaseSpeed(l1 ? BASE_SPEED_STEP : -BASE_SPEED_STEP);
    }
  } else {
    lastHoldStepMs = 0;
  }

  int ly = centerStick(now.ly);
  int ry = centerStick(now.ry);
  bool padLeft = now.buttons[BTN_PAD_LEFT] != 0;
  bool padRight = now.buttons[BTN_PAD_RIGHT] != 0;
  if (padLeft != padRight) {
    int pivot = max(abs(baseSpeed), PIVOT_SPEED_MIN);
    move = 0;
    turn = padRight ? pivot : -pivot;
  } else {
    int leftCmd = clampCmd(mapStickY(ly, baseSpeed));
    int rightCmd = clampCmd(mapStickY(ry, baseSpeed));
    move = (leftCmd + rightCmd) / 2;
    turn = (leftCmd - rightCmd) / 2;
  }
}

// Отправка команды на STM32
void sendMotorCmd(int move, int turn, const char* source) {
  move = clampCmd(move);
  turn = clampCmd(turn);
  Serial1.printf("%d,%d\n", move, turn);
  lastSentMove = move;
  lastSentTurn = turn;
  if (move != lastLoggedMove || turn != lastLoggedTurn) {
    LOG.printf("[%s] UART %d,%d mode=%s\n", source, move, turn,
               controlMode == MODE_ROS2 ? "ros2" : "direct");
    lastLoggedMove = move;
    lastLoggedTurn = turn;
  }
}

// Start: круиз 0 и стоп UART
void latchStop(const char* source) {
  baseSpeed = 0;
  webMove = 0;
  webTurn = 0;
  haveCmdVel = false;
  sendMotorCmd(0, 0, source);
}

float ps2ToAxis(uint8_t raw) {
  if (raw == 0) return -1.0f;
  if (raw == 255) return 1.0f;
  return (float)((int)raw - 128) / 127.0f;
}

void fillJoyFromPad(const PadState& p) {
  joyAxes[0] = ps2ToAxis(p.lx);
  joyAxes[1] = ps2ToAxis(p.ly);
  joyAxes[2] = ps2ToAxis(p.rx);
  joyAxes[3] = ps2ToAxis(p.ry);
  joyAxes[4] = webMove;
  joyAxes[5] = webTurn;
  for (int i = 0; i < JOY_BTNS; i++) {
    joyButtons[i] = p.buttons[i];
  }
}

void handleRoot() {
  server.send(200, "text/html", html_page);
}

// Веб: Direct → UART; ROS2 → оси Joy. 0,0 снимает оверлей (отпускание кнопки).
void handleControl() {
  if (!server.hasArg("move") || !server.hasArg("turn")) {
    server.send(400, "text/plain", "need move,turn");
    return;
  }
  int move = server.arg("move").toInt();
  int turn = server.arg("turn").toInt();
  if (controlMode == MODE_DIRECT) {
    sendMotorCmd(move, turn, "WiFi");
  } else {
    webMove = constrain((float)move / 255.0f, -1.0f, 1.0f);
    webTurn = constrain((float)turn / 255.0f, -1.0f, 1.0f);
  }
  server.send(200, "text/plain", "OK");
}

// Кастомный UDP-транспорт: ESP32 остаётся AP, не STA
bool udpOpen(uxrCustomTransport* /*t*/) {
  return urosUdp.begin(AGENT_PORT) == 1;
}

bool udpClose(uxrCustomTransport* /*t*/) {
  urosUdp.stop();
  return true;
}

size_t udpWrite(uxrCustomTransport* /*t*/, const uint8_t* buf, size_t len, uint8_t* err) {
  if (!urosUdp.beginPacket(AGENT_IP, AGENT_PORT)) {
    *err = 1;
    return 0;
  }
  size_t n = urosUdp.write(buf, len);
  if (!urosUdp.endPacket()) {
    *err = 1;
    return 0;
  }
  *err = 0;
  return n;
}

size_t udpRead(uxrCustomTransport* /*t*/, uint8_t* buf, size_t len, int timeout, uint8_t* err) {
  uint32_t start = millis();
  while (urosUdp.parsePacket() <= 0) {
    if ((millis() - start) > (uint32_t)timeout) {
      *err = 1;
      return 0;
    }
    delay(1);
  }
  int n = urosUdp.read(buf, len);
  *err = 0;
  return n < 0 ? 0 : (size_t)n;
}

// /cmd_vel: linear.x / angular.z ∈ [−1; 1] → move/turn * 255
void cmdVelCb(const void* msgin) {
  const geometry_msgs__msg__Twist* msg = (const geometry_msgs__msg__Twist*)msgin;
  rosMove = clampCmd((int)lroundf(msg->linear.x * 255.0f));
  rosTurn = clampCmd((int)lroundf(msg->angular.z * 255.0f));
  lastCmdVelMs = millis();
  haveCmdVel = true;
}

bool createUrosEntities() {
  allocator = rcl_get_default_allocator();
  rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
  if (rcl_init_options_init(&init_options, allocator) != RCL_RET_OK) return false;
  if (rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator) != RCL_RET_OK) {
    return false;
  }
  if (rclc_node_init_default(&node, "esp32_tank", "", &support) != RCL_RET_OK) return false;

  joyMsg.axes.data = joyAxes;
  joyMsg.axes.size = JOY_AXES;
  joyMsg.axes.capacity = JOY_AXES;
  joyMsg.buttons.data = joyButtons;
  joyMsg.buttons.size = JOY_BTNS;
  joyMsg.buttons.capacity = JOY_BTNS;
  joyMsg.header.frame_id.data = joyFrameBuf;
  joyMsg.header.frame_id.capacity = sizeof(joyFrameBuf);
  joyMsg.header.frame_id.size = 0;

  modeMsg.data.data = modeBuf;
  modeMsg.data.capacity = sizeof(modeBuf);
  feedbackMsg.data.data = feedbackBuf;
  feedbackMsg.data.capacity = sizeof(feedbackBuf);

  if (rclc_publisher_init_default(&joyPub, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Joy), "joy") != RCL_RET_OK) return false;
  if (rclc_publisher_init_default(&modePub, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String), "tank/mode") != RCL_RET_OK) return false;
  if (rclc_publisher_init_default(&feedbackPub, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String), "tank/feedback") != RCL_RET_OK) return false;
  if (rclc_subscription_init_default(&cmdSub, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "cmd_vel") != RCL_RET_OK) return false;
  if (rclc_executor_init(&executor, &support.context, 1, &allocator) != RCL_RET_OK) return false;
  if (rclc_executor_add_subscription(&executor, &cmdSub, &cmdMsg, &cmdVelCb, ON_NEW_DATA) != RCL_RET_OK) {
    return false;
  }
  return true;
}

void destroyUrosEntities() {
  rclc_executor_fini(&executor);
  rcl_subscription_fini(&cmdSub, &node);
  rcl_publisher_fini(&feedbackPub, &node);
  rcl_publisher_fini(&modePub, &node);
  rcl_publisher_fini(&joyPub, &node);
  rcl_node_fini(&node);
  rclc_support_fini(&support);
  urosReady = false;
  urosState = UROS_WAIT;
}

void spinUros() {
  if (urosState == UROS_WAIT) {
    if (rmw_uros_ping_agent(200, 1) == RMW_RET_OK) {
      urosState = UROS_CREATE;
      LOG.println("micro-ROS: agent ping OK");
    }
    return;
  }
  if (urosState == UROS_CREATE) {
    if (createUrosEntities()) {
      urosReady = true;
      urosState = UROS_OK;
      LOG.println("micro-ROS: connected");
    } else {
      LOG.println("micro-ROS: create failed, retry");
      urosState = UROS_WAIT;
    }
    return;
  }
  rclc_executor_spin_some(&executor, RCL_MS_TO_NS(5));
  if (rmw_uros_ping_agent(50, 1) != RMW_RET_OK) {
    LOG.println("micro-ROS: agent lost");
    destroyUrosEntities();
  }
}

void publishRos(const PadState& p) {
  if (!urosReady) return;
  fillJoyFromPad(p);
  rcl_publish(&joyPub, &joyMsg, NULL);

  const char* mode = (controlMode == MODE_ROS2) ? "ros2" : "direct";
  size_t n = strlen(mode);
  memcpy(modeBuf, mode, n);
  modeBuf[n] = 0;
  modeMsg.data.size = n;
  rcl_publish(&modePub, &modeMsg, NULL);

  if (Serial1.available() > 0) {
    int i = 0;
    while (Serial1.available() > 0 && i < (int)sizeof(feedbackBuf) - 1) {
      char c = (char)Serial1.read();
      if (c == '\n') break;
      feedbackBuf[i++] = c;
    }
    feedbackBuf[i] = 0;
    feedbackMsg.data.size = i;
    if (i > 0) {
      rcl_publish(&feedbackPub, &feedbackMsg, NULL);
    }
  }
}

void readPs2Into(PadState& out) {
  memset(&out, 0, sizeof(out));
  out.lx = 128;
  out.ly = 128;
  out.rx = 128;
  out.ry = 128;
  if (!gamepadOk) return;
  if (!ps2x.read_gamepad(false, 0)) return;
  out.ok = true;
  out.lx = ps2x.Analog(PSS_LX);
  out.ly = ps2x.Analog(PSS_LY);
  out.rx = ps2x.Analog(PSS_RX);
  out.ry = ps2x.Analog(PSS_RY);
  out.buttons[BTN_CROSS] = ps2x.Button(PSB_CROSS) ? 1 : 0;
  out.buttons[BTN_CIRCLE] = ps2x.Button(PSB_CIRCLE) ? 1 : 0;
  out.buttons[BTN_SQUARE] = ps2x.Button(PSB_SQUARE) ? 1 : 0;
  out.buttons[BTN_TRIANGLE] = ps2x.Button(PSB_TRIANGLE) ? 1 : 0;
  out.buttons[BTN_L1] = ps2x.Button(PSB_L1) ? 1 : 0;
  out.buttons[BTN_R1] = ps2x.Button(PSB_R1) ? 1 : 0;
  out.buttons[BTN_L2] = ps2x.Button(PSB_L2) ? 1 : 0;
  out.buttons[BTN_R2] = ps2x.Button(PSB_R2) ? 1 : 0;
  out.buttons[BTN_SELECT] = ps2x.Button(PSB_SELECT) ? 1 : 0;
  out.buttons[BTN_START] = ps2x.Button(PSB_START) ? 1 : 0;
  out.buttons[BTN_L3] = ps2x.Button(PSB_L3) ? 1 : 0;
  out.buttons[BTN_R3] = ps2x.Button(PSB_R3) ? 1 : 0;
  out.buttons[BTN_PAD_UP] = ps2x.Button(PSB_PAD_UP) ? 1 : 0;
  out.buttons[BTN_PAD_DOWN] = ps2x.Button(PSB_PAD_DOWN) ? 1 : 0;
  out.buttons[BTN_PAD_LEFT] = ps2x.Button(PSB_PAD_LEFT) ? 1 : 0;
  out.buttons[BTN_PAD_RIGHT] = ps2x.Button(PSB_PAD_RIGHT) ? 1 : 0;
}

void initGamepad() {
  delay(100);
  lastPs2InitErr = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_ATT, PS2_DATA, false, false);
  if (lastPs2InitErr == 0) {
    gamepadOk = true;
    LOG.println("PS2: OK");
    return;
  }
  delay(200);
  lastPs2InitErr = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_ATT, PS2_DATA, true, false);
  gamepadOk = (lastPs2InitErr == 0);
  LOG.printf("PS2: init=%d\n", lastPs2InitErr);
}

void ps2Task(void* /*arg*/) {
  unsigned long lastRetry = 0;
  unsigned long lastPoll = 0;
  for (;;) {
    unsigned long now = millis();
    if (!gamepadOk && (now - lastRetry >= GAMEPAD_RETRY_MS)) {
      lastRetry = now;
      initGamepad();
    }
    if (gamepadOk && (now - lastPoll >= GAMEPAD_POLL_MS)) {
      lastPoll = now;
      PadState snap;
      readPs2Into(snap);
      portENTER_CRITICAL(&padMux);
      padNow = snap;
      portEXIT_CRITICAL(&padMux);
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// Select, Start, Direct-микшер или UART из Twist + watchdog 200 мс
void applyModeAndDrive(const PadState& now, const PadState& prev) {
  if (btnEdge(now.buttons, prev.buttons, BTN_SELECT)) {
    controlMode = (controlMode == MODE_DIRECT) ? MODE_ROS2 : MODE_DIRECT;
    haveCmdVel = false;
    LOG.printf("mode -> %s\n", controlMode == MODE_ROS2 ? "ros2" : "direct");
    sendMotorCmd(0, 0, "MODE");
  }
  if (btnEdge(now.buttons, prev.buttons, BTN_START)) {
    latchStop("START");
    return;
  }

  if (controlMode == MODE_DIRECT) {
    int move = 0;
    int turn = 0;
    mixPad(now, prev, millis(), move, turn);
    if (move != lastSentMove || turn != lastSentTurn) {
      sendMotorCmd(move, turn, "GP");
    }
    return;
  }

  if (!haveCmdVel || (millis() - lastCmdVelMs) > CMD_VEL_TIMEOUT_MS) {
    if (lastSentMove != 0 || lastSentTurn != 0) {
      sendMotorCmd(0, 0, "WD");
    }
    return;
  }
  if (rosMove != lastSentMove || rosTurn != lastSentTurn) {
    sendMotorCmd(rosMove, rosTurn, "ROS2");
  }
}

void setup() {
  LOG.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  memset(&padNow, 0, sizeof(padNow));
  memset(&padPrev, 0, sizeof(padPrev));
  padNow.ly = padNow.ry = padNow.lx = padNow.rx = 128;
  padPrev = padNow;

  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.begin();
  LOG.println("Wi-Fi AP 192.168.4.1  Select=Direct/ROS2  Start=STOP");

  rmw_uros_set_custom_transport(false, NULL, udpOpen, udpClose, udpWrite, udpRead);

  xTaskCreatePinnedToCore(ps2Task, "ps2", 4096, NULL, 1, NULL, 1);
}

void loop() {
  server.handleClient();
  spinUros();

  PadState now;
  portENTER_CRITICAL(&padMux);
  now = padNow;
  portEXIT_CRITICAL(&padMux);

  applyModeAndDrive(now, padPrev);
  publishRos(now);
  padPrev = now;
  delay(5);
}
