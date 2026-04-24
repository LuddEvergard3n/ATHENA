################################################################################
# ATHENA UI - Qt6 Application
# Military Simulation Analysis Tool
# Version: 0.7.3
################################################################################

cmake_minimum_required(VERSION 3.16)
project(athena-ui
    VERSION 0.7.3
    DESCRIPTION "ATHENA Military Simulation UI"
    LANGUAGES CXX
)

# ------------------------------------------------------------------------------
# C++ Standard
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
# Qt Configuration
# ------------------------------------------------------------------------------
set(CMAKE_AUTOMOC ON)  # Process Q_OBJECT macros
set(CMAKE_AUTORCC ON)  # Process .qrc resource files
set(CMAKE_AUTOUIC ON)  # Process .ui designer files

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Charts
    OpenGLWidgets
)

# ------------------------------------------------------------------------------
# ATHENA Core Library
# ------------------------------------------------------------------------------
# Build athena-core as subdirectory
set(ATHENA_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../athena-core")

if(EXISTS "${ATHENA_CORE_DIR}/CMakeLists.txt")
    add_subdirectory(${ATHENA_CORE_DIR} ${CMAKE_BINARY_DIR}/athena-core)
    message(STATUS "ATHENA Core: Found and added as subdirectory")
else()
    message(FATAL_ERROR "ATHENA Core not found at ${ATHENA_CORE_DIR}")
endif()

# ------------------------------------------------------------------------------
# Compiler Warnings
# ------------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# ------------------------------------------------------------------------------
# Source Files
# ------------------------------------------------------------------------------
set(ATHENA_UI_SOURCES
    src/main.cpp
    
    # Application
    src/app/mainwindow.cpp
    
    # Commands (Undo/Redo)
    src/commands/undocommands.cpp
    
    # Simulation
    src/simulation/simulationrunner.cpp
    src/simulation/scenariobuilder.cpp
    src/simulation/scenariofile.cpp
    src/simulation/resultsexporter.cpp
    
    # Widgets
    src/widgets/mapview.cpp
    src/widgets/platformbrowser.cpp
    src/widgets/forcetree.cpp
    src/widgets/simulationpanel.cpp
    src/widgets/resultspanel.cpp
    src/widgets/platformdetails.cpp
    src/widgets/losshistogram.cpp
    src/widgets/minimap.cpp
    src/widgets/formationdialog.cpp
    
    # Graphics Items
    src/graphics/mapscene.cpp
    src/graphics/terrainlayer.cpp
    src/graphics/unititem.cpp
    src/graphics/heatmaplayer.cpp
    src/graphics/rangeoverlay.cpp
    src/graphics/movementpath.cpp
    src/graphics/engagementlines.cpp
    
    # Models
    src/models/platformmodel.cpp
    src/models/forcemodel.cpp
    
    # Utils
    src/utils/formationtemplates.cpp
)

set(ATHENA_UI_HEADERS
    src/app/mainwindow.hpp
    
    # Commands (Undo/Redo)
    src/commands/undocommands.hpp
    
    # Utils
    src/utils/geoutils.hpp
    src/utils/formationtemplates.hpp
    
    src/simulation/simulationrunner.hpp
    src/simulation/scenariobuilder.hpp
    src/simulation/scenariofile.hpp
    src/simulation/resultsexporter.hpp
    
    src/widgets/mapview.hpp
    src/widgets/platformbrowser.hpp
    src/widgets/forcetree.hpp
    src/widgets/simulationpanel.hpp
    src/widgets/resultspanel.hpp
    src/widgets/platformdetails.hpp
    src/widgets/losshistogram.hpp
    src/widgets/minimap.hpp
    src/widgets/formationdialog.hpp
    
    src/graphics/mapscene.hpp
    src/graphics/terrainlayer.hpp
    src/graphics/unititem.hpp
    src/graphics/heatmaplayer.hpp
    src/graphics/rangeoverlay.hpp
    src/graphics/movementpath.hpp
    src/graphics/engagementlines.hpp
    
    src/models/platformmodel.hpp
    src/models/forcemodel.hpp
)

set(ATHENA_UI_RESOURCES
    resources/athena.qrc
)

# ------------------------------------------------------------------------------
# Executable
# ------------------------------------------------------------------------------
if(WIN32)
    # Windows: Create GUI application (no console window)
    add_executable(athena-ui WIN32
        ${ATHENA_UI_SOURCES}
        ${ATHENA_UI_HEADERS}
        ${ATHENA_UI_RESOURCES}
    )
else()
    add_executable(athena-ui
        ${ATHENA_UI_SOURCES}
        ${ATHENA_UI_HEADERS}
        ${ATHENA_UI_RESOURCES}
    )
endif()

# ------------------------------------------------------------------------------
# Include Directories
# ------------------------------------------------------------------------------
target_include_directories(athena-ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ------------------------------------------------------------------------------
# Link Libraries
# ------------------------------------------------------------------------------
target_link_libraries(athena-ui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Charts
    Qt6::OpenGLWidgets
    athena-core
)

# ------------------------------------------------------------------------------
# Data Files
# ------------------------------------------------------------------------------
# Copy platform database to build directory
add_custom_command(TARGET athena-ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ATHENA_CORE_DIR}/data/platforms"
        "$<TARGET_FILE_DIR:athena-ui>/data/platforms"
    COMMENT "Copying platform database..."
)

# ------------------------------------------------------------------------------
# Installation
# ------------------------------------------------------------------------------
install(TARGETS athena-ui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "${ATHENA_CORE_DIR}/data/platforms"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/athena/data
)

# Qt deployment (copies Qt DLLs on Windows)
if(WIN32)
    add_custom_command(TARGET athena-ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Run windeployqt manually or via install step"
    )
endif()

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== ATHENA UI Configuration ===")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Qt Version:     ${Qt6_VERSION}")
message(STATUS "C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "Core Library:   ${ATHENA_CORE_DIR}")
message(STATUS "")
