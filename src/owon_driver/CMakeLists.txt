cmake_minimum_required(VERSION 3.8)
project(owon_driver)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

add_executable(owon_node src/owon_node.cpp)
add_executable(owon_value_marker_node src/owon_value_marker_node.cpp)

ament_target_dependencies(owon_node 
  rclcpp 
  std_msgs 
  nav_msgs 
  geometry_msgs
  visualization_msgs
  tf2
  tf2_ros
  tf2_geometry_msgs
)

ament_target_dependencies(owon_value_marker_node
  rclcpp
  std_msgs
  geometry_msgs
  visualization_msgs
)

install(TARGETS
  owon_node
  owon_value_marker_node
  DESTINATION lib/${PROJECT_NAME})

ament_package()
