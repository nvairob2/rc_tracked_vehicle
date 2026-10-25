# ROS2 этап 1 — телеуправление (Wi‑Fi / micro-ROS)

> Прошивка ESP32: `sources/ESP32/vehicle_controller_ros2/`  
> Воркспейс opi5: `~/WORK/Projects/rc_tracked_vehicle/ros2_ws/`  
> STM32: протокол без изменений — `"move,turn\n"` (−255…255).  
> Операции (старт/стоп): `ros2_ws/README.md`.

Pi **не** стоит на раме и **не** соединён проводом с ESP32. Управление в режиме ROS2: геймпад → ESP32 → Wi‑Fi → opi5 → снова Wi‑Fi → ESP32 → UART → STM32.

---

## 1. Поток данных

```text
Геймпад PS2 ──► ESP32-S3 ──UART "move,turn"──► STM32 ──► BTS7960 ×2
                    │
                    ├── Wi-Fi AP 192.168.4.1  (веб-пульт)
                    └── micro-ROS UDP :8888
                              │
                    opi5 wlan0 192.168.4.2
                              │
                    micro-ros-agent (Docker)
                              │
                         топики ROS2
                              │
                         tank_teleop
                              │
                         /cmd_vel ──► ESP32 (только режим ROS2)
```

Пока на ESP32 режим **Direct**, Pi в движении не участвует.

---

## 2. Две сети на opi5 одновременно

Нужны **два независимых интерфейса**. Один кабель в домашний роутер (SSH, интернет, Cursor). Второй — Wi‑Fi **клиента** к точке доступа ESP32 (только micro-ROS).

Типичная схема (стенд 2026-10-25):

| Интерфейс | Сеть | Адрес | Зачем |
| :--- | :--- | :--- | :--- |
| Ethernet `enP3p49s0` | домашний LAN (Cudy) | `192.168.1.160/24` | SSH с Mac (`Host opi5`), apt, Docker Hub |
| Wi‑Fi `wlan0` | AP танка `HeavyTank_10KG` | **`192.168.4.2/24`** | UDP к ESP32 `192.168.4.1:8888` |
| Default route | через `192.168.1.1` на Ethernet | — | интернет **не** через танк |

Mac в этой схеме в домашней сети (например `192.168.1.108`). К USB танка Mac не обязан быть подключён.

### Зачем `never-default`

ESP32 раздаёт DHCP и часто ставит себя шлюзом `192.168.4.1`. Если Pi возьмёт **default route** через танк, пропадёт интернет и иногда SSH «через Wi‑Fi». SSH по Ethernet обычно живёт, но `apt`/`docker pull` сломаются.

Правило: в сети танка только префикс `192.168.4.0/24`, шлюз по умолчанию — домашний роутер.

### Команды (Ethernet не трогают)

С Mac или на Pi:

```bash
./scripts/opi5_net.sh tank     # wlan0 → HeavyTank_10KG, 192.168.4.2, без default через танк
./scripts/opi5_net.sh home     # wlan0 обратно на домашний Wi‑Fi (DHCP)
./scripts/opi5_net.sh status
```

`home` поднимает профиль, который был на wlan0 до `tank` (файл `~/.config/rc_tracked_vehicle/home-wifi`, по умолчанию `Cudy-0C20-5G`). У AP танка после `home` выключается autoconnect, чтобы он не перехватывал Wi‑Fi, пока танк включён.

Проверка:

```bash
ip -br addr                 # wlan0 = 192.168.4.2, eth = 192.168.1.160
ip route                    # default только via 192.168.1.1 dev enP3p49s0
                            # плюс 192.168.4.0/24 dev wlan0
ping -c 2 192.168.4.1       # ESP32
ping -c 2 192.168.1.1       # домашний роутер
ssh с Mac: ssh opi5         # на 192.168.1.160
```

Ethernet **не отключать**, когда сажаете wlan0 на танк: иначе потеряете SSH с Mac.

Если wlan0 уже был на домашнем SSID (`Cudy-0C20-5G`) — после `wifi connect HeavyTank_10KG` он уйдёт с домашнего Wi‑Fi. Это нормально: LAN остаётся на кабеле.

---

## 3. Сеть танка (адреса)

| Узел | Адрес |
| :--- | :--- |
| ESP32 AP `HeavyTank_10KG` | `192.168.4.1` |
| Пароль AP | `password123` |
| opi5 wlan0 (статический) | `192.168.4.2` |
| micro-ros-agent | UDP `8888` на всех интерфейсах Pi (`--network=host`) |
| Прошивка ESP32 | `AGENT_IP = 192.168.4.2` |

Дистрибутив на opi5: **Humble** (Ubuntu 22.04).

---

## 4. Что такое micro-ROS agent и зачем Docker

### Зачем agent вообще

На Pi стоит обычный ROS 2 Humble: ноды общаются **DDS** (Fast DDS и т.п.). На ESP32 полного ROS 2 нет: там **micro-ROS** — урезанный клиент протокола **XRCE-DDS**.

Они говорят на разных протоколах. **micro-ros-agent** — прокси:

1. С MCU: XRCE-DDS по UDP (`192.168.4.1` ↔ `192.168.4.2:8888`).
2. С ROS 2 на Pi: обычные топики `/joy`, `/cmd_vel`, `/tank/mode`, …

Без agent команды `ros2 topic list` **не увидят** ESP32. Agent сам по себе не микширует и не едет: только мост.

В графе ROS это не отдельная «прикладная» нода вроде `tank_teleop`, а процесс-мост. После рукопожатия с прошивкой появляется клиентская нода **`/esp32_tank`**.

### Зачем образ `microros/micro-ros-agent:humble`

В Ubuntu-репозитории этой платы пакета `ros-humble-micro-ros-agent` **нет**. Официальный путь — контейнер той же версии, что Humble на Pi:

```text
docker run --rm --network=host microros/micro-ros-agent:humble udp4 --port 8888
```

- `humble` — совпадение дистрибутива с `/opt/ros/humble` (на ESP32 лучше та же линейка, zip `v2.0.8-humble`, не Iron).
- `--network=host` — контейнер слушает UDP **8888 на хосте**, в том числе на `192.168.4.2`. Без host-сети пакеты с ESP32 в контейнер не попадут.
- Launch `tank_bringup` поднимает этот контейнер сам (`agent_via_docker:=true` по умолчанию).

Нативный agent (если когда-нибудь появится в apt):  
`ros2 launch tank_bringup teleop.launch.py agent_via_docker:=false`.

---

## 5. Ноды ROS 2 на opi5

| Нода | Где код | Что делает |
| :--- | :--- | :--- |
| **`/tank_teleop`** | `ros2_ws` пакет `tank_teleop`, Python lifecycle | Подписка `/joy` → микшер (круиз, стики, L1/R1, разворот, веб-оси, Start) → публикация `/cmd_vel` **50 Гц**. Если `/joy` нет или штамп слишком старый — публикует нулевой Twist (стоп). |
| **`/esp32_tank`** | прошивка `vehicle_controller_ros2` через agent | Появляется **только в режиме ROS2** (Select), когда ping agent успешен. Публикует `/joy`, `/tank/mode`, `/tank/feedback`. Подписывается на `/cmd_vel`. |
| Launch **`tank_bringup`** | `ros2_ws/src/tank_bringup` | Не нода, а сборка: Docker agent + `tank_teleop`. |

Пакет `tank_bringup` сам по себе ничего не считает — только `teleop.launch.py`.

Жизненный цикл `tank_teleop`: при старте сам делает configure → activate (стенд без лишней launch-логики).

---

## 6. Режимы ESP32 (Select)

| Режим | Геймпад | `/cmd_vel` | UART STM32 |
| :--- | :--- | :--- | :--- |
| **Direct** (старт) | микшер на ESP32 | игнорируется | сразу `move,turn` |
| **ROS2** | только `/joy` | обязателен | только из Twist; тишина **200 мс** → `0,0` |

В Direct прошивка **не** крутит micro-ROS в горячем цикле (иначе задержка геймпада на секунды без agent).

**Start** — защёлка стопа: `baseSpeed = 0` и UART `0,0`. В ROS2 микшер обрабатывает Start **до** веб-оверлея и держит стоп, пока оверлей не снят.

Веб-пульт `http://192.168.4.1/`:

- Удержание кнопки шлёт команду, отпускание / уход пальца — `0,0`.
- Direct: сразу UART.
- ROS2: `axes[4]` / `axes[5]` в `/joy`; микшер opi5 берёт их как готовый arcade-mix.

---

## 7. Топики

| Топик | Тип | Пишет | Читает |
| :--- | :--- | :--- | :--- |
| `/joy` | `sensor_msgs/Joy` | ESP32 ~20 мс | `tank_teleop` |
| `/cmd_vel` | `geometry_msgs/Twist` | `tank_teleop` 50 Гц | ESP32 |
| `/tank/mode` | `std_msgs/String` | ESP32 (`direct` / `ros2`) | Foxglove |
| `/tank/feedback` | `std_msgs/String` | ESP32 (эхо UART STM32) | отладка |

### Масштаб Twist

```text
linear.x  ∈ [−1; 1]  →  move = x * 255
angular.z ∈ [−1; 1]  →  turn = z * 255
```

На STM32: `cmd1 = move + turn`, `cmd2 = move − turn`.

---

## 8. Карта sensor_msgs/Joy

Оси ROS: **вперёд = отрицательная** вертикаль.

| Индекс | Ось | Источник PS2 (0…255) |
| :---: | :--- | :--- |
| 0 | LX | PSS_LX |
| 1 | LY | PSS_LY → `axis = (raw - 128) / 127` |
| 2 | RX | PSS_RX |
| 3 | RY | PSS_RY → то же |
| 4 | web_move | веб-пульт, −1…1 (0 если нет) |
| 5 | web_turn | веб-пульт, −1…1 |

| Индекс | Кнопка |
| :---: | :--- |
| 0 | Cross |
| 1 | Circle |
| 2 | Square |
| 3 | Triangle |
| 4 | L1 |
| 5 | R1 |
| 6 | L2 |
| 7 | R2 |
| 8 | Select |
| 9 | Start |
| 10 | L3 |
| 11 | R3 |
| 12 | Pad Up |
| 13 | Pad Down |
| 14 | Pad Left |
| 15 | Pad Right |

Конвертация: `axis = (raw - 128) / 127`, clamp [−1; 1]. Обратно: `raw = 128 + axis * 127`.

---

## 9. Микшер

Константы: шаг 10, deadzone стика 12, pivot min 80, hold L1/R1 каждые 80 мс.  
Логика — как в `vehicle_controller_ps2.ino` / `tank_teleop.mixer`.

---

## 10. Чеклист стенда

См. `schema/ros2_stage1_bench.md`.
