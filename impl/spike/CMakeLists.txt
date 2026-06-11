cmake_minimum_required(VERSION 3.20)
project(usd_qt_spike CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ---------------------------------------------------------------------------
# Qt 6 — only QtWidgets is needed (QApplication + QWindow; the macOS path is
# Metal-native, so no OpenGL[Widgets] modules).
#   Configure with: -DCMAKE_PREFIX_PATH=<Qt6_INSTALL>
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ---------------------------------------------------------------------------
# OpenUSD (monolithic usd_ms build).
#   Configure with: -DCMAKE_PREFIX_PATH=<USD_INSTALL>  (finds pxrConfig.cmake)
#   Requires >= 25.11 built with imaging + usd-imaging + Metal (hgiMetal).
# ---------------------------------------------------------------------------
find_package(pxr CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Executable — main.mm is Objective-C++ (CAMetalLayer / Metal present path).
# ---------------------------------------------------------------------------
if(NOT APPLE)
    message(FATAL_ERROR "This spike's present path is macOS/Metal-native (main.mm). "
                        "On Linux/Windows-WSL use the HgiGL path (QOpenGLWidget renders "
                        "directly, no interop) — see docs/06 R3.")
endif()

add_executable(usd_qt_spike main.mm)

# Objective-C ARC for the .mm (retains CAMetalLayer / pipeline members safely).
set_source_files_properties(main.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

# QT_NO_KEYWORDS: Qt's signals/slots/emit macros collide with TBB/Boost headers
# that OpenUSD pulls in.  Use Q_SIGNALS/Q_SLOTS/Q_EMIT instead.
target_compile_definitions(usd_qt_spike PRIVATE QT_NO_KEYWORDS)

target_link_libraries(usd_qt_spike PRIVATE
    Qt6::Widgets
    ${PXR_LIBRARIES}
    "-framework Metal"
    "-framework QuartzCore"
    "-framework Foundation"
    "-framework AppKit"
    "-framework CoreGraphics"
)

target_include_directories(usd_qt_spike PRIVATE ${PXR_INCLUDE_DIRS})

message(STATUS "usd_qt_spike: macOS Metal-native present (CAMetalLayer, no GL interop)")
