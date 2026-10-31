"""Картографирование: лидар + открытая одометрия + slam_toolbox."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    pkg = get_package_share_directory('tank_nav')
    mapper = os.path.join(pkg, 'config', 'mapper_params_online_async.yaml')
    use_rviz = LaunchConfiguration('use_rviz')

    return LaunchDescription([
        DeclareLaunchArgument('use_rviz', default_value='true'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([pkg, '/launch/lidar.launch.py']),
        ),
        Node(
            package='tank_nav',
            executable='cmd_vel_odom.py',
            name='cmd_vel_odom',
            output='screen',
        ),
        Node(
            package='slam_toolbox',
            executable='async_slam_toolbox_node',
            name='slam_toolbox',
            output='screen',
            parameters=[mapper],
        ),
        Node(
            condition=IfCondition(use_rviz),
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            arguments=['-d', os.path.join(pkg, 'rviz', 'slam.rviz')],
        ),
    ])
