"""Старт agent (UDP 8888) и tank_teleop."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, LogInfo
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description() -> LaunchDescription:
    start_agent = LaunchConfiguration('start_agent')
    agent_port = LaunchConfiguration('agent_port')
    rate_hz = LaunchConfiguration('rate_hz')

    return LaunchDescription([
        DeclareLaunchArgument('start_agent', default_value='true'),
        DeclareLaunchArgument('agent_port', default_value='8888'),
        DeclareLaunchArgument('rate_hz', default_value='50.0'),
        LogInfo(msg='tank_bringup: opi5 должен быть 192.168.4.2, ESP32 AP 192.168.4.1'),
        ExecuteProcess(
            condition=IfCondition(start_agent),
            cmd=['ros2', 'run', 'micro_ros_agent', 'micro_ros_agent', 'udp4', '--port', agent_port],
            output='screen',
            name='micro_ros_agent',
        ),
        Node(
            package='tank_teleop',
            executable='teleop_node',
            name='tank_teleop',
            output='screen',
            parameters=[{
                'rate_hz': ParameterValue(rate_hz, value_type=float),
                'cmd_timeout_ms': 200,
            }],
        ),
    ])
