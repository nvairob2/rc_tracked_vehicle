# tank_sim — Webots

## Только визуализация (Mac)

1. Установить [Webots](https://cyberbotics.com/) **R2025a** (или новее; мир объявлен как R2025a).
2. File → Open →  
   `ros2_ws/src/tank_sim/worlds/tank_room.wbt`  
   При первом открытии Webots скачает PROTO фона/арены с GitHub (нужен интернет).
3. В мире: комната 8×8 м, препятствия, PROTO `TrackedTank` с лидаром «D500».  
   Координаты **ENU** (Z вверх) — как в Webots R2025a.

Контроллер `<extern>` — для ROS2. Без ROS робот стоит. Скан: окно робота → D500 → Enable, либо View → Optional Rendering → Show Lidar Rays / Point Cloud.

## ROS2 + Webots (opi5 / Ubuntu Humble)

```bash
sudo apt install ros-humble-webots-ros2
# Webots тоже должен быть установлен в системе
source /opt/ros/humble/setup.bash
source ~/WORK/Projects/rc_tracked_vehicle/ros2_ws/install/setup.bash
ros2 launch tank_sim webots.launch.py
```

Дальше в другом терминале: `ros2 launch tank_nav slam.launch.py` с `use_sim_time:=true` (потребуется донастройка параметра в mapper yaml).
