# ROS2 workspace (этап 1)

Пакеты: `tank_teleop`, `tank_bringup`.  
Архитектура, две сети, agent, карта Joy: **`schema/ros2_stage1.md`**.

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

Ethernet — домашний LAN (SSH `192.168.1.160`).  
wlan0 — AP `HeavyTank_10KG`, IP **`192.168.4.2`**, **без** default route через танк.

Команды: раздел «Две сети на opi5» в `schema/ros2_stage1.md`.

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
