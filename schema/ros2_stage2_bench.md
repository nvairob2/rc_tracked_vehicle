# Чеклист стенда — ROS2 этап 2 (лидар)

## 0. Железо

- [ ] D500 → плата CP2102 → USB opi5.
- [ ] Питание 5 В с UBEC (общий GND с логикой), не с пина ESP32.
- [ ] `ls /dev/serial/by-id/` или `/dev/ttyUSB0` виден на Pi.
- [ ] `sudo usermod -aG dialout $USER` и перелогин (доступ к serial).

## 1. Скан

- [ ] `colcon build` включает `ldlidar_*`, `tank_description`, `tank_nav`.
- [ ] `ros2 launch tank_nav lidar.launch.py`
- [ ] `ros2 topic hz /ldlidar_node/scan` ≈ 10 Гц.
- [ ] В RViz Fixed Frame `base_link` / `ldlidar_link` виден облако точек.

## 2. SLAM

- [ ] `ros2 launch tank_nav slam.launch.py`
- [ ] Движение тележки (геймпад ROS2) → `/map` заполняется.
- [ ] `ros2 run nav2_map_server map_saver_cli -f ~/maps/room`

## 3. Nav2

- [ ] `ros2 launch tank_nav nav.launch.py map:=$HOME/maps/room.yaml`
- [ ] В RViz: 2D Pose Estimate, затем Nav2 Goal.
- [ ] `/cmd_vel` появляется; в режиме ROS2 на ESP32 моторы реагируют (гусеницы на подставке!).

## 4. Webots

- [ ] На Mac: Open `ros2_ws/src/tank_sim/worlds/tank_room.wbt`, сцена без ошибок PROTO.
