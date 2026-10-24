# ROS2 workspace (этап 1)

Пакеты: `tank_teleop`, `tank_bringup`. Контракт топиков: `schema/ros2_stage1.md`.

## Сборка на opi5

```bash
sudo apt install ros-${ROS_DISTRO}-micro-ros-agent  # если есть в репозитории
# иначе: docker run -it --rm --net=host microros/micro-ros-agent:humble udp4 --port 8888

cd ros2_ws
source /opt/ros/$ROS_DISTRO/setup.bash
colcon build --symlink-install
source install/setup.bash
```

Сеть: подключить opi5 к `HeavyTank_10KG`, IP `192.168.4.2`.

```bash
ros2 launch tank_bringup teleop.launch.py
```

Agent отдельно (если launch не находит пакет):

```bash
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888
ros2 launch tank_bringup teleop.launch.py start_agent:=false
```

## Тесты микшера без ROS

```bash
cd ros2_ws/src/tank_teleop
python3 -m pytest test -q
```

## Foxglove

Открыть `src/tank_bringup/config/foxglove.json` как layout, топики `/joy`, `/cmd_vel`, `/tank/mode`, `/tank/feedback`.
