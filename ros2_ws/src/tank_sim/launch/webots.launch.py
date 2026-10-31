"""Запуск Webots + драйвер ROS2 (на Ubuntu/opi5 с webots_ros2).

На Mac мир открывают вручную: Webots → Open tank_room.wbt
"""

import os
import pathlib

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, LogInfo
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    pkg = get_package_share_directory('tank_sim')
    world = os.path.join(pkg, 'worlds', 'tank_room.wbt')
    # webots_ros2_driver стандартный launch (Humble)
    try:
        driver_share = get_package_share_directory('webots_ros2_driver')
        has_driver = True
    except Exception:
        driver_share = ''
        has_driver = False

    actions = [
        DeclareLaunchArgument('use_sim_time', default_value='true'),
        LogInfo(msg=f'Webots world: {world}'),
    ]

    if has_driver:
        from webots_ros2_driver.webots_launcher import WebotsLauncher
        from webots_ros2_driver.webots_controller import WebotsController

        webots = WebotsLauncher(world=world, ros2_supervisor=True)
        robot_description = pathlib.Path(
            os.path.join(pkg, 'resource', 'tracked_tank.urdf'),
        ).read_text()
        controller = WebotsController(
            robot_name='tracked_tank',
            parameters=[
                {'robot_description': robot_description},
                {'use_sim_time': True},
            ],
        )
        actions.extend([
            webots,
            controller,
            Node(
                package='tank_nav',
                executable='cmd_vel_odom.py',
                name='cmd_vel_odom',
                parameters=[{'use_sim_time': True}],
                output='screen',
            ),
        ])
    else:
        actions.append(LogInfo(
            msg='webots_ros2_driver не найден. Откройте worlds/tank_room.wbt в Webots вручную.',
        ))

    return LaunchDescription(actions)
