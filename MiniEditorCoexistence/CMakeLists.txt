cmake_minimum_required(VERSION 3.21)

project(MiniEditorCoexistence VERSION 1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Phase 0 is intentionally pure MFC. Later phases will add Qt one panel at a
# time, leaving this baseline available for comparison.
set(CMAKE_MFC_FLAG 2)
find_package(MFC REQUIRED)

option(MINI_EDITOR_USE_QT_MEDIA_LIBRARY
       "Replace only the MFC Media Library pane with a Qt Model/View panel"
       ON)

if(MINI_EDITOR_USE_QT_MEDIA_LIBRARY)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
endif()

add_executable(MiniEditorCoexistence WIN32
    src/DemoProject.h
    src/EditorPaneBase.h
    src/EditorPaneBase.cpp
    src/MediaLibraryPane.h
    src/MediaLibraryPane.cpp
    src/PreviewPane.h
    src/PreviewPane.cpp
    src/PropertiesPane.h
    src/PropertiesPane.cpp
    src/TimelinePane.h
    src/TimelinePane.cpp
    src/MainFrame.h
    src/MainFrame.cpp
    src/MiniEditorCoexistenceApp.cpp
    resources/MiniEditorCoexistence.rc
    resources/resource.h
)

target_include_directories(MiniEditorCoexistence PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

target_compile_definitions(MiniEditorCoexistence PRIVATE
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MINI_EDITOR_USE_QT_MEDIA_LIBRARY)
    target_sources(MiniEditorCoexistence PRIVATE
        src/MediaAssetModel.h
        src/MediaAssetModel.cpp
        src/QtMediaLibraryPanel.h
        src/QtMediaLibraryPanel.cpp
        src/QtMediaLibraryHost.h
        src/QtMediaLibraryHost.cpp
        src/QtRuntime.h
        src/QtRuntime.cpp
    )

    target_link_libraries(MiniEditorCoexistence PRIVATE Qt6::Widgets)
    set_target_properties(MiniEditorCoexistence PROPERTIES
        AUTOMOC ON
        qt_no_entrypoint TRUE
    )
    target_compile_definitions(MiniEditorCoexistence PRIVATE
        MINI_EDITOR_USE_QT_MEDIA_LIBRARY=1
    )

    add_custom_command(TARGET MiniEditorCoexistence POST_BUILD
        COMMAND "$<TARGET_FILE_DIR:Qt6::Core>/windeployqt.exe"
                "$<$<CONFIG:Debug>:--debug>"
                "$<$<NOT:$<CONFIG:Debug>>:--release>"
                "$<TARGET_FILE:MiniEditorCoexistence>"
        COMMENT "Deploying Qt runtime files beside MiniEditorCoexistence"
    )
else()
    target_compile_definitions(MiniEditorCoexistence PRIVATE
        MINI_EDITOR_USE_QT_MEDIA_LIBRARY=0
    )
endif()
