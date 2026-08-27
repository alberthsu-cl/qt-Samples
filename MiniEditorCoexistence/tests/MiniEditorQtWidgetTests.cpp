#include "MediaLibrary.h"
#include "QtMediaLibraryPanel.h"
#include "QtPropertiesPanel.h"
#include "QtTimelineCanvas.h"
#include "QtTransportPanel.h"
#include "TimelineClipEdit.h"
#include "TimelineGeometry.h"

#include <QComboBox>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QtTest>

#include <optional>

class MiniEditorQtWidgetTests final : public QObject
{
    Q_OBJECT

private slots:
    void propertiesModelRefreshDoesNotEmitUserEdit();
    void propertiesUserEditEmitsCompleteSettings();
    void transportRefreshAndButtonsUseSemanticCommands();
    void mediaLibrarySeparatesProgrammaticAndUserSelection();
    void timelineClickSeekFocusAndDeleteUseSemanticHandlers();
    void timelineBodyDragEmitsFrameBasedMove();
    void timelineEndTrimEmitsTrimmedState();
    void timelineAudioVisibilitySeparatesRefreshAndUserToggle();
};

void MiniEditorQtWidgetTests::propertiesModelRefreshDoesNotEmitUserEdit()
{
    QtPropertiesPanel panel;
    QSignalSpy editedSpy(&panel, &QtPropertiesPanel::clipSettingsEdited);

    panel.setClipSettings({ 72, 135, ClipPosition::BottomRight });

    QCOMPARE(editedSpy.count(), 0);
    QCOMPARE(panel.findChild<QSpinBox *>(QStringLiteral("opacitySpinBox"))->value(), 72);
    QCOMPARE(panel.findChild<QSpinBox *>(QStringLiteral("scaleSpinBox"))->value(), 135);
    QCOMPARE(panel.findChild<QComboBox *>(QStringLiteral("positionComboBox"))->currentData().toInt(),
             static_cast<int>(ClipPosition::BottomRight));

    panel.setEditingEnabled(false);
    QVERIFY(!panel.findChild<QSlider *>(QStringLiteral("opacitySlider"))->isEnabled());
    QVERIFY(!panel.findChild<QComboBox *>(QStringLiteral("positionComboBox"))->isEnabled());
}

void MiniEditorQtWidgetTests::propertiesUserEditEmitsCompleteSettings()
{
    QtPropertiesPanel panel;
    panel.setClipSettings({ 100, 125, ClipPosition::TopLeft });
    QSignalSpy editedSpy(&panel, &QtPropertiesPanel::clipSettingsEdited);

    panel.findChild<QSpinBox *>(QStringLiteral("opacitySpinBox"))->setValue(65);

    QCOMPARE(editedSpy.count(), 1);
    const QList<QVariant> arguments = editedSpy.takeFirst();
    QCOMPARE(arguments[0].toInt(), 65);
    QCOMPARE(arguments[1].toInt(), 125);
    QCOMPARE(arguments[2].toInt(), static_cast<int>(ClipPosition::TopLeft));
}

void MiniEditorQtWidgetTests::transportRefreshAndButtonsUseSemanticCommands()
{
    QtTransportPanel panel;
    QSignalSpy commandSpy(&panel, &QtTransportPanel::playbackCommandRequested);
    QSignalSpy positionSpy(&panel, &QtTransportPanel::playbackPositionRequested);

    PlaybackState playback;
    playback.isPlaying = true;
    playback.currentFrame = 45;
    playback.durationFrames = 90;
    playback.framesPerSecond = 30;
    panel.setPlaybackState(playback);

    QCOMPARE(commandSpy.count(), 0);
    QCOMPARE(positionSpy.count(), 0);
    QCOMPARE(panel.findChild<QToolButton *>(QStringLiteral("playPauseButton"))->text(),
             QStringLiteral("Pause"));
    QCOMPARE(panel.findChild<QSlider *>(QStringLiteral("positionSlider"))->value(), 45);
    QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("timecodeLabel"))->text(),
             QStringLiteral("00:00:01:15"));

    QTest::mouseClick(panel.findChild<QToolButton *>(QStringLiteral("playPauseButton")),
                      Qt::LeftButton);
    QTest::mouseClick(panel.findChild<QToolButton *>(QStringLiteral("stopButton")),
                      Qt::LeftButton);

    QCOMPARE(commandSpy.count(), 2);
    QCOMPARE(commandSpy.at(0)[0].toInt(),
             static_cast<int>(PlaybackCommand::TogglePlayPause));
    QCOMPARE(commandSpy.at(1)[0].toInt(), static_cast<int>(PlaybackCommand::Stop));
}

void MiniEditorQtWidgetTests::mediaLibrarySeparatesProgrammaticAndUserSelection()
{
    MediaLibrary library;
    const int videoId = library.addKnownAsset(
        L"D:/media/video.mp4", MediaKind::Video, 300, 0x5078A0);
    const int imageId = library.addKnownAsset(
        L"D:/media/image.png", MediaKind::Image, 90, 0x7850A0);
    QVERIFY(videoId != imageId);

    QtMediaLibraryPanel panel(library);
    QSignalSpy selectedSpy(&panel, &QtMediaLibraryPanel::assetSelected);
    QSignalSpy importSpy(&panel, &QtMediaLibraryPanel::importRequested);
    QSignalSpy removeSpy(&panel, &QtMediaLibraryPanel::removeRequested);

    panel.setSelectedAssetIndex(0);
    QCOMPARE(selectedSpy.count(), 0);

    QListView *assetView = panel.findChild<QListView *>(QStringLiteral("assetView"));
    QVERIFY(assetView != nullptr);
    assetView->setCurrentIndex(assetView->model()->index(1, 0));
    QCOMPARE(selectedSpy.count(), 1);
    QCOMPARE(selectedSpy.takeFirst()[0].toInt(), 1);

    QTest::mouseClick(panel.findChild<QPushButton *>(QStringLiteral("importButton")),
                      Qt::LeftButton);
    QTest::mouseClick(panel.findChild<QPushButton *>(QStringLiteral("removeButton")),
                      Qt::LeftButton);
    QCOMPARE(importSpy.count(), 1);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(removeSpy.at(0)[0].toInt(), 1);
    QCOMPARE(removeSpy.at(0)[1].toInt(), imageId);

    panel.clearSelection();
    QCOMPARE(selectedSpy.count(), 0);
    QVERIFY(!assetView->currentIndex().isValid());
}

void MiniEditorQtWidgetTests::timelineClickSeekFocusAndDeleteUseSemanticHandlers()
{
    QtTimelineCanvas canvas;
    canvas.resize(1000, TimelineGeometry::kCanvasHeight);
    const TimelineClip clip{ 7, 101, TimelineTrackType::Video,
                             { 60, 120, 0 }, {} };
    canvas.setTimelineClips({ clip });
    canvas.setTimelineDuration(600);

    int selectedClipId = 0;
    int deletedClipId = 0;
    int focusedTimelineCount = 0;
    int soughtFrame = -1;
    canvas.setTimelineClipSelectedHandler(
        [&](int clipId) { selectedClipId = clipId; });
    canvas.setTimelineClipDeletedHandler(
        [&](int clipId) { deletedClipId = clipId; });
    canvas.setTimelineFocusRequestedHandler(
        [&] { ++focusedTimelineCount; });
    canvas.setSeekHandler([&](int frame) { soughtFrame = frame; });

    const TimelineGeometry geometry(100, 600);
    const TimelineRectangle clipRect = geometry.clipRectangle(clip);
    const QPoint clipCenter(clipRect.left + clipRect.width / 2,
                            clipRect.top + clipRect.height / 2);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, clipCenter);
    QCOMPARE(selectedClipId, clip.id);

    const QPoint emptyTrackPoint(geometry.xForFrame(400), clipCenter.y());
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, emptyTrackPoint);
    QCOMPARE(focusedTimelineCount, 1);

    const QPoint rulerPoint(geometry.xForFrame(150), 10);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, rulerPoint);
    QCOMPARE(soughtFrame, geometry.rulerFrameAtX(rulerPoint.x()));

    canvas.setSelectedClipId(clip.id);
    canvas.setFocus();
    QTest::keyClick(&canvas, Qt::Key_Delete);
    QCOMPARE(deletedClipId, clip.id);
}

void MiniEditorQtWidgetTests::timelineBodyDragEmitsFrameBasedMove()
{
    QtTimelineCanvas canvas;
    canvas.resize(1000, TimelineGeometry::kCanvasHeight);
    const TimelineClip clip{ 8, 102, TimelineTrackType::Video,
                             { 60, 120, 10 }, {} };
    canvas.setTimelineClips({ clip });
    canvas.setSelectedClipId(clip.id);
    canvas.setTimelineDuration(600);

    int editedClipId = 0;
    TimelineClipState editedState;
    TimelineClipEditKind editedKind = TimelineClipEditKind::TrimStart;
    canvas.setTimelineClipEditedHandler(
        [&](int clipId, const TimelineClipState &state, TimelineClipEditKind kind) {
            editedClipId = clipId;
            editedState = state;
            editedKind = kind;
        });

    const TimelineGeometry geometry(100, 600);
    const TimelineRectangle clipRect = geometry.clipRectangle(clip);
    const QPoint pressPoint(clipRect.left + clipRect.width / 2,
                            clipRect.top + clipRect.height / 2);
    const QPoint releasePoint(geometry.xForFrame(300), pressPoint.y());
    const int frameOffset = geometry.frameAtX(pressPoint.x()) - clip.state.startFrame;
    const int expectedStart = geometry.frameAtX(releasePoint.x()) - frameOffset;

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, pressPoint);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, releasePoint);

    QCOMPARE(editedClipId, clip.id);
    QCOMPARE(editedKind, TimelineClipEditKind::Move);
    QCOMPARE(editedState.startFrame, expectedStart);
    QCOMPARE(editedState.durationFrames, clip.state.durationFrames);
    QCOMPARE(editedState.sourceInFrame, clip.state.sourceInFrame);
}

void MiniEditorQtWidgetTests::timelineEndTrimEmitsTrimmedState()
{
    QtTimelineCanvas canvas;
    canvas.resize(1000, TimelineGeometry::kCanvasHeight);
    const TimelineClip clip{ 9, 103, TimelineTrackType::Video,
                             { 60, 120, 10 }, {} };
    canvas.setTimelineClips({ clip });
    canvas.setSelectedClipId(clip.id);
    canvas.setTimelineDuration(600);
    canvas.setAssetPresentationResolver(
        [](int mediaAssetId) -> std::optional<TimelineAssetPresentation> {
            if (mediaAssetId != 103)
                return std::nullopt;
            return TimelineAssetPresentation{
                QStringLiteral("Video"), QColor(80, 120, 160),
                TimelineTrackType::Video, MediaKind::Video, 600
            };
        });

    TimelineClipState editedState;
    TimelineClipEditKind editedKind = TimelineClipEditKind::Move;
    canvas.setTimelineClipEditedHandler(
        [&](int, const TimelineClipState &state, TimelineClipEditKind kind) {
            editedState = state;
            editedKind = kind;
        });

    const TimelineGeometry geometry(100, 600);
    const TimelineRectangle clipRect = geometry.clipRectangle(clip);
    const QPoint trimHandle(clipRect.left + clipRect.width - 2,
                            clipRect.top + clipRect.height / 2);
    const QPoint extendedEnd(geometry.xForFrame(clip.state.startFrame
                                                 + clip.state.durationFrames + 30),
                             trimHandle.y());

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, trimHandle);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, extendedEnd);

    QCOMPARE(editedKind, TimelineClipEditKind::TrimEnd);
    QCOMPARE(editedState.startFrame, clip.state.startFrame);
    QCOMPARE(editedState.durationFrames,
             geometry.frameAtXUnclamped(extendedEnd.x()) - clip.state.startFrame);
    QCOMPARE(editedState.sourceInFrame, clip.state.sourceInFrame);
}

void MiniEditorQtWidgetTests::timelineAudioVisibilitySeparatesRefreshAndUserToggle()
{
    QtTimelineCanvas canvas;
    canvas.resize(1000, TimelineGeometry::kCanvasHeight);
    int visibilityEditCount = 0;
    bool requestedVisibility = true;
    canvas.setAudioTrackVisibilityHandler([&](bool isVisible) {
        ++visibilityEditCount;
        requestedVisibility = isVisible;
    });

    TimelineViewState state;
    state.isAudioTrackVisible = true;
    canvas.setViewState(state);
    QCOMPARE(visibilityEditCount, 0);

    QToolButton *visibilityButton = canvas.findChild<QToolButton *>(
        QStringLiteral("audioTrackVisibilityButton"));
    QVERIFY(visibilityButton != nullptr);
    QVERIFY(visibilityButton->isChecked());
    QTest::mouseClick(visibilityButton, Qt::LeftButton);
    QCOMPARE(visibilityEditCount, 1);
    QVERIFY(!requestedVisibility);

    state.isAudioTrackVisible = false;
    canvas.setViewState(state);
    QCOMPARE(visibilityEditCount, 1);
    QVERIFY(!visibilityButton->isChecked());
}

QTEST_MAIN(MiniEditorQtWidgetTests)

#include "MiniEditorQtWidgetTests.moc"
