cmake_minimum_required(VERSION 3.21)

project(QmlMiniEditor VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Qml)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(QmlMiniEditor
    main.cpp
)

# qt_add_qml_module embeds Main.qml in the executable and registers the C++
# types marked with QML_ELEMENT. This avoids runtime-relative QML file paths.
qt_add_qml_module(QmlMiniEditor
    URI QmlMiniEditor
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/MediaLibraryModel.h
        src/MediaLibraryModel.cpp
        src/EditorViewModel.h
        src/EditorViewModel.cpp
)

# qmltyperegistrar generates a C++ source file that includes QML_ELEMENT
# headers by their file name. Make src/ available to that generated source.
target_include_directories(QmlMiniEditor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(QmlMiniEditor PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
)

set_target_properties(QmlMiniEditor PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Place the Qt DLLs, QML modules, and platform plugins beside the executable
# so running from Visual Studio works without manually setting PATH.
if(WIN32)
    add_custom_command(TARGET QmlMiniEditor POST_BUILD
        COMMAND "$<TARGET_FILE_DIR:Qt6::Core>/windeployqt.exe"
                "$<$<CONFIG:Debug>:--debug>"
                "$<$<NOT:$<CONFIG:Debug>>:--release>"
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                "$<TARGET_FILE:QmlMiniEditor>"
        COMMENT "Deploying the Qt Quick runtime beside QmlMiniEditor"
    )
endif()
