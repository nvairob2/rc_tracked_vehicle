# 2026-10-31 — Этап 2: лидар D500, Nav2, Webots

## Решение

Лидар **LDROBOT D500 (STL-19P)** подключается к **opi5 по USB (CP2102)**, не через ESP32/Wi‑Fi: поток ~20 КБ/с формально мал, но джиттер Wi‑Fi и занятость ESP32 недопустимы для скана 10 Гц и SLAM.

ESP32 остаётся телеуправлением и UART к STM32. Автономный `/cmd_vel` от Nav2 уходит в ESP32 в режиме ROS2 (тот же Twist, что у `tank_teleop`).

## Пакеты

| Пакет | Назначение |
| :--- | :--- |
| `tank_description` | URDF/xacro: `base_link`, гусеницы (визуал), `ldlidar_base` |
| `tank_nav` | Launch SLAM + Nav2, параметры для гусеничной базы |
| `tank_sim` | Мир Webots + PROTO + launch `webots_ros2` |
| `ldlidar_*` | Vendor SDK из `lib/ldrobot_d500/…` (symlink в `ros2_ws/src`) |

## Топики этапа 2

| Топик | Тип | Источник |
| :--- | :--- | :--- |
| `/scan` | `sensor_msgs/LaserScan` | `ldlidar_node` / Webots Lidar |
| `/map` | `nav_msgs/OccupancyGrid` | `slam_toolbox` |
| `/cmd_vel` | `geometry_msgs/Twist` | Nav2 (или teleop) |
| TF | `map`→`odom`→`base_link`→`ldlidar_link` | slam + robot_state_publisher |

## Симуляция

Webots-мир открывается на Mac для визуальной проверки. Полный контур ROS2+Webots — на opi5/Ubuntu с пакетом `webots_ros2_driver` (Humble).
