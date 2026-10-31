# ROS2 workspace (этап 1)

Пакеты этапа 1: `tank_teleop`, `tank_bringup` — **`schema/ros2_stage1.md`**.  
Пакеты этапа 2: `tank_description`, `tank_nav`, `tank_sim`, `ldlidar_*` — **`schema/ros2_stage2.md`**.

Путь на opi5: `~/WORK/Projects/rc_tracked_vehicle/ros2_ws/`.  
Humble: `/opt/ros/humble`.

## Включить / выключить стек

Не tmux: сессия из чужого SSH умирает при отключении. Юнит пользователя `nik`:

```bash
systemctl --user start tank-bringup     # включить
systemctl --user stop tank-bringup      # выключить
systemctl --user restart tank-bringup
systemctl --user status tank-bringup
journalctl --user -u tank-bringup -f    # лог launch + agent + teleop
```

Файл юнита: `scripts/tank-bringup.service` → на Pi `~/.config/systemd/user/tank-bringup.service`.  
Автозагрузки при включении питания нет.

После `start` в графе должны быть `/tank_teleop` и топики `/joy`, `/cmd_vel`. Нода `/esp32_tank` — только когда ESP32 в режиме **ROS2** (Select) и wlan0 в сети танка.

```bash
source /opt/ros/humble/setup.bash
source ~/WORK/Projects/rc_tracked_vehicle/ros2_ws/install/setup.bash
ros2 node list
ros2 topic echo /tank/mode
```

Launch вручную (если юнит не используете):

```bash
ros2 launch tank_bringup teleop.launch.py
# без Docker: agent_via_docker:=false
```

## Две сети

Ethernet — домашний LAN (SSH `192.168.1.160`), скрипт его не трогает.  
wlan0 — либо AP танка, либо домашний Wi‑Fi:

```bash
./scripts/opi5_net.sh tank     # 192.168.4.2, без default через ESP32
./scripts/opi5_net.sh home     # обратно на Cudy (или последний домашний SSID)
./scripts/opi5_net.sh status
```

Подробности: `schema/ros2_stage1.md` §2.

## Сборка и выкладка с Mac

```bash
./scripts/sync_opi5.sh
ssh opi5 'systemctl --user restart tank-bringup'
```

Agent в apt нет — launch по умолчанию: Docker `microros/micro-ros-agent:humble` с `--network=host` (зачем: `schema/ros2_stage1.md` §4).

## Тесты микшера без ROS

```bash
cd ros2_ws/src/tank_teleop
python3 -m pytest test -q
```

## Foxglove

Layout: `src/tank_bringup/config/foxglove.json` — `/joy`, `/cmd_vel`, `/tank/mode`, `/tank/feedback`.

## Этап 2 (лидар / SLAM / Nav2)

Зависимости на opi5:

```bash
sudo apt install libudev-dev \
  ros-humble-slam-toolbox ros-humble-navigation2 ros-humble-nav2-bringup \
  ros-humble-robot-state-publisher ros-humble-xacro ros-humble-tf2-ros
# лидар: пакеты из symlink lib/ldrobot_d500 → src/ldlidar*
```

```bash
./scripts/sync_opi5.sh
# порт: ls /dev/serial/by-id/
ros2 launch tank_nav lidar.launch.py
# топик драйвера (не /scan):
ros2 topic hz /ldlidar_node/scan
ros2 topic echo /ldlidar_node/scan --once

ros2 launch tank_nav slam.launch.py
# сохранить карту: ros2 run nav2_map_server map_saver_cli -f ~/maps/room

ros2 launch tank_nav nav.launch.py map:=$HOME/maps/room.yaml
```

Webots (мир на Mac): `src/tank_sim/worlds/tank_room.wbt` — см. `src/tank_sim/README.md`.
