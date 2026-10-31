# ROS2 этап 2 — лидар D500, SLAM, Nav2, Webots

> История решения: `history/2026-10-31-ros2-stage2-nav-webots.md`  
> Этап 1 (геймпад): `schema/ros2_stage1.md`

## 1. Железо лидара

| Параметр | Значение |
| :--- | :--- |
| Модель | LDROBOT D500 Kit = STL-19P (в ПО: LDS19) |
| Интерфейс | UART 230400 → USB CP2102 → opi5 |
| Питание | 5 В с UBEC линии моторов (не с GPIO ESP32) |
| Скан | 10 Гц, ~5000 точек/с, до 12 м |
| Драйвер ROS2 | `ldlidar_node` (`lib/ldrobot_d500/ldrobot-lidar-ros2-main`) |

**Не** гонять сырой UART/LaserScan через ESP32 Wi‑Fi на этом этапе.

## 2. Поток на стенде

```text
D500 ──USB──► opi5 ── ldlidar_node ── /ldlidar_node/scan
                      │
                 slam_toolbox ── /map , TF map→odom
                      │
                    Nav2 ── /cmd_vel
                      │
         (режим ROS2) ESP32 ──UART──► STM32
```

Пока opi5 не на раме: USB-кабель со стенда до Pi; тележка неподвижна или в Direct/ROS2 по геймпаду. Навигация в движении — после монтажа SBC или длинного USB.

## 3. Симуляция Webots

| Что | Где |
| :--- | :--- |
| Мир `tank_room.wbt` | открыть в Webots на Mac |
| PROTO гусеничной аппроксимации | дифференциал + Lidar |
| ROS2 bridge | `ros2 launch tank_sim webots.launch.py` на Ubuntu/opi5 |

## 4. Запуск (opi5)

```bash
# лидар (порт уточнить: ls /dev/serial/by-id/)
ros2 launch tank_nav lidar.launch.py serial_port:=/dev/ttyUSB0

# картографирование
ros2 launch tank_nav slam.launch.py

# навигация по уже сохранённой карте
ros2 launch tank_nav nav.launch.py map:=/path/to/map.yaml
```

## 5. TF

```text
map → odom → base_link → ldlidar_base → ldlidar_link
```

`base_link` — центр корпуса. `ldlidar_base` — основание датчика на крыше.

Одометрия пока **открытая** из `/cmd_vel` (`cmd_vel_odom.py`), пока энкодеры STM32 не публикуют `/odom`. Для картографии на стенде этого достаточно при аккуратном движении; для точной навигации позже заменим на wheel odometry.

## 6. Чеклист

`schema/ros2_stage2_bench.md`.
