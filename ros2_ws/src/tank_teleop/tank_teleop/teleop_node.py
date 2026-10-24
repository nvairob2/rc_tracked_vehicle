"""Lifecycle-узел: /joy → /cmd_vel (50 Гц)."""

from __future__ import annotations

import rclpy
from geometry_msgs.msg import Twist
from rclpy.lifecycle import LifecycleNode, State, TransitionCallbackReturn
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Joy

from tank_teleop.joy_map import cmd_to_twist
from tank_teleop.mixer import TankMixer


class TankTeleopNode(LifecycleNode):
    def __init__(self) -> None:
        super().__init__('tank_teleop')
        self._mixer = TankMixer()
        self._joy: Joy | None = None
        self._pub = None
        self._sub = None
        self._timer = None
        self.declare_parameter('rate_hz', 50.0)
        self.declare_parameter('cmd_timeout_ms', 200)

    def on_configure(self, state: State) -> TransitionCallbackReturn:
        self._mixer.reset()
        self._joy = None
        self._pub = self.create_lifecycle_publisher(Twist, 'cmd_vel', 10)
        self._sub = self.create_subscription(Joy, 'joy', self._on_joy, qos_profile_sensor_data)
        self.get_logger().info('tank_teleop: configured')
        return TransitionCallbackReturn.SUCCESS

    def on_activate(self, state: State) -> TransitionCallbackReturn:
        rate = float(self.get_parameter('rate_hz').value)
        period = 1.0 / max(rate, 1.0)
        self._timer = self.create_timer(period, self._tick)
        self.get_logger().info('tank_teleop: active')
        return super().on_activate(state)

    def on_deactivate(self, state: State) -> TransitionCallbackReturn:
        if self._timer is not None:
            self._timer.cancel()
            self.destroy_timer(self._timer)
            self._timer = None
        self._publish_stop()
        return super().on_deactivate(state)

    def on_cleanup(self, state: State) -> TransitionCallbackReturn:
        if self._sub is not None:
            self.destroy_subscription(self._sub)
            self._sub = None
        if self._pub is not None:
            self.destroy_publisher(self._pub)
            self._pub = None
        return TransitionCallbackReturn.SUCCESS

    def _on_joy(self, msg: Joy) -> None:
        self._joy = msg

    def _tick(self) -> None:
        if self._joy is None or self._pub is None:
            self._publish_stop()
            return

        timeout_ms = int(self.get_parameter('cmd_timeout_ms').value)
        now = self.get_clock().now()
        age_ms = (now - rclpy.time.Time.from_msg(self._joy.header.stamp)).nanoseconds / 1e6
        # Если штамп нулевой (ESP32 без синхронизации времени) — не режем по возрасту.
        if self._joy.header.stamp.sec != 0 and age_ms > timeout_ms:
            self._publish_stop()
            return

        now_ms = now.nanoseconds // 1_000_000
        move, turn = self._mixer.update(now_ms, list(self._joy.axes), list(self._joy.buttons))
        lin, ang = cmd_to_twist(move, turn)
        msg = Twist()
        msg.linear.x = lin
        msg.angular.z = ang
        self._pub.publish(msg)

    def _publish_stop(self) -> None:
        if self._pub is None:
            return
        self._pub.publish(Twist())


def main(args=None) -> None:
    rclpy.init(args=args)
    node = TankTeleopNode()
    # Авто-переход configure → activate для стенда без extra launch-логики.
    node.trigger_configure()
    node.trigger_activate()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
