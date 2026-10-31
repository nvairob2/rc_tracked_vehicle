"""Лидар D500 + URDF тележки. Топик скана: /ldlidar_node/scan → ремап на /scan."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LifecycleNode, Node


def generate_launch_description() -> LaunchDescription:
    pkg = get_package_share_directory('tank_nav')
    lidar_yaml = os.path.join(pkg, 'config', 'ldlidar.yaml')
    life_yaml = os.path.join(pkg, 'config', 'lifecycle_lidar.yaml')

    return LaunchDescription([
        DeclareLaunchArgument('use_rviz', default_value='false'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                get_package_share_directory('tank_description'),
                '/launch/description.launch.py',
            ]),
        ),
        LifecycleNode(
            package='ldlidar_node',
            executable='ldlidar_node',
            name='ldlidar_node',
            namespace='',
            output='screen',
            parameters=[lidar_yaml],
            # топик: /ldlidar_node/scan (ремап на /scan ломал LifecyclePublisher)
        ),
        Node(
            package='nav2_lifecycle_manager',
            executable='lifecycle_manager',
            name='lifecycle_manager',
            output='screen',
            parameters=[life_yaml],
        ),
    ])
