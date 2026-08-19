cmake_minimum_required(VERSION 3.21)

project(MiniEditorCoexistence VERSION 1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Phase 0 is intentionally pure MFC. Later phases will add Qt one panel at a
# time, leaving this baseline available for comparison.
set(CMAKE_MFC_FLAG 2)
find_package(MFC REQUIRED)

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
