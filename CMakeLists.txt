cmake_minimum_required(VERSION 2.8.3)
project(trajopt_benchmarking)

add_compile_options(-std=c++11 -Wall -Wextra)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  roslib
  trajopt
  trajopt_examples
  tesseract_ros
  tesseract_planning
  octomap_ros
  pcl_conversions
)

find_package(PCL REQUIRED COMPONENTS core features filters io segmentation surface)

catkin_package()

include_directories(
# include
  ${catkin_INCLUDE_DIRS}
  SYSTEM ${PCL_INCLUDE_DIRS}
)

add_executable(${PROJECT_NAME}_collision_comparison src/collision_comparison.cpp)
target_link_libraries(${PROJECT_NAME}_collision_comparison ${catkin_LIBRARIES})

add_executable(${PROJECT_NAME}_constraints_comparison src/constraints_comparison.cpp)
target_link_libraries(${PROJECT_NAME}_constraints_comparison ${catkin_LIBRARIES})

add_executable(${PROJECT_NAME}_costs_comparison src/costs_comparison.cpp)
target_link_libraries(${PROJECT_NAME}_costs_comparison ${catkin_LIBRARIES})

add_executable(${PROJECT_NAME}_solver_comparison_case1 src/solver_comparison_case1.cpp)
target_link_libraries(${PROJECT_NAME}_solver_comparison_case1 ${catkin_LIBRARIES})

add_executable(${PROJECT_NAME}_solver_comparison_case2 src/solver_comparison_case2.cpp)
target_link_libraries(${PROJECT_NAME}_solver_comparison_case2 ${catkin_LIBRARIES})

# Mark executables and/or libraries for installation
install(
  TARGETS ${PROJECT_NAME}_collision_comparison ${PROJECT_NAME}_constraints_comparison ${PROJECT_NAME}_costs_comparison ${PROJECT_NAME}_solver_comparison_case1 ${PROJECT_NAME}_solver_comparison_case2
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(
  DIRECTORY launch
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)

