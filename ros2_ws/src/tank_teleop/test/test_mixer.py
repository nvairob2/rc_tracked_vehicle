import pytest

from tank_teleop.joy_map import JoyMap, cmd_to_twist, ps2_to_ros_axis, ros_axis_to_ps2
from tank_teleop.mixer import TankMixer


def _joy(ly=128, ry=128, pressed=None, held=None, web=None):
    axes = [0.0] * JoyMap.AXIS_COUNT
    axes[JoyMap.AX_LY] = ps2_to_ros_axis(ly)
    axes[JoyMap.AX_RY] = ps2_to_ros_axis(ry)
    if web is not None:
        axes[JoyMap.AX_WEB_MOVE] = web[0]
        axes[JoyMap.AX_WEB_TURN] = web[1]
    buttons = [0] * JoyMap.BUTTON_COUNT
    if held:
        for idx in held:
            buttons[idx] = 1
    mixer = TankMixer()
    if pressed:
        first = [0] * JoyMap.BUTTON_COUNT
        mixer.update(0, axes, first)
        for idx in pressed:
            buttons[idx] = 1
    return mixer, axes, buttons


def test_idle_center():
    mixer, axes, buttons = _joy()
    assert mixer.update(0, axes, buttons) == (0, 0)


def test_sticks_direct_forward():
    mixer, axes, buttons = _joy(ly=0, ry=0)
    move, turn = mixer.update(0, axes, buttons)
    assert move == 255
    assert turn == 0


def test_sticks_direct_reverse():
    mixer, axes, buttons = _joy(ly=255, ry=255)
    move, turn = mixer.update(0, axes, buttons)
    assert move == -255
    assert turn == 0


def test_pad_up_increments_cruise():
    mixer, axes, buttons = _joy(pressed=[JoyMap.BTN_PAD_UP])
    move, turn = mixer.update(20, axes, buttons)
    assert mixer.base_speed == 10
    assert move == 10
    assert turn == 0


def test_cruise_opposite_stick_no_reverse():
    mixer = TankMixer()
    mixer.base_speed = 100
    axes = [0.0] * JoyMap.AXIS_COUNT
    axes[JoyMap.AX_LY] = ps2_to_ros_axis(255)
    axes[JoyMap.AX_RY] = ps2_to_ros_axis(255)
    buttons = [0] * JoyMap.BUTTON_COUNT
    move, turn = mixer.update(0, axes, buttons)
    assert move == 0
    assert turn == 0


def test_l2_latches_stop():
    mixer, axes, buttons = _joy(pressed=[JoyMap.BTN_L2])
    mixer.base_speed = 80
    move, _ = mixer.update(10, axes, buttons)
    assert mixer.base_speed == 0
    assert move == 0


def test_l1_hold_ramps():
    mixer = TankMixer()
    axes = [0.0] * JoyMap.AXIS_COUNT
    buttons = [0] * JoyMap.BUTTON_COUNT
    buttons[JoyMap.BTN_L1] = 1
    mixer.update(0, axes, buttons)
    mixer.update(80, axes, buttons)
    assert mixer.base_speed == 20


def test_r1_decrements():
    mixer = TankMixer()
    mixer.base_speed = 30
    axes = [0.0] * JoyMap.AXIS_COUNT
    buttons = [0] * JoyMap.BUTTON_COUNT
    buttons[JoyMap.BTN_R1] = 1
    mixer.update(0, axes, buttons)
    assert mixer.base_speed == 20


def test_pivot_left():
    mixer, axes, buttons = _joy(held=[JoyMap.BTN_PAD_LEFT])
    move, turn = mixer.update(0, axes, buttons)
    assert move == 0
    assert turn == -80


def test_web_overlay():
    mixer, axes, buttons = _joy(web=(150 / 255.0, 0.0))
    move, turn = mixer.update(0, axes, buttons)
    assert move == 150
    assert turn == 0


def test_start_overrides_web_overlay():
    mixer = TankMixer()
    axes = [0.0] * JoyMap.AXIS_COUNT
    buttons = [0] * JoyMap.BUTTON_COUNT
    axes[JoyMap.AX_WEB_MOVE] = 150 / 255.0
    assert mixer.update(0, axes, buttons) == (150, 0)
    buttons[JoyMap.BTN_START] = 1
    assert mixer.update(10, axes, buttons) == (0, 0)
    assert mixer.base_speed == 0
    buttons[JoyMap.BTN_START] = 0
    assert mixer.update(20, axes, buttons) == (0, 0)


def test_start_stops_without_web():
    mixer = TankMixer()
    mixer.base_speed = 80
    axes = [0.0] * JoyMap.AXIS_COUNT
    buttons = [0] * JoyMap.BUTTON_COUNT
    mixer.update(0, axes, buttons)
    buttons[JoyMap.BTN_START] = 1
    assert mixer.update(10, axes, buttons) == (0, 0)
    assert mixer.base_speed == 0


def test_web_overlay_release_returns_to_sticks():
    mixer = TankMixer()
    axes = [0.0] * JoyMap.AXIS_COUNT
    buttons = [0] * JoyMap.BUTTON_COUNT
    axes[JoyMap.AX_WEB_MOVE] = 150 / 255.0
    assert mixer.update(0, axes, buttons)[0] == 150
    axes[JoyMap.AX_WEB_MOVE] = 0.0
    axes[JoyMap.AX_LY] = ps2_to_ros_axis(0)
    axes[JoyMap.AX_RY] = ps2_to_ros_axis(0)
    move, turn = mixer.update(20, axes, buttons)
    assert move == 255
    assert turn == 0


def test_twist_scale():
    assert cmd_to_twist(255, 0) == (1.0, 0.0)
    assert ros_axis_to_ps2(-1.0) == 0
    assert ros_axis_to_ps2(1.0) == 255
    assert ros_axis_to_ps2(0.0) == 128


if __name__ == '__main__':
    pytest.main([__file__])
