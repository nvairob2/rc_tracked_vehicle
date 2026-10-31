#!/usr/bin/env python3
"""Открытая одометрия из /cmd_vel, пока нет энкодеров STM32 в ROS2.

Публикует /odom и TF odom→base_link. Для стенда и первой интеграции Nav2.
"""

from __future__ import annotations

import math

import rclpy
from geometry_msgs.msg import Quaternion, TransformStamped, Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node
from tf2_ros import TransformBroadcaster


def yaw_to_quat(yaw: float) -> Quaternion:
    q = Quaternion()
    q.z = math.sin(yaw * 0.5)
    q.w = math.cos(yaw * 0.5)
    return q


class CmdVelOdom(Node):
    def __init__(self) -> None:
        super().__init__('cmd_vel_odom')
        self.declare_parameter('odom_frame', 'odom')
        self.declare_parameter('base_frame', 'base_link')
        self.declare_parameter('publish_tf', True)

        self._x = 0.0
        self._y = 0.0
        self._yaw = 0.0
        self._vx = 0.0
        self._wz = 0.0
        self._last = self.get_clock().now()

        self._pub = self.create_publisher(Odometry, 'odom', 10)
        self._tf = TransformBroadcaster(self)
        self.create_subscription(Twist, 'cmd_vel', self._on_cmd, 10)
        self.create_timer(0.02, self._tick)  # 50 Гц
        self.get_logger().info('cmd_vel_odom: открытая одометрия из /cmd_vel')

    def _on_cmd(self, msg: Twist) -> None:
        self._vx = msg.linear.x
        self._wz = msg.angular.z

    def _tick(self) -> None:
        now = self.get_clock().now()
        dt = (now - self._last).nanoseconds * 1e-9
        self._last = now
        if dt <= 0.0 or dt > 0.5:
            return

        self._yaw += self._wz * dt
        self._x += self._vx * math.cos(self._yaw) * dt
        self._y += self._vx * math.sin(self._yaw) * dt

        odom_frame = self.get_parameter('odom_frame').value
        base_frame = self.get_parameter('base_frame').value
        q = yaw_to_quat(self._yaw)

        odom = Odometry()
        odom.header.stamp = now.to_msg()
        odom.header.frame_id = odom_frame
        odom.child_frame_id = base_frame
        odom.pose.pose.position.x = self._x
        odom.pose.pose.position.y = self._y
        odom.pose.pose.orientation = q
        odom.twist.twist.linear.x = self._vx
        odom.twist.twist.angular.z = self._wz
        self._pub.publish(odom)

        if self.get_parameter('publish_tf').value:
            t = TransformStamped()
            t.header.stamp = now.to_msg()
            t.header.frame_id = odom_frame
            t.child_frame_id = base_frame
            t.transform.translation.x = self._x
            t.transform.translation.y = self._y
            t.transform.rotation = q
            self._tf.sendTransform(t)


def main() -> None:
    rclpy.init()
    node = CmdVelOdom()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
