#include "MediaLibrary.h"
#include "MediaAssetModel.h"
#include "QtMediaLibraryPanel.h"
#include "QtPropertiesPanel.h"
#include "QtPreviewPanel.h"
#include "QtTimelineCanvas.h"
#include "QtTimelineToolbar.h"
#include "QtTransportPanel.h"
#include "QtThumbnailCache.h"
#include "TimelineClipEdit.h"
#include "TimelineGeometry.h"

#include <QComboBox>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

#include <optional>
#include <filesystem>

class MiniEditorQtWidgetTests final : public QObject
{
    Q_OBJECT

private slots:
    void propertiesModelRefreshDoesNotEmitUserEdit();
    void propertiesUserEditEmitsCompleteSettings();
    void propertiesFadeEditorsRespectTheClipDuration();
    void realVideoPreviewUsesTimelineClipPresentation();
    void transportRefreshAndButtonsUseSemanticCommands();
    void mediaLibrarySeparatesProgrammaticAndUserSelection();
    void mediaLibraryModelExposesDecodedRealImageThumbnail();
    void timelineClickSeekFocusAndDeleteUseSemanticHandlers();
    void timelineBodyDragEmitsFrameBasedMove();
    void timelineEndTrimEmitsTrimmedState();
    void timelineAudioVisibilitySeparatesRefreshAndUserToggle();
    void timelinePresentationRefreshUpdatesToolbarAtomically();
};

void MiniEditorQtWidgetTests::mediaLibraryModelExposesDecodedRealImageThumbnail()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("thumbnail.png"));
    QImage sourceImage(16, 9, QImage::Format_ARGB32);
    sourceImage.fill(QColor(220, 40, 30));
    QVERIFY(sourceImage.save(imagePath));

    MediaLibrary library;
    const std::optional<int> assetId = library.addFile(
        std::filesystem::path(imagePath.toStdWString()));
    QVERIFY(assetId.has_value());

    QtThumbnailCache cache;
    cache.refresh(library);
    MediaAssetModel model(library, &cache);
    const QImage thumbnail = model.index(0, 0)
        .data(MediaAssetModel::ThumbnailImageRole).value<QImage>();
    QVERIFY(!thumbnail.isNull());
    QCOMPARE(thumbnail.size(), sourceImage.size());
}

void MiniEditorQtWidgetTests::propertiesModelRefreshDoesNotEmitUserEdit()
{
    QtPropertiesPanel panel;
    QSignalSpy editedSpy(&panel, &QtPropertiesPanel::clipSettingsEdited);

    ClipPropertiesViewState viewState;
    viewState.target = ClipPropertiesTarget::TimelineClip;
    viewState.editingEnabled = true;
    viewState.mediaKind = MediaKind::Audio;
    viewState.durationFrames = 90;
    viewState.settings = { 72, 135, ClipPosition::BottomRight, 20, 10 };
    panel.setViewState(viewState);

    QCOMPARE(editedSpy.count(), 0);
    QCOMPARE(panel.findChild<QSpinBox *>(QStringLiteral("opacitySpinBox"))->value(), 72);
    QCOMPARE(panel.findChild<QSpinBox *>(QStringLiteral("scaleSpinBox"))->value(), 135);
    QCOMPARE(panel.findChild<QComboBox *>(QStringLiteral("positionComboBox"))->currentData().toInt(),
             static_cast<int>(ClipPosition::BottomRight));
    QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("fadeSummaryLabel"))->text(),
             QStringLiteral("60 of 90 frames at full level"));
    QCOMPARE(panel.findChild<QSpinBox *>(QStringLiteral("fadeInSpinBox"))->toolTip(),
             QStringLiteral("Frames the clip takes to ramp up from silence."));
    QVERIFY(panel.findChild<QWidget *>(QStringLiteral("opacityEditor"))->isHidden());
    QVERIFY(panel.findChild<QWidget *>(QStringLiteral("scaleEditor"))->isHidden());
    QVERIFY(panel.findChild<QComboBox *>(
                QStringLiteral("positionComboBox"))->isHidden());
    QCOMPARE(editedSpy.count(), 0);

    viewState.target = ClipPropertiesTarget::MediaAsset;
    viewState.mediaDisplayName = L"Narration.wav";
    viewState.mediaFilePath = L"D:/media/Narration.wav";
    panel.setViewState(viewState);
    QVERIFY(panel.findChild<QWidget *>(
                QStringLiteral("propertiesFormContainer"))->isHidden());
    QVERIFY(panel.findChild<QWidget *>(QStringLiteral("opacityEditor"))->isHidden());
    QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("selectionMessageLabel"))->text(),
             QStringLiteral("Name: Narration.wav\nType: Audio\nDuration: 00:03:00\n"
                            "Source: D:/media/Narration.wav\n\n"
                            "Add this media to the timeline to edit placement properties."));
    QCOMPARE(editedSpy.count(), 0);

    viewState.target = ClipPropertiesTarget::TimelineClip;
    viewState.mediaKind = MediaKind::Image;
    panel.setViewState(viewState);
    QVERIFY(!panel.findChild<QWidget *>(
                 QStringLiteral("propertiesFormContainer"))->isHidden());
    QVERIFY(!panel.findChild<QWidget *>(QStringLiteral("opacityEditor"))->isHidden());
    QVERIFY(!panel.findChild<QWidget *>(QStringLiteral("scaleEditor"))->isHidden());
    QVERIFY(!panel.findChild<QComboBox *>(
                 QStringLiteral("positionComboBox"))->isHidden());
    QCOMPARE(panel.findChild<QSpinBox *>(QStringLiteral("fadeInSpinBox"))->toolTip(),
             QStringLiteral("Frames the clip takes to ramp up from transparent."));
    QCOMPARE(editedSpy.count(), 0);

    viewState.editingEnabled = false;
    panel.setViewState(viewState);
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
    QCOMPARE(arguments[3].toInt(), 0);
    QCOMPARE(arguments[4].toInt(), 0);
}

void MiniEditorQtWidgetTests::propertiesFadeEditorsRespectTheClipDuration()
{
    QtPropertiesPanel panel;
    auto *fadeInSlider = panel.findChild<QSlider *>(QStringLiteral("fadeInSlider"));
    auto *fadeIn = panel.findChild<QSpinBox *>(QStringLiteral("fadeInSpinBox"));
    auto *fadeOutSlider = panel.findChild<QSlider *>(QStringLiteral("fadeOutSlider"));
    auto *fadeOut = panel.findChild<QSpinBox *>(QStringLiteral("fadeOutSpinBox"));
    auto *summary = panel.findChild<QLabel *>(QStringLiteral("fadeSummaryLabel"));
    QVERIFY(fadeInSlider != nullptr && fadeIn != nullptr);
    QVERIFY(fadeOutSlider != nullptr && fadeOut != nullptr && summary != nullptr);

    QSignalSpy editedSpy(&panel, &QtPropertiesPanel::clipSettingsEdited);
    panel.setClipDurationFrames(90);
    ClipSettings settings;
    settings.fadeInFrames = 20;
    settings.fadeOutFrames = 10;
    panel.setClipSettings(settings);

    QCOMPARE(fadeIn->value(), 20);
    QCOMPARE(fadeInSlider->value(), 20);
    QCOMPARE(fadeOut->value(), 10);
    QCOMPARE(fadeOutSlider->value(), 10);
    QCOMPARE(summary->text(),
             QStringLiteral("60 of 90 frames at full opacity"));
    QCOMPARE(editedSpy.count(), 0);

    // Each editor spans the whole clip, so a stepper or slider never stops
    // responding because the other fade happens to be long.
    QCOMPARE(fadeIn->maximum(), 90);
    QCOMPARE(fadeInSlider->maximum(), 90);
    QCOMPARE(fadeOut->maximum(), 90);
    QCOMPARE(fadeOutSlider->maximum(), 90);

    // The fade being dragged wins; the other one yields so the pair still fits.
    fadeInSlider->setValue(85);
    QCOMPARE(fadeIn->value(), 85);
    QCOMPARE(fadeOut->value(), 5);
    QCOMPARE(fadeOutSlider->value(), 5);
    QCOMPARE(summary->text(),
             QStringLiteral("0 of 90 frames at full opacity"));
    QCOMPARE(editedSpy.count(), 1);
    const QList<QVariant> fadeArguments = editedSpy.takeFirst();
    QCOMPARE(fadeArguments[3].toInt(), 85);
    QCOMPARE(fadeArguments[4].toInt(), 5);

    // Trimming the clip shorter keeps the stored pair valid and proportional,
    // and a model refresh must never look like a user edit.
    panel.setClipDurationFrames(45);
    QCOMPARE(fadeIn->value(), 42);
    QCOMPARE(fadeOut->value(), 3);
    QCOMPARE(fadeIn->maximum(), 45);
    QCOMPARE(editedSpy.count(), 0);

    panel.setEditingEnabled(false);
    QVERIFY(!fadeIn->isEnabled());
    QVERIFY(!fadeInSlider->isEnabled());
    QVERIFY(!fadeOut->isEnabled());
    QVERIFY(!fadeOutSlider->isEnabled());
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
    QVERIFY(panel.findChild<QWidget *>(QStringLiteral("transportControls")) != nullptr);
    QCOMPARE(panel.findChild<QToolButton *>(QStringLiteral("stepBackwardButton"))->text(),
             QStringLiteral("<"));
    QCOMPARE(panel.findChild<QToolButton *>(QStringLiteral("stepForwardButton"))->text(),
             QStringLiteral(">"));
    QCOMPARE(panel.findChild<QSlider *>(QStringLiteral("positionSlider"))->value(), 45);
    QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("timecodeLabel"))->text(),
             QStringLiteral("00:00:01:15"));

    panel.show();
    panel.resize(350, 52);
    QVERIFY(panel.findChild<QToolButton *>(QStringLiteral("stepBackwardButton"))->isHidden());
    QVERIFY(panel.findChild<QToolButton *>(QStringLiteral("stepForwardButton"))->isHidden());
    QVERIFY(panel.findChild<QLabel *>(QStringLiteral("timecodeLabel"))->isHidden());
    panel.resize(700, 52);
    QVERIFY(!panel.findChild<QToolButton *>(QStringLiteral("stepBackwardButton"))->isHidden());
    QVERIFY(!panel.findChild<QToolButton *>(QStringLiteral("stepForwardButton"))->isHidden());
    QVERIFY(!panel.findChild<QLabel *>(QStringLiteral("timecodeLabel"))->isHidden());

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
    QTemporaryFile realMediaFile;
    QVERIFY(realMediaFile.open());
    const int realVideoId = library.addKnownAsset(
        std::filesystem::path(realMediaFile.fileName().toStdWString()),
        MediaKind::Video, 300, 0x4f91b8);
    QVERIFY(videoId != imageId && imageId != realVideoId);

    QtMediaLibraryPanel panel(library);
    QSignalSpy selectedSpy(&panel, &QtMediaLibraryPanel::assetSelected);
    QSignalSpy importSpy(&panel, &QtMediaLibraryPanel::importRequested);
    QSignalSpy removeSpy(&panel, &QtMediaLibraryPanel::removeRequested);

    panel.setSelectedAssetIndex(0);
    QCOMPARE(selectedSpy.count(), 0);

    QListView *assetView = panel.findChild<QListView *>(QStringLiteral("assetView"));
    QVERIFY(assetView != nullptr);
    auto *sourceFilter = panel.findChild<QComboBox *>(
        QStringLiteral("mediaSourceFilterComboBox"));
    QVERIFY(sourceFilter != nullptr);
    QCOMPARE(sourceFilter->count(), 3);
    QCOMPARE(sourceFilter->itemText(0), QStringLiteral("All"));
    QCOMPARE(sourceFilter->itemText(1), QStringLiteral("Real"));
    QCOMPARE(sourceFilter->itemText(2), QStringLiteral("Fake"));
    // The library opens on real project media, not the fake/sample assets.
    QCOMPARE(sourceFilter->currentIndex(), 1);
    QCOMPARE(assetView->model()->rowCount(), 1);
    QCOMPARE(assetView->model()->index(0, 0)
                 .data(MediaAssetModel::AssetIndexRole).toInt(), 2);
    sourceFilter->setCurrentIndex(2);
    QCOMPARE(assetView->model()->rowCount(), 2);
    sourceFilter->setCurrentIndex(0);
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

    MediaLibrary emptyLibrary;
    QtMediaLibraryPanel emptyPanel(emptyLibrary);
    auto *emptyImportButton = emptyPanel.findChild<QToolButton *>(
        QStringLiteral("emptyImportButton"));
    QVERIFY(emptyImportButton != nullptr);
    QVERIFY(emptyImportButton->text().isEmpty());
    QSignalSpy emptyImportSpy(&emptyPanel, &QtMediaLibraryPanel::importRequested);
    QTest::mouseClick(emptyImportButton, Qt::LeftButton);
    QCOMPARE(emptyImportSpy.count(), 1);
}

void MiniEditorQtWidgetTests::realVideoPreviewUsesTimelineClipPresentation()
{
    QtPreviewPanel panel;
    panel.resize(640, 400);

    PreviewState state;
    state.hasMedia = true;
    state.mediaKind = MediaKind::Video;
    state.settings.scalePercent = 50;
    state.settings.position = ClipPosition::BottomRight;
    state.effectiveOpacityPercent = 42;
    panel.setPreviewState(state);

    // The decoder targets our QVideoSink. QtPreviewPanel paints the received
    // frame itself, which is what allows timeline opacity and placement.
    QVERIFY(panel.videoSink() != nullptr);
}

void MiniEditorQtWidgetTests::timelineClickSeekFocusAndDeleteUseSemanticHandlers()
{
    QtTimelineCanvas canvas;
    canvas.resize(1000, TimelineGeometry::kCanvasHeight);
    const TimelineClip clip{ 7, 101, TimelineTrackType::Video,
                             { 60, 120, 0 }, {} };
    TimelinePresentationState presentation;
    presentation.clips = { clip };
    presentation.durationFrames = 600;
    canvas.setPresentationState(presentation);

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

    presentation.selectedClipId = clip.id;
    canvas.setPresentationState(presentation);
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
    QCOMPARE(visibilityButton->iconSize(), QSize(20, 20));
    QVERIFY(!visibilityButton->icon().isNull());
    QVERIFY(visibilityButton->isChecked());
    QTest::mouseClick(visibilityButton, Qt::LeftButton);
    QCOMPARE(visibilityEditCount, 1);
    QVERIFY(!requestedVisibility);

    state.isAudioTrackVisible = false;
    canvas.setViewState(state);
    QCOMPARE(visibilityEditCount, 1);
    QVERIFY(!visibilityButton->isChecked());
}

void MiniEditorQtWidgetTests::timelinePresentationRefreshUpdatesToolbarAtomically()
{
    QtTimelineToolbar toolbar;
    QSignalSpy editedSpy(&toolbar, &QtTimelineToolbar::viewStateEdited);

    TimelinePresentationState state;
    state.view.zoomPercent = 175;
    state.view.isAudioTrackVisible = false;
    state.view.isRippleEditingEnabled = true;
    state.splitEnabled = true;
    toolbar.setPresentationState(state);

    QCOMPARE(editedSpy.count(), 0);
    QCOMPARE(toolbar.findChild<QSlider *>(
                 QStringLiteral("timelineZoomSlider"))->value(), 175);
    QVERIFY(toolbar.findChild<QToolButton *>(
                QStringLiteral("timelineRippleButton"))->isChecked());
    QVERIFY(toolbar.findChild<QToolButton *>(
                QStringLiteral("timelineSplitButton"))->isEnabled());
}

QTEST_MAIN(MiniEditorQtWidgetTests)

#include "MiniEditorQtWidgetTests.moc"
