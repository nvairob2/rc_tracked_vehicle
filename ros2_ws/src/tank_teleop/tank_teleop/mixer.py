"""Микшер геймпада — копия логики vehicle_controller_ps2.ino."""

from __future__ import annotations

from tank_teleop.joy_map import JoyMap, clamp, ros_axis_to_ps2


class TankMixer:
    BASE_SPEED_STEP = 10
    STICK_DEADZONE = 12
    PIVOT_SPEED_MIN = 80
    SPEED_HOLD_MS = 80

    def __init__(self) -> None:
        self.base_speed = 0
        self._prev_buttons = [0] * JoyMap.BUTTON_COUNT
        self._last_hold_ms = 0
        self._hold_armed = False
        self._web_estop = False

    def reset(self) -> None:
        self.base_speed = 0
        self._prev_buttons = [0] * JoyMap.BUTTON_COUNT
        self._last_hold_ms = 0
        self._hold_armed = False
        self._web_estop = False

    def update(
        self,
        now_ms: int,
        axes: list[float],
        buttons: list[int],
    ) -> tuple[int, int]:
        """Вернуть (move, turn) в диапазоне −255…255."""
        axes = _pad_float(axes, JoyMap.AXIS_COUNT)
        buttons = _pad_int(buttons, JoyMap.BUTTON_COUNT)

        web_move = axes[JoyMap.AX_WEB_MOVE]
        web_turn = axes[JoyMap.AX_WEB_TURN]
        web_active = abs(web_move) > 0.01 or abs(web_turn) > 0.01
        if not web_active:
            self._web_estop = False

        # Start — стоп во всех режимах, в том числе поверх веб-оверлея.
        if buttons[JoyMap.BTN_START] != 0:
            self.base_speed = 0
            self._web_estop = True
            self._prev_buttons = list(buttons)
            return 0, 0

        if web_active and not self._web_estop:
            move = int(round(clamp(web_move, -1.0, 1.0) * 255.0))
            turn = int(round(clamp(web_turn, -1.0, 1.0) * 255.0))
            self._prev_buttons = list(buttons)
            return move, turn

        if self._pressed(buttons, JoyMap.BTN_L2) or self._pressed(buttons, JoyMap.BTN_R2):
            self.base_speed = 0

        if self._pressed(buttons, JoyMap.BTN_PAD_UP) or self._pressed(buttons, JoyMap.BTN_TRIANGLE):
            self._bump(self.BASE_SPEED_STEP)
        if self._pressed(buttons, JoyMap.BTN_PAD_DOWN) or self._pressed(buttons, JoyMap.BTN_CROSS):
            self._bump(-self.BASE_SPEED_STEP)

        l1 = buttons[JoyMap.BTN_L1] != 0
        r1 = buttons[JoyMap.BTN_R1] != 0
        if l1 != r1:
            if not self._hold_armed or (now_ms - self._last_hold_ms) >= self.SPEED_HOLD_MS:
                self._last_hold_ms = now_ms
                self._hold_armed = True
                self._bump(self.BASE_SPEED_STEP if l1 else -self.BASE_SPEED_STEP)
        else:
            self._hold_armed = False
            self._last_hold_ms = 0

        ly = self._center_stick(ros_axis_to_ps2(axes[JoyMap.AX_LY]))
        ry = self._center_stick(ros_axis_to_ps2(axes[JoyMap.AX_RY]))

        pad_left = buttons[JoyMap.BTN_PAD_LEFT] != 0
        pad_right = buttons[JoyMap.BTN_PAD_RIGHT] != 0
        if pad_left != pad_right:
            pivot = max(abs(self.base_speed), self.PIVOT_SPEED_MIN)
            move = 0
            turn = pivot if pad_right else -pivot
        else:
            left_cmd = _clamp_cmd(self._map_stick_y(ly, self.base_speed))
            right_cmd = _clamp_cmd(self._map_stick_y(ry, self.base_speed))
            move = (left_cmd + right_cmd) // 2
            turn = (left_cmd - right_cmd) // 2

        self._prev_buttons = list(buttons)
        return _clamp_cmd(move), _clamp_cmd(turn)

    def _pressed(self, buttons: list[int], index: int) -> bool:
        return buttons[index] != 0 and self._prev_buttons[index] == 0

    def _bump(self, delta: int) -> None:
        self.base_speed = _clamp_cmd(self.base_speed + delta)

    def _center_stick(self, raw: int) -> int:
        if abs(raw - 128) <= self.STICK_DEADZONE:
            return 128
        return raw

    def _map_stick_y(self, stick_val: int, base: int) -> int:
        base = _clamp_cmd(base)
        if base == 0:
            return self._stick_direct(stick_val)
        return self._stick_cruise(stick_val, base)

    def _stick_direct(self, stick_val: int) -> int:
        stick_val = self._center_stick(stick_val)
        if stick_val < 128:
            return (128 - stick_val) * 255 // 128
        if stick_val > 128:
            return -(stick_val - 128) * 255 // 127
        return 0

    def _stick_cruise(self, stick_val: int, base: int) -> int:
        stick_val = self._center_stick(stick_val)
        if base > 0:
            if stick_val < 128:
                return base + (255 - base) * (128 - stick_val) // 128
            if stick_val > 128:
                return base * (255 - stick_val) // 127
            return base
        if stick_val > 128:
            return base + (-255 - base) * (stick_val - 128) // 127
        if stick_val < 128:
            return base * stick_val // 128
        return base


def _clamp_cmd(value: int) -> int:
    return int(clamp(value, -255, 255))


def _pad_float(values: list[float], size: int) -> list[float]:
    out = [0.0] * size
    for i, value in enumerate(values[:size]):
        out[i] = float(value)
    return out


def _pad_int(values: list[int], size: int) -> list[int]:
    out = [0] * size
    for i, value in enumerate(values[:size]):
        out[i] = int(value)
    return out
