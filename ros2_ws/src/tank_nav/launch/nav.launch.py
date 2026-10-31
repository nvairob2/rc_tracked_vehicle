"""Навигация Nav2 по готовой карте (map.yaml)."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    pkg = get_package_share_directory('tank_nav')
    nav2_bringup = get_package_share_directory('nav2_bringup')
    params = os.path.join(pkg, 'config', 'nav2_params.yaml')
    default_map = os.path.join(pkg, 'maps', 'empty.yaml')

    return LaunchDescription([
        DeclareLaunchArgument('map', default_value=default_map),
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([pkg, '/launch/lidar.launch.py']),
        ),
        Node(
            package='tank_nav',
            executable='cmd_vel_odom.py',
            name='cmd_vel_odom',
            output='screen',
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(nav2_bringup, 'launch', 'bringup_launch.py'),
            ),
            launch_arguments={
                'map': LaunchConfiguration('map'),
                'use_sim_time': LaunchConfiguration('use_sim_time'),
                'params_file': params,
                'slam': 'False',
                'autostart': 'true',
            }.items(),
        ),
    ])
