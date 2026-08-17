cmake_minimum_required(VERSION 3.21)

project(ThreadedEffectPreview VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Run Qt's Meta-Object Compiler for signals, slots, and Q_OBJECT classes.
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

qt_add_executable(ThreadedEffectPreview
    main.cpp
    EffectType.h
    FrameProcessor.h
    FrameProcessor.cpp
    PreviewWindow.h
    PreviewWindow.cpp
)

target_link_libraries(ThreadedEffectPreview PRIVATE Qt6::Widgets)

set_target_properties(ThreadedEffectPreview PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Let the Debug/Release executable run outside Visual Studio as well.
if(WIN32)
    add_custom_command(TARGET ThreadedEffectPreview POST_BUILD
        COMMAND "$<TARGET_FILE_DIR:Qt6::Core>/windeployqt.exe"
                "$<$<CONFIG:Debug>:--debug>"
                "$<$<NOT:$<CONFIG:Debug>>:--release>"
                "$<TARGET_FILE:ThreadedEffectPreview>"
        COMMENT "Deploying Qt runtime files beside ThreadedEffectPreview"
    )
endif()

# This target has no widgets. It exercises the real worker-thread contract:
# queue work to FrameProcessor, wait for its signal, then check the result.
enable_testing()

qt_add_executable(ThreadedEffectPreviewTests
    tests/tst_frameprocessor.cpp
    EffectType.h
    FrameProcessor.h
    FrameProcessor.cpp
)

target_link_libraries(ThreadedEffectPreviewTests PRIVATE
    Qt6::Gui
    Qt6::Test
)

# The test source lives in tests/, while the worker headers remain in the
# project root beside the application sources.
target_include_directories(ThreadedEffectPreviewTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_test(NAME threaded_effect_preview
         COMMAND ThreadedEffectPreviewTests)

# The test executable can be started by CTest or the Python runner outside
# Visual Studio, so deploy the Qt DLLs beside it just like the sample app.
if(WIN32)
    add_custom_command(TARGET ThreadedEffectPreviewTests POST_BUILD
        COMMAND "$<TARGET_FILE_DIR:Qt6::Core>/windeployqt.exe"
                "$<$<CONFIG:Debug>:--debug>"
                "$<$<NOT:$<CONFIG:Debug>>:--release>"
                "$<TARGET_FILE:ThreadedEffectPreviewTests>"
        COMMENT "Deploying Qt runtime files beside ThreadedEffectPreviewTests"
    )
endif()
