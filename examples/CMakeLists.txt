# Setup the project and settings
cmake_minimum_required(VERSION 3.22)
project(examples)

# Include raylib's cmake helper functions, so the examples can also be
# configured standalone (`cd examples && cmake .`) and not only as part
# of the raylib project.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")
include(AddIfFlagCompiles)

# Default to the Desktop platform when configured standalone.
if (NOT DEFINED PLATFORM)
    set(PLATFORM "Desktop" CACHE STRING "Platform to build examples for")
endif ()

# Directories that contain examples
set(example_dirs
    audio
    core
    models
    others
    shaders
    shapes
    text
    textures
    )

# Next few lines will check for existence of symbols or header files
# They are needed for the physac example and threads examples
set(CMAKE_REQUIRED_DEFINITIONS -D_POSIX_C_SOURCE=199309L)
include(CheckSymbolExists)
check_symbol_exists(CLOCK_MONOTONIC time.h HAVE_CLOCK_MONOTONIC)
check_symbol_exists(QueryPerformanceCounter windows.h HAVE_QPC)
set(CMAKE_REQUIRED_DEFINITIONS)

if (HAVE_QPC OR HAVE_CLOCK_MONOTONIC)
    set(example_dirs ${example_dirs} physac)
endif ()

include(CheckIncludeFile)
CHECK_INCLUDE_FILE("stdatomic.h" HAVE_STDATOMIC_H)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads)
if (CMAKE_USE_PTHREADS_INIT AND HAVE_STDATOMIC_H)
    add_if_flag_compiles("-std=c11" CMAKE_C_FLAGS)
    if (THREADS_HAVE_PTHREAD_ARG)
        add_if_flag_compiles("-pthread" CMAKE_C_FLAGS)
    endif ()
    if (CMAKE_THREAD_LIBS_INIT)
        link_libraries("${CMAKE_THREAD_LIBS_INIT}")
    endif ()
endif ()

if (APPLE AND NOT CMAKE_SYSTEM STRLESS "Darwin-18.0.0")
    add_definitions(-DGL_SILENCE_DEPRECATION)
    MESSAGE(AUTHOR_WARNING "OpenGL is deprecated starting with macOS 10.14 (Mojave)!")
endif ()

# Collect all source files and resource files
# into a CMake variable
set(example_sources)
set(example_resources)
foreach (example_dir ${example_dirs})
    # Get the .cpp and .cpp files
    file(GLOB sources ${example_dir}/*.cpp)
    list(APPEND example_sources ${sources})

    # Any any resources
    file(GLOB resources ${example_dir}/resources/*)
    list(APPEND example_resources ${resources})
endforeach ()

if(NOT CMAKE_USE_PTHREADS_INIT OR NOT HAVE_STDATOMIC_H)
    # Items requiring pthreads
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/core/core_loading_thread.cpp)
endif ()

# The OpenGL 4.6 example calls desktop OpenGL functions directly, it requires
# raylib to run with an OpenGL 4.6 context (cmake -DOPENGL_VERSION=4.6)
if (NOT "${PLATFORM}" MATCHES "Desktop|SDL|RGFW|Win32")
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/opengl46_compute_particles.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/opengl46_indirect_elements.cpp)
endif ()

if (${PLATFORM} MATCHES "Android")
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/rlgl_standalone.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/standard_lighting.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/core/core_3d_picking.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/core/core_vr_simulator.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/core/core_3d_camera_free.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/core/core_3d_camera_first_person.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/core/core_world_screen.cpp)

    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_mesh_picking.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_cubicmap.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_skybox.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_mesh_picking.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_mesh_generation.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_heightmap.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_billboard.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_rlgl_solar_system.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_rlgl_full_solar_system.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_solar_system.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_obj_viewer.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_animation.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_first_person_maze.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/models/models_magicavoxel_loading.cpp)


    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shaders_custom_uniform.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shaders_model_shader.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shaders_view_depth.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shaders_postprocessing.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shaders_raymarching.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shaders_palette_switch.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shaders_basic_lighting.cpp)

elseif (${PLATFORM} MATCHES "Web")
    set(example_sources) # clear example_sources
    list(APPEND example_sources core/core_input_gestures_testbed.cpp)

elseif ("${PLATFORM}" STREQUAL "DRM")
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/rlgl_standalone.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/raylib_opengl_interop.cpp)

elseif ("${PLATFORM}" MATCHES "Memory")
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/raylib_opengl_interop.cpp)

elseif ("${OPENGL_VERSION}" STREQUAL "Software")
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/rlgl_standalone.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/raylib_opengl_interop.cpp)

elseif (NOT SUPPORT_GESTURES_SYSTEM)
    # Items requiring gestures system
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/textures/textures_mouse_painting.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/core/core_basic_screen_manager.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/core/core_input_gestures_web.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/core/core_input_gestures.cpp)

endif ()

# The rlgl_standalone example only targets desktop, without shared libraries.
if (BUILD_SHARED_LIBS OR NOT ${PLATFORM} MATCHES "Desktop")
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/rlgl_standalone.cpp)
endif()

# The audio examples fail to link if raylib is built without raudio
if (NOT SUPPORT_MODULE_RAUDIO)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_mixed_processor.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_module_playing.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_music_stream.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_raw_stream.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_sound_loading.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_sound_multi.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_stream_effects.cpp)

    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/others/embedded_files_loading.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/textures/textures_sprite_button.cpp)
    list(REMOVE_ITEM example_sources ${CMAKE_CURRENT_SOURCE_DIR}/textures/textures_sprite_explosion.cpp)
endif()

include_directories(BEFORE SYSTEM others/external/include)

if (NOT TARGET raylib)
    find_package(raylib 2.0 REQUIRED)
endif ()

# Do each example
foreach (example_source ${example_sources})
    # Create the basename for the example
    get_filename_component(example_name ${example_source} NAME)
    string(REPLACE ".cpp" "" example_name ${example_name})

    # Setup the example
    add_executable(${example_name} ${example_source})

    target_link_libraries(${example_name} raylib)
    if (NOT WIN32)
        target_link_libraries(${example_name} m)
    endif()

    # This example calls system OpenGL stencil functions directly.
    if ("${example_name}" STREQUAL "gl_scissor_shapes")
        if ("${PLATFORM}" MATCHES "Desktop" OR "${PLATFORM}" MATCHES "RGFW")
            find_package(OpenGL REQUIRED)
            target_link_libraries(${example_name} OpenGL::GL)
        endif ()
    endif ()

    string(REGEX MATCH ".*/.*/" resources_dir ${example_source})
    string(APPEND resources_dir "resources")

    if (${PLATFORM} MATCHES "Web")
        target_compile_options(${example_name} PRIVATE -Os)
        target_link_options(${example_name} PRIVATE
            -sALLOW_MEMORY_GROWTH=1
            -sEXPORTED_RUNTIME_METHODS=[requestFullscreen]
            -sUSE_GLFW=3
            --shell-file "${CMAKE_SOURCE_DIR}/src/shell.html"
        )
        set_target_properties(${example_name} PROPERTIES SUFFIX ".html")

        if (EXISTS ${resources_dir})
            # The local resources path needs to be mapped to /resources virtual path
            string(APPEND resources_dir "@resources")
            set_target_properties(${example_name} PROPERTIES LINK_FLAGS "--preload-file ${resources_dir}")
        endif ()

        if(${GRAPHICS} MATCHES "GRAPHICS_API_OPENGL_ES3")
            target_link_options(${example_name} PUBLIC "-sMIN_WEBGL_VERSION=2")
            target_link_options(${example_name} PUBLIC "-sMAX_WEBGL_VERSION=2")
        endif()

        # Checks if OSX and links appropriate frameworks (Only required on MacOS)
        if (APPLE)
            target_link_libraries(${example_name} "-framework IOKit")
            target_link_libraries(${example_name} "-framework Cocoa")
            target_link_libraries(${example_name} "-framework OpenGL")
            target_link_libraries(${example_name} "-framework QuartzCore")
        endif()

    elseif (${PLATFORM} MATCHES "RGFW")
        if (APPLE)
            target_link_libraries(${example_name} "-framework IOKit")
            target_link_libraries(${example_name} "-framework Cocoa")
            target_link_libraries(${example_name} "-framework OpenGL")
        elseif (WIN32)
            target_link_libraries(${example_name} "-lgdi32")
            target_link_libraries(${example_name} "-lwinmm")
        elseif (UNIX)
            target_link_libraries(${example_name} "-lX11")
            target_link_libraries(${example_name} "-lXrandr")
        endif()
    endif ()
endforeach ()

# Copy all of the resource files to the destination
file(COPY ${example_resources} DESTINATION "resources/")
