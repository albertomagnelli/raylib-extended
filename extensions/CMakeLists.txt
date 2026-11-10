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

# Standalone configurations do not inherit the C++ standard of the parent project
if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# CameraController example, it only needs the header-only camera_controller.hpp
add_executable(camera_controller_example camera_controller_example.cpp)

target_include_directories(camera_controller_example PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(camera_controller_example PRIVATE raylib)

set_target_properties(camera_controller_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Particle example, ported from the RayLib-Demo project (examples/app/main.cpp, mx Partikel-Demo)
add_executable(particle_example particle_example.cpp)

target_link_libraries(particle_example PRIVATE raylib)

set_target_properties(particle_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Shadows example, ported from the RayLib-Demo project (examples/phase9_shadows/main.cpp, mx Phase 9)
add_executable(shadows_example shadows_example.cpp)

target_link_libraries(shadows_example PRIVATE raylib glm::glm)

set_target_properties(shadows_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
