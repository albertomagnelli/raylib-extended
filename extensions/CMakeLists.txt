# Setup the project and settings
cmake_minimum_required(VERSION 3.22)
project(extensions CXX)

# Include raylib's cmake helper functions, so the extensions can also be
# configured standalone (`cd extensions && cmake -B build`) and not only as part
# of the raylib project
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")

# When configured standalone, raylib is not a target yet: it is built from the parent directory
if (NOT TARGET raylib)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." "${CMAKE_BINARY_DIR}/raylib")
endif ()

# CameraController example, it only needs the header-only camera_controller.hpp
add_executable(camera_controller_example camera_controller_example.cpp)

target_include_directories(camera_controller_example PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(camera_controller_example PRIVATE raylib)

set_target_properties(camera_controller_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
