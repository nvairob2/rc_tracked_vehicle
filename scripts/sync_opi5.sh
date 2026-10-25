#!/usr/bin/env bash
# Синхронизация репозитория на opi5 и colcon build.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${OPI5_DEST:-opi5:~/WORK/Projects/rc_tracked_vehicle/}"

rsync -az --delete \
  --exclude '.idea/' \
  --exclude 'ros2_ws/build/' \
  --exclude 'ros2_ws/install/' \
  --exclude 'ros2_ws/log/' \
  --exclude '__pycache__/' \
  --exclude '.pytest_cache/' \
  --exclude '.DS_Store' \
  "$ROOT/" "$DEST"

ssh -o BatchMode=yes opi5 'bash -lc "
  source /opt/ros/humble/setup.bash
  cd ~/WORK/Projects/rc_tracked_vehicle/ros2_ws
  colcon build --symlink-install
"'
echo "OK. Перезапуск стека: ssh opi5 'systemctl --user restart tank-bringup'"
