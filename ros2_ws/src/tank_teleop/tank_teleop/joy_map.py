"""Индексы sensor_msgs/Joy — совпадают с schema/ros2_stage1.md."""


class JoyMap:
    AX_LX = 0
    AX_LY = 1
    AX_RX = 2
    AX_RY = 3
    AX_WEB_MOVE = 4
    AX_WEB_TURN = 5

    BTN_CROSS = 0
    BTN_CIRCLE = 1
    BTN_SQUARE = 2
    BTN_TRIANGLE = 3
    BTN_L1 = 4
    BTN_R1 = 5
    BTN_L2 = 6
    BTN_R2 = 7
    BTN_SELECT = 8
    BTN_START = 9
    BTN_L3 = 10
    BTN_R3 = 11
    BTN_PAD_UP = 12
    BTN_PAD_DOWN = 13
    BTN_PAD_LEFT = 14
    BTN_PAD_RIGHT = 15

    AXIS_COUNT = 6
    BUTTON_COUNT = 16


def clamp(value: float, lo: float, hi: float) -> float:
    return lo if value < lo else hi if value > hi else value


def ros_axis_to_ps2(axis: float) -> int:
    """ROS вертикаль: вперёд = −1. PS2 raw: вперёд = 0, центр = 128, назад = 255."""
    axis = clamp(float(axis), -1.0, 1.0)
    if axis <= -1.0:
        return 0
    if axis >= 1.0:
        return 255
    return int(round(128.0 + axis * 127.0))


def ps2_to_ros_axis(raw: int) -> float:
    return clamp((int(raw) - 128) / 127.0, -1.0, 1.0)


def cmd_to_twist(move: int, turn: int) -> tuple[float, float]:
    """move/turn −255…255 → Twist linear.x / angular.z."""
    return clamp(move / 255.0, -1.0, 1.0), clamp(turn / 255.0, -1.0, 1.0)


def twist_to_cmd(linear_x: float, angular_z: float) -> tuple[int, int]:
    move = int(round(clamp(linear_x, -1.0, 1.0) * 255.0))
    turn = int(round(clamp(angular_z, -1.0, 1.0) * 255.0))
    return move, turn
