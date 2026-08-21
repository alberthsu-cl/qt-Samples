cmake_minimum_required(VERSION 3.21)

project(MiniEditorCoexistence VERSION 1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Phase 0 is intentionally pure MFC. Later phases will add Qt one panel at a
# time, leaving this baseline available for comparison.
set(CMAKE_MFC_FLAG 2)
find_package(MFC REQUIRED)

option(MINI_EDITOR_USE_QT
       "Build every UI panel migrated to Qt so far"
       ON)

if(MINI_EDITOR_USE_QT)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
endif()

add_executable(MiniEditorCoexistence WIN32
    src/DemoProject.h
    src/ProjectState.h
    src/WorkspaceSettings.h
    src/WorkspaceSettings.cpp
    src/MfcEditorPaneBase.h
    src/MfcEditorPaneBase.cpp
    src/MfcPreviewCanvas.h
    src/MfcPreviewCanvas.cpp
    src/MfcTimelineCanvas.h
    src/MfcTimelineCanvas.cpp
    src/MfcWorkspaceSplitter.h
    src/MfcWorkspaceSplitter.cpp
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

# Visual Studio filters are virtual folders only: the files stay in src/.
# Keep the model layer easy to find while studying the UI migration boundary.
source_group("DataModel" FILES
    src/DemoProject.h
    src/ProjectState.h
    src/WorkspaceSettings.h
    src/WorkspaceSettings.cpp
    src/MediaAssetModel.h
    src/MediaAssetModel.cpp
)

target_compile_definitions(MiniEditorCoexistence PRIVATE
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MINI_EDITOR_USE_QT)
    target_sources(MiniEditorCoexistence PRIVATE
        src/MediaAssetModel.h
        src/MediaAssetModel.cpp
        src/QtMediaLibraryPanel.h
        src/QtMediaLibraryPanel.cpp
        src/QtMediaLibraryHost.h
        src/QtMediaLibraryHost.cpp
        src/QtPropertiesPanel.h
        src/QtPropertiesPanel.cpp
        src/QtPropertiesHost.h
        src/QtPropertiesHost.cpp
        src/QtTransportPanel.h
        src/QtTransportPanel.cpp
        src/QtTransportHost.h
        src/QtTransportHost.cpp
        src/QtTimelineToolbar.h
        src/QtTimelineToolbar.cpp
        src/QtTimelineToolbarHost.h
        src/QtTimelineToolbarHost.cpp
        src/QtRuntime.h
        src/QtRuntime.cpp
    )

    target_link_libraries(MiniEditorCoexistence PRIVATE Qt6::Widgets)
    set_target_properties(MiniEditorCoexistence PROPERTIES
        AUTOMOC ON
        qt_no_entrypoint TRUE
    )
    target_compile_definitions(MiniEditorCoexistence PRIVATE
        MINI_EDITOR_USE_QT=1
    )

    add_custom_command(TARGET MiniEditorCoexistence POST_BUILD
        COMMAND "$<TARGET_FILE_DIR:Qt6::Core>/windeployqt.exe"
                "$<$<CONFIG:Debug>:--debug>"
                "$<$<NOT:$<CONFIG:Debug>>:--release>"
                "$<TARGET_FILE:MiniEditorCoexistence>"
        COMMENT "Deploying Qt runtime files beside MiniEditorCoexistence"
    )
else()
    # These are complete MFC fallbacks. They intentionally stay out of the
    # Qt-enabled target, where their corresponding Qt panels are used instead.
    target_sources(MiniEditorCoexistence PRIVATE
        src/MfcMediaLibraryPane.h
        src/MfcMediaLibraryPane.cpp
        src/MfcTransportBar.h
        src/MfcTransportBar.cpp
        src/MfcPropertiesPane.h
        src/MfcPropertiesPane.cpp
        src/MfcTimelinePane.h
        src/MfcTimelinePane.cpp
    )
    target_compile_definitions(MiniEditorCoexistence PRIVATE
        MINI_EDITOR_USE_QT=0
    )
endif()
