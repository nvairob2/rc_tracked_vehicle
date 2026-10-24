# ROS2 этап 1 — телеуправление (Wi‑Fi / micro-ROS)

> Прошивка ESP32: `sources/ESP32/vehicle_controller_ros2/`  
> Воркспейс opi5: `ros2_ws/`  
> STM32: протокол без изменений — `"move,turn\n"` (−255…255).

---

## 1. Поток данных

```text
PS2 ──► ESP32-S3 ──UART──► STM32 ──► BTS7960 ×2
              │
              ├── Wi‑Fi AP 192.168.4.1 (веб-пульт)
              └── micro-ROS UDP ──► opi5:192.168.4.2:8888
                                        │
                                   tank_teleop
                                        │
                                   /cmd_vel ──► ESP32 (режим ROS2)
```

## 2. Сеть

| Узел | Адрес |
| :--- | :--- |
| ESP32 AP `HeavyTank_10KG` | `192.168.4.1` |
| Пароль AP | `password123` |
| opi5 (статический) | `192.168.4.2` |
| micro-ros-agent | UDP `8888` |

На opi5 задать статический IP в сети танка, затем:

```bash
# установка agent (один раз, нативный или docker — см. ниже)
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888

# ноды
cd /path/to/rc_tracked_vehicle/ros2_ws
source /opt/ros/$ROS_DISTRO/setup.bash
colcon build --symlink-install
source install/setup.bash
ros2 launch tank_bringup teleop.launch.py
```

Дистрибутив: **Humble** (Ubuntu 22.04) или **Jazzy** (24.04) — по образу opi5.

## 3. Режимы ESP32 (Select)

| Режим | Геймпад | `/cmd_vel` | UART STM32 |
| :--- | :--- | :--- | :--- |
| **Direct** (старт) | микшер на ESP32 | игнорируется | сразу `move,turn` |
| **ROS2** | только `/joy` | обязателен | только из Twist; тишина **200 мс** → `0,0` |

**Start** — защёлка стопа: `baseSpeed = 0` и UART `0,0`. В ROS2 микшер обрабатывает Start **до** веб-оверлея и держит стоп, пока оверлей не снят.

Веб-пульт `http://192.168.4.1/`:

- Удержание кнопки шлёт команду, отпускание / уход пальца — `0,0`.
- Direct: как раньше, сразу UART.
- ROS2: команды уходят в `/joy` как оси `axes[4]` (move) и `axes[5]` (turn), −1…1; микшер opi5 пропускает их как готовый arcade-mix. Нулевые оси снимают оверлей.

## 4. Топики

| Топик | Тип | Пишет | Читает |
| :--- | :--- | :--- | :--- |
| `/joy` | `sensor_msgs/Joy` | ESP32 ~20 мс | `tank_teleop` |
| `/cmd_vel` | `geometry_msgs/Twist` | `tank_teleop` 50 Гц | ESP32 |
| `/tank/mode` | `std_msgs/String` | ESP32 (`direct` / `ros2`) | Foxglove |
| `/tank/feedback` | `std_msgs/String` | ESP32 (эхо UART STM32) | отладка |

### Масштаб Twist

```text
linear.x  ∈ [−1; 1]  →  move = x * 255
angular.z ∈ [−1; 1]  →  turn = z * 255
```

На STM32 по-прежнему: `cmd1 = move + turn`, `cmd2 = move − turn`.

## 5. Карта sensor_msgs/Joy

Оси ROS: **вперёд = отрицательная** вертикаль.

| Индекс | Ось | Источник PS2 (0…255) |
| :---: | :--- | :--- |
| 0 | LX | PSS_LX |
| 1 | LY | PSS_LY → `axis = (raw - 128) / 127` |
| 2 | RX | PSS_RX |
| 3 | RY | PSS_RY → то же |
| 4 | web_move | веб-пульт, −1…1 (0 если нет) |
| 5 | web_turn | веб-пульт, −1…1 |

| Индекс | Кнопка |
| :---: | :--- |
| 0 | Cross |
| 1 | Circle |
| 2 | Square |
| 3 | Triangle |
| 4 | L1 |
| 5 | R1 |
| 6 | L2 |
| 7 | R2 |
| 8 | Select |
| 9 | Start |
| 10 | L3 |
| 11 | R3 |
| 12 | Pad Up |
| 13 | Pad Down |
| 14 | Pad Left |
| 15 | Pad Right |

Конвертация PS2 raw → ROS axis: `axis = (raw - 128) / 127`, clamp [−1; 1].  
Обратно: `raw = 128 + axis * 127`.

## 6. Микшер (совпадает Direct ESP32 и `tank_teleop`)

Константы: шаг 10, deadzone стика 12, pivot min 80, hold L1/R1 каждые 80 мс.

Логика — как в `vehicle_controller_ps2.ino` / `tank_teleop.mixer`.

## 7. Чеклист стенда

См. `schema/ros2_stage1_bench.md`.
