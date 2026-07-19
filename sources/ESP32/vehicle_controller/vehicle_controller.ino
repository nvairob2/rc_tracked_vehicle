#include <WiFi.h>
#include <WebServer.h>

// Настройки нативного UART для вашей платы
#define RX_PIN 44
#define TX_PIN 43

// Создаем Wi-Fi сеть нашего танка
const char* ssid = "HeavyTank_10KG";
const char* password = "password123";

WebServer server(80);

// HTML-код пульта управления (откроется в браузере на телефоне)
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

// Обработчик нажатий кнопок на сайте
void handleControl() {
  if (server.hasArg("move") && server.hasArg("turn")) {
    String moveVal = server.arg("move");
    String turnVal = server.arg("turn");
    
    // МГНОВЕННО пересылаем команду по UART в STM32
    Serial1.print(moveVal + "," + turnVal + "\n");
    
    // Дублируем в отладку на Mac
    Serial.printf("С сайта отправлено на STM32 -> Move:%s Turn:%s\n", moveVal.c_str(), turnVal.c_str());
  }
  server.send(200, "text/plain", "OK");
}

void handleRoot() {
  server.send(200, "text/html", html_page);
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // Запускаем точку доступа Wi-Fi
  WiFi.softAP(ssid, password);
  
  // Привязываем адреса сайта к функциям кода
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  
  server.begin();
  Serial.println("Wi-Fi сеть танка запущена! SSID: HeavyTank_10KG IP: 192.168.4.1");
}

void loop() {
  server.handleClient(); // Постоянно обрабатываем запросы от сайта
}
