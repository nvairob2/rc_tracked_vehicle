# Autonomous tracked vehicle with radio control.

## ROS2 этап 1

Телеуправление по Wi‑Fi (micro-ROS): геймпад на ESP32, микшер на `opi5`.

- Контракт топиков и сеть: `schema/ros2_stage1.md`
- Чеклист стенда: `schema/ros2_stage1_bench.md`
- Прошивка ESP32: `sources/ESP32/vehicle_controller_ros2/`
- Пакеты: `ros2_ws/` (`tank_teleop`, `tank_bringup`)
- Решение: `history/2026-10-11-ros2-stage1.md`

## ROS2 этап 2

Лидар D500 (USB→opi5), SLAM, Nav2, симуляция Webots.

- Схема: `schema/ros2_stage2.md`
- Пакеты: `tank_description`, `tank_nav`, `tank_sim` (+ symlink `ldlidar_*`)
- Webots мир: `ros2_ws/src/tank_sim/worlds/tank_room.wbt`
- Решение: `history/2026-10-31-ros2-stage2-nav-webots.md`

## Hardware

### Vehicle

#### Motor controller

WeAct Black Pill V2.0 (STM32F411CEU6)