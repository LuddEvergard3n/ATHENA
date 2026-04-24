# ATHENA Core - CMake Build System
# Version: 0.9.1
#
# Usage:
#   mkdir build && cd build
#   cmake ..
#   cmake --build . --config Release
#
# Windows with vcpkg:
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake_minimum_required(VERSION 3.16)
project(ATHENA VERSION 0.9.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ATHENA_BUILD_GUI "Build GUI application" ON)
option(ATHENA_BUILD_CLI "Build CLI application" ON)
option(ATHENA_BUILD_TESTS "Build tests" OFF)

# Core sources
set(CORE_SOURCES
    src/types.cpp src/json.cpp src/rng.cpp src/context.cpp
    src/manifest.cpp src/scenario.cpp src/scheduler.cpp
    src/serialization.cpp src/terrain.cpp src/terrain_semantics.cpp
    src/pathfinding.cpp src/platform_loader.cpp src/environment.cpp
)

set(SYSTEMS_SOURCES
    src/systems/movement.cpp src/systems/combat.cpp
    src/systems/logistics.cpp src/systems/detection.cpp src/systems/c2.cpp
)

set(ANALYSIS_SOURCES
    src/analysis/montecarlo.cpp src/analysis/sobol.cpp src/analysis/binary_output.cpp
)

set(AI_SOURCES src/ai/doctrine.cpp)
set(REPORT_SOURCES src/report/pdf_report.cpp)
set(INTEGRATED_SOURCES src/integrated.cpp)

set(TUI_SOURCES
    src/tui/terminal.cpp src/tui/widgets.cpp src/tui/browser.cpp
)

set(GUI_SOURCES
    src/gui/browser.cpp src/gui/integrated_gui.cpp
)

set(IMGUI_SOURCES
    external/imgui/imgui.cpp external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

# Core library
add_library(athena_core STATIC
    ${CORE_SOURCES} ${SYSTEMS_SOURCES} ${ANALYSIS_SOURCES}
    ${AI_SOURCES} ${REPORT_SOURCES} ${INTEGRATED_SOURCES}
)
target_include_directories(athena_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(athena_core PRIVATE
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
)

# CLI
if(ATHENA_BUILD_CLI)
    add_executable(athena-cli src/main.cpp)
    target_link_libraries(athena-cli PRIVATE athena_core)
    if(UNIX)
        target_link_libraries(athena-cli PRIVATE pthread)
    endif()
endif()

# GUI
if(ATHENA_BUILD_GUI)
    find_package(glfw3 CONFIG QUIET)
    if(NOT glfw3_FOUND)
        find_package(glfw3 REQUIRED)
    endif()
    find_package(OpenGL REQUIRED)
    
    add_library(imgui STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
    )
    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
    
    add_library(athena_tui STATIC ${TUI_SOURCES})
    target_link_libraries(athena_tui PUBLIC athena_core)
    
    add_library(athena_gui STATIC ${GUI_SOURCES})
    target_include_directories(athena_gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
    target_link_libraries(athena_gui PUBLIC athena_core imgui)
    
    add_executable(athena src/unified_main.cpp)
    target_link_libraries(athena PRIVATE athena_core athena_tui athena_gui imgui glfw OpenGL::GL)
    
    if(UNIX AND NOT APPLE)
        target_link_libraries(athena PRIVATE dl pthread)
    endif()
    if(APPLE)
        target_link_libraries(athena PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    endif()
    if(WIN32)
        target_link_libraries(athena PRIVATE opengl32)
        # Build as a Windows GUI application (no console window).
        # MinGW: equivalent to -mwindows linker flag.
        # MSVC:  needs /ENTRY:mainCRTStartup to keep main() as entry point.
        set_target_properties(athena PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MSVC)
            target_link_options(athena PRIVATE "/ENTRY:mainCRTStartup")
        endif()
    endif()
endif()

# Tests
if(ATHENA_BUILD_TESTS)
    enable_testing()
    file(GLOB TEST_SOURCES "test/test_*.cpp")
    foreach(TEST_SOURCE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME} PRIVATE athena_core)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endforeach()
endif()

message(STATUS "ATHENA v${PROJECT_VERSION}: GUI=${ATHENA_BUILD_GUI} CLI=${ATHENA_BUILD_CLI}")
