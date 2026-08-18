cmake_minimum_required(VERSION 3.21)

project(MfcQtCoexistence VERSION 1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2 means: use the MFC shared DLL. This is the normal choice for a desktop
# application and keeps this project compatible with Qt's shared DLL model.
set(CMAKE_MFC_FLAG 2)
find_package(MFC REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Keep the original MFC dialog available as a learning comparison point.
option(MFCQT_USE_QT_SETTINGS_DIALOG
       "Use the Qt implementation of the effect settings dialog"
       ON)

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
    src/QtEffectSettingsDialog.h
    src/QtEffectSettingsDialog.cpp
    src/QtRuntime.h
    src/QtRuntime.cpp
    resources/MfcQtCoexistence.rc
    resources/resource.h
)

target_include_directories(MfcQtCoexistence PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

# MFC supplies WinMain. Do not link Qt's helper library, which supplies an
# entry point for ordinary Qt applications with a main() function.
set_target_properties(MfcQtCoexistence PROPERTIES
    qt_no_entrypoint TRUE
)

# Keep Windows' legacy min/max macros from colliding with std::min/std::max.
target_compile_definitions(MfcQtCoexistence PRIVATE
    NOMINMAX
    UNICODE
    _UNICODE
)

# CImage uses Windows GDI+ to read common image formats such as PNG and JPEG.
target_link_libraries(MfcQtCoexistence PRIVATE
    gdiplus
    Qt6::Widgets
)

if(MFCQT_USE_QT_SETTINGS_DIALOG)
    target_compile_definitions(MfcQtCoexistence PRIVATE
        MFCQT_USE_QT_SETTINGS_DIALOG=1
    )
else()
    target_compile_definitions(MfcQtCoexistence PRIVATE
        MFCQT_USE_QT_SETTINGS_DIALOG=0
    )
endif()

# This is now a hybrid MFC + Qt executable. Deploy Qt beside it so it can be
# launched directly from the Debug/Release directory, not only by Visual Studio.
if(WIN32)
    add_custom_command(TARGET MfcQtCoexistence POST_BUILD
        COMMAND "$<TARGET_FILE_DIR:Qt6::Core>/windeployqt.exe"
                "$<$<CONFIG:Debug>:--debug>"
                "$<$<NOT:$<CONFIG:Debug>>:--release>"
                "$<TARGET_FILE:MfcQtCoexistence>"
        COMMENT "Deploying Qt runtime files beside MfcQtCoexistence"
    )
endif()
