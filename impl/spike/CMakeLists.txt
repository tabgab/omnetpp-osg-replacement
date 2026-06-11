cmake_minimum_required(VERSION 3.20)
project(usd_qt_spike CXX)

# ---------------------------------------------------------------------------
# C++ standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Qt AUTOMOC — required for QObject-derived classes (SpikeWidget etc.)
# ---------------------------------------------------------------------------
set(CMAKE_AUTOMOC ON)

# ---------------------------------------------------------------------------
# Qt 6
# Configure with: -DCMAKE_PREFIX_PATH=<Qt6_INSTALL>
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)

# ---------------------------------------------------------------------------
# OpenUSD (monolithic usd_ms build)
# Configure with: -Dpxr_DIR=<USD_INSTALL>/lib/cmake/pxr
#   or equivalently: -DCMAKE_PREFIX_PATH=<USD_INSTALL>
# The pxrConfig.cmake in the OpenUSD install exports PXR_LIBRARIES and
# PXR_INCLUDE_DIRS.  Requires OpenUSD >= 25.11 built with:
#   PXR_BUILD_IMAGING=ON
#   PXR_BUILD_USD_IMAGING=ON
#   PXR_ENABLE_METAL_SUPPORT=ON   (macOS)
#   PXR_BUILD_MONOLITHIC=ON       (produces usd_ms)
# ---------------------------------------------------------------------------
find_package(pxr CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(usd_qt_spike main.cpp)

# QT_NO_KEYWORDS is REQUIRED: Qt defines keywords (signals, slots, emit,
# foreach) that collide with identifiers used in TBB and Boost headers that
# OpenUSD pulls in transitively.  With this flag, Qt macros become
# Q_SIGNALS / Q_SLOTS / Q_EMIT / Q_FOREACH and the collision is avoided.
target_compile_definitions(usd_qt_spike PRIVATE QT_NO_KEYWORDS)

target_link_libraries(usd_qt_spike PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    ${PXR_LIBRARIES}
)

target_include_directories(usd_qt_spike PRIVATE
    ${PXR_INCLUDE_DIRS}
)

# ---------------------------------------------------------------------------
# macOS-specific notes
# ---------------------------------------------------------------------------
if(APPLE)
    # On macOS, OpenGL is capped at version 4.1.  HgiGL therefore cannot be
    # used for rendering (it requires GL >= 4.5).  Instead, the engine uses
    # HgiMetal for all GPU work and hgiInterop to composite the Metal-rendered
    # output into the QOpenGLWidget's GL FBO — the same path usdview takes.
    # Both hgiMetal and hgiInterop are bundled inside the monolithic usd_ms
    # library when PXR_ENABLE_METAL_SUPPORT=ON, so no extra link target is
    # needed beyond ${PXR_LIBRARIES}.  If building non-monolithic, add
    # "hgiMetal" and "hgiInterop" to target_link_libraries manually.
    message(STATUS "usd_qt_spike: macOS — HgiMetal + hgiInterop path (inside usd_ms)")
endif()
