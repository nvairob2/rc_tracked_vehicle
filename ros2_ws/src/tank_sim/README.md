# tank_sim — Webots

## Только визуализация (Mac)

1. Установить [Webots](https://cyberbotics.com/) (есть сборка для Apple Silicon).
2. File → Open →  
   `ros2_ws/src/tank_sim/worlds/tank_room.wbt`
3. В мире: комната 8×8 м, препятствия, PROTO `TrackedTank` с лидаром «D500».

Контроллер `<extern>` — для ROS2. Без ROS робот стоит; скан лидара в Webots виден через View → Optional Rendering → Show Lidar Point Cloud (если доступно) или окно Lidar.

## ROS2 + Webots (opi5 / Ubuntu Humble)

```bash
sudo apt install ros-humble-webots-ros2
# Webots тоже должен быть установлен в системе
source /opt/ros/humble/setup.bash
source ~/WORK/Projects/rc_tracked_vehicle/ros2_ws/install/setup.bash
ros2 launch tank_sim webots.launch.py
```

Дальше в другом терминале: `ros2 launch tank_nav slam.launch.py` с `use_sim_time:=true` (потребуется донастройка параметра в mapper yaml).
