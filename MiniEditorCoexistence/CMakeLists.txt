cmake_minimum_required(VERSION 3.21)

project(MiniEditorCoexistence VERSION 1.0 LANGUAGES CXX RC)

include(CTest)

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
    if(BUILD_TESTING)
        find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia
                                           MultimediaWidgets Test)
    else()
        find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia
                                           MultimediaWidgets)
    endif()
endif()

add_executable(MiniEditorCoexistence WIN32
    src/DemoProject.h
    src/ProjectState.h
    src/FrameTimecode.h
    src/MediaKind.h
    src/EditorProject.h
    src/EditorProject.cpp
    src/EditorChange.h
    src/EditorHistory.h
    src/EditorHistory.cpp
    src/ProjectSerializer.h
    src/ProjectSerializer.cpp
    src/MediaLibrary.h
    src/MediaLibrary.cpp
    src/TimelineModel.h
    src/TimelineModel.cpp
    src/TimelineGeometry.h
    src/TimelineGeometry.cpp
    src/TimelineClipEdit.h
    src/TimelineClipEdit.cpp
    src/ClipFade.h
    src/ClipFade.cpp
    src/TimelineTrackPolicy.h
    src/TimelineTrackPolicy.cpp
    src/TimelinePlaybackResolver.h
    src/TimelinePlaybackResolver.cpp
    src/EditorSession.h
    src/EditorSession.cpp
    src/EditorCommandController.h
    src/EditorCommandController.cpp
    src/PlaybackBackend.h
    src/PlaybackBackend.cpp
    src/PlaybackClockController.h
    src/PlaybackClockController.cpp
    src/TimelineEditingController.h
    src/TimelineEditingController.cpp
    src/PreviewStateResolver.h
    src/PreviewStateResolver.cpp
    src/ClipPropertiesStateResolver.h
    src/ClipPropertiesStateResolver.cpp
    src/TimelinePresentationStateResolver.h
    src/TimelinePresentationStateResolver.cpp
    src/ProjectDocumentService.h
    src/ProjectDocumentService.cpp
    src/WorkspaceSettings.h
    src/WorkspaceSettings.cpp
    src/WorkspaceLayout.h
    src/WorkspaceLayout.cpp
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
    src/MediaKind.h
    src/EditorProject.h
    src/EditorProject.cpp
    src/EditorChange.h
    src/EditorHistory.h
    src/EditorHistory.cpp
    src/ProjectSerializer.h
    src/ProjectSerializer.cpp
    src/MediaLibrary.h
    src/MediaLibrary.cpp
    src/TimelineModel.h
    src/TimelineModel.cpp
    src/EditorSession.h
    src/EditorSession.cpp
    src/WorkspaceSettings.h
    src/WorkspaceSettings.cpp
    src/MediaAssetModel.h
    src/MediaAssetModel.cpp
)

source_group("Layout" FILES
    src/WorkspaceLayout.h
    src/WorkspaceLayout.cpp
    src/TimelineGeometry.h
    src/TimelineGeometry.cpp
    src/TimelineClipEdit.h
    src/TimelineClipEdit.cpp
    src/ClipFade.h
    src/ClipFade.cpp
    src/TimelineTrackPolicy.h
    src/TimelineTrackPolicy.cpp
)

source_group("Playback" FILES
    src/TimelinePlaybackResolver.h
    src/TimelinePlaybackResolver.cpp
    src/PlaybackBackend.h
    src/PlaybackBackend.cpp
    src/PlaybackClockController.h
    src/PlaybackClockController.cpp
    src/QtMediaPlaybackBackend.h
    src/QtMediaPlaybackBackend.cpp
)

source_group("Controller" FILES
    src/EditorCommandController.h
    src/EditorCommandController.cpp
    src/TimelineEditingController.h
    src/TimelineEditingController.cpp
)

source_group("Presentation" FILES
    src/FrameTimecode.h
    src/PreviewStateResolver.h
    src/PreviewStateResolver.cpp
    src/ClipPropertiesStateResolver.h
    src/ClipPropertiesStateResolver.cpp
    src/TimelinePresentationStateResolver.h
    src/TimelinePresentationStateResolver.cpp
)

source_group("Services" FILES
    src/ProjectDocumentService.h
    src/ProjectDocumentService.cpp
)

if(BUILD_TESTING)
    add_executable(MiniEditorCoreTests
        tests/MiniEditorCoreTests.cpp
        src/FrameTimecode.h
        src/EditorProject.h
        src/EditorProject.cpp
        src/EditorChange.h
        src/EditorHistory.h
        src/EditorHistory.cpp
        src/ProjectSerializer.h
        src/ProjectSerializer.cpp
        src/MediaLibrary.h
        src/MediaLibrary.cpp
        src/TimelineModel.h
        src/TimelineModel.cpp
        src/TimelineGeometry.h
        src/TimelineGeometry.cpp
        src/TimelineClipEdit.h
        src/TimelineClipEdit.cpp
        src/ClipFade.h
        src/ClipFade.cpp
        src/TimelineTrackPolicy.h
        src/TimelineTrackPolicy.cpp
        src/TimelinePlaybackResolver.h
        src/TimelinePlaybackResolver.cpp
        src/EditorSession.h
        src/EditorSession.cpp
        src/EditorCommandController.h
        src/EditorCommandController.cpp
        src/PlaybackBackend.h
        src/PlaybackBackend.cpp
        src/PlaybackClockController.h
        src/PlaybackClockController.cpp
        src/TimelineEditingController.h
        src/TimelineEditingController.cpp
        src/PreviewStateResolver.h
        src/PreviewStateResolver.cpp
        src/ClipPropertiesStateResolver.h
        src/ClipPropertiesStateResolver.cpp
        src/TimelinePresentationStateResolver.h
        src/TimelinePresentationStateResolver.cpp
        src/ProjectDocumentService.h
        src/ProjectDocumentService.cpp
        src/WorkspaceLayout.h
        src/WorkspaceLayout.cpp
    )
    target_include_directories(MiniEditorCoreTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    add_test(NAME MiniEditorCoreTests COMMAND $<TARGET_FILE:MiniEditorCoreTests>)

    # Build this target explicitly when you want the full local check. Normal
    # app builds stay fast and do not run tests as an implicit post-build step.
    add_custom_target(check
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -C $<CONFIG>
        DEPENDS MiniEditorCoexistence MiniEditorCoreTests
        USES_TERMINAL
    )

    source_group("Tests" FILES tests/MiniEditorCoreTests.cpp)
endif()

target_compile_definitions(MiniEditorCoexistence PRIVATE
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MINI_EDITOR_USE_QT)
    target_sources(MiniEditorCoexistence PRIVATE
        src/TimelineAssetPresentation.h
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
        src/QtPreviewPanel.h
        src/QtPreviewPanel.cpp
        src/QtPreviewHost.h
        src/QtPreviewHost.cpp
        src/QtMediaPlaybackBackend.h
        src/QtMediaPlaybackBackend.cpp
        src/QtTimelineToolbar.h
        src/QtTimelineToolbar.cpp
        src/QtTimelineToolbarHost.h
        src/QtTimelineToolbarHost.cpp
        src/QtTimelineCanvas.h
        src/QtTimelineCanvas.cpp
        src/QtTimelineCanvasHost.h
        src/QtTimelineCanvasHost.cpp
        src/QtRuntime.h
        src/QtRuntime.cpp
    )

    target_link_libraries(MiniEditorCoexistence PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
    )
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

    if(BUILD_TESTING)
        add_executable(MiniEditorQtWidgetTests
            tests/MiniEditorQtWidgetTests.cpp
            src/ProjectState.h
            src/MediaKind.h
            src/MediaLibrary.h
            src/MediaLibrary.cpp
            src/MediaAssetModel.h
            src/MediaAssetModel.cpp
            src/QtMediaLibraryPanel.h
            src/QtMediaLibraryPanel.cpp
            src/QtPropertiesPanel.h
            src/QtPropertiesPanel.cpp
            src/ClipPropertiesStateResolver.h
            src/QtTransportPanel.h
            src/QtTransportPanel.cpp
            src/TimelineAssetPresentation.h
            src/TimelinePresentationStateResolver.h
            src/TimelineModel.h
            src/TimelineGeometry.h
            src/TimelineGeometry.cpp
            src/TimelineClipEdit.h
            src/TimelineClipEdit.cpp
            src/ClipFade.h
            src/ClipFade.cpp
            src/TimelineTrackPolicy.h
            src/TimelineTrackPolicy.cpp
            src/QtTimelineCanvas.h
            src/QtTimelineCanvas.cpp
            src/QtTimelineToolbar.h
            src/QtTimelineToolbar.cpp
        )
        target_include_directories(MiniEditorQtWidgetTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(MiniEditorQtWidgetTests PRIVATE
            Qt6::Widgets
            Qt6::Test
        )
        set_target_properties(MiniEditorQtWidgetTests PROPERTIES AUTOMOC ON)

        add_test(NAME MiniEditorQtWidgetTests
                 COMMAND $<TARGET_FILE:MiniEditorQtWidgetTests>)
        set_tests_properties(MiniEditorQtWidgetTests PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
        add_dependencies(check MiniEditorQtWidgetTests)
        source_group("Tests" FILES tests/MiniEditorQtWidgetTests.cpp)

        add_custom_command(TARGET MiniEditorQtWidgetTests POST_BUILD
            COMMAND "$<TARGET_FILE_DIR:Qt6::Core>/windeployqt.exe"
                    "$<$<CONFIG:Debug>:--debug>"
                    "$<$<NOT:$<CONFIG:Debug>>:--release>"
                    --include-plugins qoffscreen
                    "$<TARGET_FILE:MiniEditorQtWidgetTests>"
            COMMENT "Deploying the offscreen Qt runtime for widget tests"
        )
    endif()
else()
    # These are complete MFC fallbacks. They intentionally stay out of the
    # Qt-enabled target, where their corresponding Qt panels are used instead.
    target_sources(MiniEditorCoexistence PRIVATE
        src/MfcEditorPaneBase.h
        src/MfcEditorPaneBase.cpp
        src/MfcDoubleBufferedPaint.h
        src/MfcDoubleBufferedPaint.cpp
        src/MfcPreviewCanvas.h
        src/MfcPreviewCanvas.cpp
        src/MfcMediaLibraryPane.h
        src/MfcMediaLibraryPane.cpp
        src/MfcTransportBar.h
        src/MfcTransportBar.cpp
        src/MfcPropertiesPane.h
        src/MfcPropertiesPane.cpp
        src/MfcTimelinePane.h
        src/MfcTimelinePane.cpp
        src/MfcTimelineCanvas.h
        src/MfcTimelineCanvas.cpp
    )
    target_compile_definitions(MiniEditorCoexistence PRIVATE
        MINI_EDITOR_USE_QT=0
    )
endif()
