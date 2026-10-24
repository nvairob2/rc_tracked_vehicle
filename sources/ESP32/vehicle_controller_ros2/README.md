# vehicle_controller_ros2

Прошивка ESP32-S3 этапа 1: геймпад + Wi‑Fi + micro-ROS UDP.  
Схема: `schema/ros2_stage1.md`.

## Arduino IDE

1. Плата: **ESP32S3 Dev Module**. Рекомендуется **USB CDC On Boot = Enabled** (логи в USB Monitor, UART к STM32 остаётся `Serial1` на GPIO43/44). На ядре ESP32 3.x при CDC Off объекта `USBSerial` нет.
2. Библиотека **micro_ros_arduino** в Library Manager **нет**. Скачать ZIP с [Releases](https://github.com/micro-ROS/micro_ros_arduino/releases):
   - Humble на opi5 → **`v2.0.8-humble`**
   - Jazzy на opi5 → **`v2.0.8-jazzy`**
   Файл вида `micro_ros_arduino.zip` (не Source code).
3. Arduino IDE (на Mac): **Sketch → Include Library → Add .ZIP Library…** → этот ZIP. Полностью закрыть и снова открыть IDE.
   Библиотека должна лежать здесь:
   `/Users/<you>/Documents/Arduino/libraries/micro_ros_arduino/src/micro_ros_arduino.h`
   Если «Add .ZIP» ничего не положил — распаковать ZIP вручную в `Documents/Arduino/libraries/` (внутри ровно одна папка `micro_ros_arduino`, не вложенная дважды).
   M4 не требует отдельной сборки micro-ROS: в ZIP уже бинарники **для ESP32**, хост только кросс-компилирует скетч (как PS2-прошивку).
4. Arduino ищет `src/esp32s3/libmicroros.a`. Если пишет `Precompiled library .../esp32s3 not found`:
   ```bash
   cp -R ~/Documents/Arduino/libraries/micro_ros_arduino/src/esp32 \
        ~/Documents/Arduino/libraries/micro_ros_arduino/src/esp32s3
   ```
   Не ставьте релиз **Iron** (`3.0.0-iron`) — нужен Humble/Jazzy, как на opi5.
5. Открыть **всю папку** `vehicle_controller_ros2` (нужны `PS2X_lib.*` рядом).
6. `AGENT_IP` = `192.168.4.2`, порт `8888`.

## Кнопки

| Кнопка | Действие |
| :--- | :--- |
| Select | Direct ↔ ROS2 |
| Start | стоп: круиз 0 и UART `0,0` |

## Режимы

- **Direct** — микшер на ESP32, UART сразу.
- **ROS2** — `/joy` на opi5, UART только из `/cmd_vel`, watchdog 200 мс.
