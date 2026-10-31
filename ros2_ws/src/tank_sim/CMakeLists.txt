cmake_minimum_required(VERSION 3.8)
project(tank_sim)

find_package(ament_cmake REQUIRED)

install(DIRECTORY worlds protos launch resource
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
