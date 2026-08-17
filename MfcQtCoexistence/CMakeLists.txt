cmake_minimum_required(VERSION 3.21)

project(MfcQtCoexistence VERSION 1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2 means: use the MFC shared DLL. This is the normal choice for a desktop
# application and keeps this project compatible with Qt's shared DLL model.
set(CMAKE_MFC_FLAG 2)
find_package(MFC REQUIRED)

add_executable(MfcQtCoexistence WIN32
    src/EffectSettings.h
    src/EffectSettingsDialog.h
    src/EffectSettingsDialog.cpp
    src/ImageDisplayWindow.h
    src/ImageDisplayWindow.cpp
    src/ImageProcessor.h
    src/ImageProcessor.cpp
    src/ImageToggleButton.h
    src/ImageToggleButton.cpp
    src/MainFrame.h
    src/MainFrame.cpp
    src/MfcQtCoexistenceApp.cpp
    resources/MfcQtCoexistence.rc
    resources/resource.h
)

target_include_directories(MfcQtCoexistence PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

# Keep Windows' legacy min/max macros from colliding with std::min/std::max.
target_compile_definitions(MfcQtCoexistence PRIVATE
    NOMINMAX
    UNICODE
    _UNICODE
)

# CImage uses Windows GDI+ to read common image formats such as PNG and JPEG.
target_link_libraries(MfcQtCoexistence PRIVATE gdiplus)
