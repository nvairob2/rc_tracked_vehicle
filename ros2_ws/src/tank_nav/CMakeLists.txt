cmake_minimum_required(VERSION 3.8)
project(tank_nav)

find_package(ament_cmake REQUIRED)

install(DIRECTORY launch config rviz maps
  DESTINATION share/${PROJECT_NAME}
)

install(PROGRAMS scripts/cmd_vel_odom.py
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
