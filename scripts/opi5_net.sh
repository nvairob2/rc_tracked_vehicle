#!/usr/bin/env bash
# Переключение wlan0 на opi5. Ethernet не трогаем.
#   tank   — AP танка, 192.168.4.2, без default route через ESP32
#   home   — сохранённый домашний Wi‑Fi (DHCP)
#   status — кто на wlan0 и куда смотрит default
set -euo pipefail

TANK_SSID="${TANK_SSID:-HeavyTank_10KG}"
TANK_PASS="${TANK_PASS:-password123}"
TANK_CIDR="${TANK_CIDR:-192.168.4.2/24}"
TANK_PING="${TANK_PING:-192.168.4.1}"
DEFAULT_HOME_CON="${DEFAULT_HOME_CON:-Cudy-0C20-5G}"
STATE_DIR="${HOME}/.config/rc_tracked_vehicle"
HOME_CON_FILE="${STATE_DIR}/home-wifi"

if [[ "$(hostname)" != "orangepi5ultra" ]]; then
  exec ssh -o BatchMode=yes opi5 "bash -s -- $(printf '%q ' "$@")" < "$0"
fi

mkdir -p "$STATE_DIR"

active_wifi() {
  nmcli -t -f NAME,TYPE,DEVICE con show --active \
    | awk -F: '$2 == "802-11-wireless" && $3 == "wlan0" { print $1; exit }'
}

saved_home() {
  if [[ -s "$HOME_CON_FILE" ]]; then
    cat "$HOME_CON_FILE"
  else
    printf '%s\n' "$DEFAULT_HOME_CON"
  fi
}

remember_home() {
  local current
  current="$(active_wifi || true)"
  if [[ -n "$current" && "$current" != "$TANK_SSID" ]]; then
    printf '%s\n' "$current" > "$HOME_CON_FILE"
  elif [[ ! -s "$HOME_CON_FILE" ]]; then
    printf '%s\n' "$DEFAULT_HOME_CON" > "$HOME_CON_FILE"
  fi
}

require_profile() {
  local name="$1"
  if ! nmcli -t -f NAME con show | grep -qx "$name"; then
    echo "Нет профиля NetworkManager: $name" >&2
    exit 1
  fi
}

cmd_status() {
  echo "wlan0: $(active_wifi || echo '—')"
  echo "home : $(saved_home)"
  ip -br addr show wlan0 || true
  ip -br addr show enP3p49s0 || true
  echo "--- default ---"
  ip route show default || true
}

ssid_visible() {
  nmcli -t -f SSID dev wifi list ifname wlan0 | grep -qx "$TANK_SSID"
}

cmd_tank() {
  remember_home
  nmcli radio wifi on
  local try
  for try in 1 2 3; do
    nmcli dev wifi rescan ifname wlan0 || true
    sleep 2
    ssid_visible && break
    if [[ "$try" -eq 3 ]]; then
      echo "AP ${TANK_SSID} не виден. Танк включён?" >&2
      exit 1
    fi
  done
  # Пароль в команде: из SSH нет keyring, `con up` без него зависает.
  if ! nmcli -w 25 device wifi connect "$TANK_SSID" password "$TANK_PASS" ifname wlan0; then
    echo "Ассоциация с ${TANK_SSID} не удалась. Перезагрузите ESP32 и повторите." >&2
    exit 1
  fi
  nmcli con modify "$TANK_SSID" \
    ipv4.method manual \
    ipv4.addresses "$TANK_CIDR" \
    ipv4.gateway "" \
    ipv4.never-default yes \
    connection.autoconnect yes \
    connection.autoconnect-priority 100
  nmcli con modify "$(saved_home)" connection.autoconnect-priority 10 || true
  nmcli device reapply wlan0 || true
  echo "tank: wlan0 ${TANK_CIDR}, default не через танк"
  ip -br addr show wlan0
  ip route show default
  ping -c 1 -W 2 "$TANK_PING" >/dev/null && echo "ping ${TANK_PING} ok" || echo "ping ${TANK_PING} нет ответа"
}

cmd_home() {
  local home
  home="$(saved_home)"
  require_profile "$home"
  if nmcli -t -f NAME con show | grep -qx "$TANK_SSID"; then
    nmcli con modify "$TANK_SSID" connection.autoconnect no connection.autoconnect-priority -10 || true
    nmcli con down "$TANK_SSID" >/dev/null 2>&1 || true
  fi
  nmcli con modify "$home" connection.autoconnect yes connection.autoconnect-priority 50
  nmcli con up "$home" ifname wlan0
  echo "home: wlan0 -> ${home}"
  ip -br addr show wlan0
  ip route show default
}

case "${1:-status}" in
  tank) cmd_tank ;;
  home) cmd_home ;;
  status) cmd_status ;;
  *)
    echo "usage: $0 tank|home|status" >&2
    exit 2
    ;;
esac
