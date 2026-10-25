# 2026-10-25 — Переключение сетей opi5

Ethernet (`enP3p49s0`, домашний LAN) не трогаем: SSH с Mac остаётся.

`scripts/opi5_net.sh`:

- `tank` — wlan0 на AP `HeavyTank_10KG`, статический `192.168.4.2/24`, `ipv4.never-default yes` (default route не через ESP32).
- `home` — wlan0 обратно на сохранённый домашний профиль (по умолчанию `Cudy-0C20-5G`, DHCP). Имя пишется в `~/.config/rc_tracked_vehicle/home-wifi` перед уходом на танк. У профиля танка `autoconnect no`, чтобы после возврата AP не перехватывал Wi‑Fi.
- `status` — активный Wi‑Fi и default route.

С Mac: `./scripts/opi5_net.sh tank` (скрипт сам ходит по SSH на `opi5`).
