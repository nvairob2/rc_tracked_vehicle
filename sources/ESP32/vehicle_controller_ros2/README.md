# vehicle_controller_ros2

Прошивка ESP32-S3 этапа 1: геймпад + Wi‑Fi + micro-ROS UDP.  
Схема: `schema/ros2_stage1.md`.

## Arduino IDE

1. Плата: **ESP32S3 Dev Module**, USB CDC On Boot — как у рабочего PS2-скетча.
2. Библиотека **micro_ros_arduino** (zip с [github.com/micro-ROS/micro_ros_arduino](https://github.com/micro-ROS/micro_ros_arduino)): Sketch → Include Library → Add .ZIP.
3. Открыть **всю папку** `vehicle_controller_ros2` (нужны `PS2X_lib.*` рядом).
4. `AGENT_IP` = `192.168.4.2`, порт `8888`.

## Кнопки

| Кнопка | Действие |
| :--- | :--- |
| Select | Direct ↔ ROS2 |
| Start | стоп: круиз 0 и UART `0,0` |

## Режимы

- **Direct** — микшер на ESP32, UART сразу.
- **ROS2** — `/joy` на opi5, UART только из `/cmd_vel`, watchdog 200 мс.
