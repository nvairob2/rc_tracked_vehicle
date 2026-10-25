# Чеклист стенда — ROS2 этап 1

Прошивки: ESP32 `vehicle_controller_ros2`, STM32 `motor_controller` без смены протокола.

## 0. Подготовка

- [ ] ESP32: `vehicle_controller_ros2`, USB CDC On Boot Enabled (прошивка с Mac по USB — по желанию).
- [ ] opi5: Ethernet в домашний LAN (SSH), wlan0 → `HeavyTank_10KG`, IP **192.168.4.2**, default **не** через танк (`schema/ros2_stage1.md` §2).
- [ ] `ping 192.168.4.1` с Pi проходит.
- [ ] `systemctl --user start tank-bringup` → `status` active.
- [ ] `ros2 node list`: есть `/tank_teleop`. `/esp32_tank` — после Select.

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
