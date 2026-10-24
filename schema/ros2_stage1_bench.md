# Чеклист стенда — ROS2 этап 1

Прошивки: ESP32 `vehicle_controller_ros2`, STM32 `motor_controller` без смены протокола.

## 0. Подготовка

- [ ] ESP32: библиотека `micro_ros_arduino`, скетч из папки `vehicle_controller_ros2`.
- [ ] opi5 в Wi‑Fi `HeavyTank_10KG`, IP **192.168.4.2**.
- [ ] `colcon build` в `ros2_ws`, `source install/setup.bash`.
- [ ] `ros2 launch tank_bringup teleop.launch.py` (или agent отдельно + `start_agent:=false`).
- [ ] Serial ESP32 115200: `Wi-Fi AP 192.168.4.1`.

## 1. Direct (по умолчанию)

- [ ] Геймпад: ↑ задаёт круиз, стики, ←/→ разворот — моторы как в PS2-скетче.
- [ ] Веб `http://192.168.4.1/` FORWARD/STOP работает.
- [ ] В логе ESP32: `mode=direct`, строки `[GP]` / `[WiFi]`.
- [ ] `/cmd_vel` на opi5 **не** двигает машину (режим Direct).

## 2. Переключение ROS2

- [ ] Select: в Serial `mode -> ros2`, UART `0,0`.
- [ ] `ros2 topic echo /tank/mode` → `ros2`.
- [ ] `ros2 topic echo /joy` — оси/кнопки при движении стиков.
- [ ] `ros2 topic echo /cmd_vel` — `tank_teleop` публикует 50 Гц.
- [ ] Движение только когда teleop запущен и `/cmd_vel` живой.
- [ ] Веб в ROS2: FORWARD появляется в `/joy` axes[4], затем в `/cmd_vel`.

## 3. Watchdog (обрыв Wi‑Fi)

- [ ] Режим ROS2, машина едет.
- [ ] Выключить Wi‑Fi на opi5 **или** убить `tank_teleop` / agent.
- [ ] За **≤ 200 мс** UART `0,0`, в логе ESP32 `[WD]`.
- [ ] Select обратно в Direct — управление снова локальное.

## 4. Start

- [ ] Start: круиз 0 и стоп, в любом режиме.

## 5. Foxglove (опционально)

- [ ] Layout `ros2_ws/src/tank_bringup/config/foxglove.json`.
- [ ] Видны `/joy`, `/cmd_vel`, `/tank/mode`, `/tank/feedback`.
