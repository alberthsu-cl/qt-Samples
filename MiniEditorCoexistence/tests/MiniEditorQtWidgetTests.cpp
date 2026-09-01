#include "MediaLibrary.h"
#include "MediaAssetModel.h"
#include "QtMediaLibraryPanel.h"
#include "QtAudioWaveformCache.h"
#include "QtPropertiesPanel.h"
#include "QtFrameEffectProcessor.h"
#include "QtPreviewEffectPipeline.h"
#include "QtPreviewPanel.h"
#include "QtTimelineCanvas.h"
#include "QtTimelineToolbar.h"
#include "QtTransportPanel.h"
#include "QtThumbnailCache.h"
#include "TimelineClipEdit.h"
#include "TimelineGeometry.h"

#include <QComboBox>
#include <QDataStream>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSignalSpy>
#include <QThread>
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
    void dspControlsEditCompleteClipSettings();
    void previewEffectPipelineProcessesOffTheUiThreadAndConflates();
    void realVideoPreviewUsesTimelineClipPresentation();
    void realStillImagePlaybackDoesNotPaintAudioTimeOverlay();
    void transportRefreshAndButtonsUseSemanticCommands();
    void mediaLibrarySeparatesProgrammaticAndUserSelection();
    void mediaLibraryModelExposesDecodedRealImageThumbnail();
    void mediaLibraryPaintsAudioArtworkWithoutDecodedThumbnail();
    void timelineThumbnailCacheRegeneratesPerClipEffects();
    void timelineCanvasRequestsDistinctTrimmedSourceFrames();
    void timelineCanvasPaintsResolvedAudioWaveform();
    void audioWaveformCacheDecodesRealPcm();
    void timelineClickSeekFocusAndDeleteUseSemanticHandlers();
    void timelineBodyDragEmitsFrameBasedMove();
    void timelineEndTrimEmitsTrimmedState();
    void timelineAudioVisibilitySeparatesRefreshAndUserToggle();
    void timelineVideoAudioMuteSeparatesRefreshAndUserToggle();
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

void MiniEditorQtWidgetTests::mediaLibraryPaintsAudioArtworkWithoutDecodedThumbnail()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString audioPath = directory.filePath(QStringLiteral("music.mp3"));
    QFile audioFile(audioPath);
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.close();

    MediaLibrary library;
    QVERIFY(library.addFile(
        std::filesystem::path(audioPath.toStdWString())).has_value());

    QtMediaLibraryPanel panel(library);
    panel.resize(360, 280);
    panel.show();
    QCoreApplication::processEvents();

    QListView *assetView = panel.findChild<QListView *>(
        QStringLiteral("assetView"));
    QVERIFY(assetView != nullptr);
    QCOMPARE(assetView->model()->rowCount(), 1);
    const QRect itemRect = assetView->visualRect(assetView->model()->index(0, 0));
    QVERIFY(itemRect.isValid());
    const QImage paintedItem = assetView->viewport()->grab(itemRect).toImage();

    int cyanPixelCount = 0;
    for (int y = 0; y < paintedItem.height(); ++y) {
        for (int x = 0; x < paintedItem.width(); ++x) {
            const QColor pixel = paintedItem.pixelColor(x, y);
            if (pixel.red() < 80 && pixel.green() > 130 && pixel.blue() > 160)
                ++cyanPixelCount;
        }
    }
    QVERIFY2(cyanPixelCount > 30,
             "Audio cards must paint recognizable cyan waveform artwork.");
}

void MiniEditorQtWidgetTests::timelineThumbnailCacheRegeneratesPerClipEffects()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("effect-source.png"));
    QImage sourceImage(16, 9, QImage::Format_ARGB32);
    sourceImage.fill(QColor(220, 40, 30));
    QVERIFY(sourceImage.save(imagePath));

    MediaLibrary library;
    const std::optional<int> assetId = library.addFile(
        std::filesystem::path(imagePath.toStdWString()));
    QVERIFY(assetId.has_value());

    QtThumbnailCache cache;
    cache.refresh(library);
    const QImage libraryThumbnail = cache.imageFor(*assetId);
    QVERIFY(!libraryThumbnail.isNull());

    TimelineClip affectedClip;
    affectedClip.id = 11;
    affectedClip.mediaAssetId = *assetId;
    affectedClip.trackType = TimelineTrackType::Video;
    affectedClip.settings.effect = ClipEffectKind::Grayscale;
    affectedClip.settings.effectIntensityPercent = 100;

    TimelineClip originalClip = affectedClip;
    originalClip.id = 12;
    originalClip.settings.effect = ClipEffectKind::None;

    cache.prepareTimelineThumbnails({ affectedClip, originalClip }, 100);
    const QImage grayscale = cache.timelineImageFor(affectedClip);
    const int expectedGray = qGray(qRgb(220, 40, 30));
    QCOMPARE(qRed(grayscale.pixel(0, 0)), expectedGray);
    QCOMPARE(qGreen(grayscale.pixel(0, 0)), expectedGray);
    QCOMPARE(qBlue(grayscale.pixel(0, 0)), expectedGray);

    // A second placement and Media Library still expose the original source.
    QCOMPARE(cache.timelineImageFor(originalClip).pixelColor(0, 0),
             QColor(220, 40, 30));
    QCOMPARE(cache.imageFor(*assetId).pixelColor(0, 0), QColor(220, 40, 30));

    // Changing the same clip's DSP settings replaces its derived cache entry.
    affectedClip.settings.effect = ClipEffectKind::Invert;
    cache.prepareTimelineThumbnails({ affectedClip, originalClip }, 100);
    const QImage inverted = cache.timelineImageFor(affectedClip);
    QCOMPARE(inverted.pixelColor(0, 0), QColor(35, 215, 225));

    // Adding an unrelated library asset must not discard the strip cache for
    // an existing timeline placement.
    const QString importedImagePath = directory.filePath(QStringLiteral("new-media.png"));
    QImage importedImage(16, 9, QImage::Format_ARGB32);
    importedImage.fill(QColor(30, 160, 220));
    QVERIFY(importedImage.save(importedImagePath));
    QVERIFY(library.addFile(
        std::filesystem::path(importedImagePath.toStdWString())).has_value());
    cache.refresh(library);
    QCOMPARE(cache.timelineImageFor(affectedClip).pixelColor(0, 0),
             QColor(35, 215, 225));
}

void MiniEditorQtWidgetTests::timelineCanvasRequestsDistinctTrimmedSourceFrames()
{
    QtTimelineCanvas canvas;
    canvas.resize(1000, TimelineGeometry::kCanvasHeight);

    TimelineClip clip{ 27, 101, TimelineTrackType::Video,
                       { 0, 240, 60 }, {} };
    TimelinePresentationState state;
    state.clips = { clip };
    state.durationFrames = 600;
    canvas.setPresentationState(state);
    canvas.setAssetPresentationResolver([](int) {
        TimelineAssetPresentation presentation;
        presentation.displayName = QStringLiteral("Trimmed video");
        presentation.color = QColor(60, 80, 110);
        presentation.mediaKind = MediaKind::Video;
        return std::optional<TimelineAssetPresentation>(presentation);
    });

    std::vector<int> requestedSourceFrames;
    QImage thumbnail(16, 9, QImage::Format_RGB32);
    thumbnail.fill(QColor(220, 80, 40));
    canvas.setClipThumbnailResolver(
        [&requestedSourceFrames, &thumbnail](const TimelineClip &, int sourceFrame) {
            requestedSourceFrames.push_back(sourceFrame);
            return thumbnail;
        });

    canvas.show();
    QCoreApplication::processEvents();
    canvas.grab();

    QVERIFY(requestedSourceFrames.size() > 1);
    QCOMPARE(requestedSourceFrames.front(), 60);
    QVERIFY(std::any_of(requestedSourceFrames.begin(), requestedSourceFrames.end(),
                        [](int frame) { return frame > 60; }));
}

void MiniEditorQtWidgetTests::timelineCanvasPaintsResolvedAudioWaveform()
{
    QtTimelineCanvas canvas;
    canvas.resize(1000, TimelineGeometry::kCanvasHeight);

    TimelineClip clip{ 31, 202, TimelineTrackType::Audio,
                       { 0, 240, 0 }, {} };
    TimelinePresentationState state;
    state.clips = { clip };
    state.durationFrames = 600;
    state.view.isAudioTrackVisible = true;
    canvas.setPresentationState(state);
    canvas.setAssetPresentationResolver([](int) {
        TimelineAssetPresentation presentation;
        presentation.displayName = QStringLiteral("Real audio");
        presentation.color = QColor(68, 63, 137);
        presentation.mediaKind = MediaKind::Audio;
        presentation.isRealAsset = true;
        return std::optional<TimelineAssetPresentation>(presentation);
    });
    canvas.setAudioWaveformResolver(
        [](const TimelineClip &, int pixelWidth) {
            std::vector<AudioWaveformPeak> peaks(pixelWidth);
            for (int x = 0; x < pixelWidth; ++x) {
                const float amplitude = (x % 12 < 6) ? 0.25F : 0.85F;
                peaks[x] = { -amplitude, amplitude };
            }
            return std::make_shared<const std::vector<AudioWaveformPeak>>(
                std::move(peaks));
        });

    canvas.show();
    QCoreApplication::processEvents();
    const QImage paintedCanvas = canvas.grab().toImage();

    const TimelineGeometry geometry(100, state.durationFrames);
    const TimelineRectangle clipRectangle = geometry.clipRectangle(clip);
    int waveformPixelCount = 0;
    for (int y = clipRectangle.top + 3;
         y < clipRectangle.top + clipRectangle.height - 3; ++y) {
        for (int x = clipRectangle.left + 2;
             x < clipRectangle.left + clipRectangle.width - 2; ++x) {
            const QColor pixel = paintedCanvas.pixelColor(x, y);
            if (pixel.red() > 150 && pixel.blue() > 200)
                ++waveformPixelCount;
        }
    }
    QVERIFY2(waveformPixelCount > 100,
             "A1 must paint the PCM peak columns supplied by the waveform cache.");
}

void MiniEditorQtWidgetTests::audioWaveformCacheDecodesRealPcm()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString audioPath = directory.filePath(QStringLiteral("levels.wav"));

    constexpr quint32 sampleRate = 8000;
    constexpr quint16 channelCount = 1;
    constexpr quint16 bitsPerSample = 16;
    constexpr quint32 sampleCount = sampleRate;
    constexpr quint32 bytesPerSample = bitsPerSample / 8;
    constexpr quint32 pcmByteCount = sampleCount * bytesPerSample;

    QFile audioFile(audioPath);
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    QDataStream stream(&audioFile);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + pcmByteCount);
    stream.writeRawData("WAVEfmt ", 8);
    stream << quint32(16) << quint16(1) << channelCount << sampleRate
           << quint32(sampleRate * channelCount * bytesPerSample)
           << quint16(channelCount * bytesPerSample) << bitsPerSample;
    stream.writeRawData("data", 4);
    stream << pcmByteCount;
    for (quint32 sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        const qint16 amplitude = sampleIndex < sampleCount / 2 ? 2000 : 24000;
        stream << qint16(sampleIndex % 2 == 0 ? amplitude : -amplitude);
    }
    audioFile.close();

    MediaLibrary library;
    const std::optional<int> assetId = library.addFile(
        std::filesystem::path(audioPath.toStdWString()));
    QVERIFY(assetId.has_value());

    QtAudioWaveformCache cache;
    QSignalSpy waveformSpy(&cache, &QtAudioWaveformCache::waveformChanged);
    cache.refresh(library);
    // Placing an asset while its library preload is still pending queues a
    // newer generation. The stale preload must not suppress the A1 result.
    cache.requestForTimeline(*assetId);
    QVERIFY2(waveformSpy.wait(5000), "The WAV decoder did not produce PCM peaks.");

    TimelineClip clip{ 41, *assetId, TimelineTrackType::Audio,
                       { 0, 30, 0 }, {} };
    const SharedAudioWaveform peaks = cache.waveformForClip(clip, 80);
    QVERIFY(peaks != nullptr);
    QCOMPARE(peaks->size(), std::size_t(80));
    QVERIFY((*peaks)[10].maximum < 0.2F);
    QVERIFY((*peaks)[70].maximum > 0.6F);
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
    // Opacity is one row inside a group that audio still uses, so that row is
    // hidden on its own. Scale and Position are the whole Size / Position
    // group, which is hidden as a unit rather than leaving a titled empty box.
    // isHidden() only reports an explicit hide, so the rows inside a hidden
    // group must be checked with isVisibleTo().
    QVERIFY(panel.findChild<QWidget *>(QStringLiteral("opacityEditor"))->isHidden());
    QVERIFY(panel.findChild<QGroupBox *>(QStringLiteral("sizePositionGroup"))->isHidden());
    QVERIFY(panel.findChild<QGroupBox *>(QStringLiteral("dspGroup"))->isHidden());
    QVERIFY(!panel.findChild<QWidget *>(
                QStringLiteral("scaleEditor"))->isVisibleTo(&panel));
    QVERIFY(!panel.findChild<QComboBox *>(
                QStringLiteral("positionComboBox"))->isVisibleTo(&panel));
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
    QCOMPARE(arguments[5].toInt(), static_cast<int>(ClipEffectKind::None));
    QCOMPARE(arguments[6].toInt(), 100);
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

void MiniEditorQtWidgetTests::dspControlsEditCompleteClipSettings()
{
    QtPropertiesPanel panel;
    auto *effectComboBox = panel.findChild<QComboBox *>(QStringLiteral("effectComboBox"));
    auto *intensitySpinBox =
        panel.findChild<QSpinBox *>(QStringLiteral("effectIntensitySpinBox"));
    auto *intensitySlider =
        panel.findChild<QSlider *>(QStringLiteral("effectIntensitySlider"));
    auto *intensityEditor =
        panel.findChild<QWidget *>(QStringLiteral("effectIntensityEditor"));
    QVERIFY(effectComboBox != nullptr && intensitySpinBox != nullptr);
    QVERIFY(intensitySlider != nullptr && intensityEditor != nullptr);

    QSignalSpy clipSettingsSpy(&panel, &QtPropertiesPanel::clipSettingsEdited);

    // Applying stored clip state must not look like a user edit, and an
    // inactive effect must disable its own intensity control.
    ClipSettings settings;
    panel.setEditingEnabled(true);
    panel.setClipSettings(settings);
    QCOMPARE(clipSettingsSpy.count(), 0);
    QVERIFY(!intensityEditor->isEnabled());

    settings.effect = ClipEffectKind::Blur;
    settings.effectIntensityPercent = 40;
    panel.setClipSettings(settings);
    QCOMPARE(clipSettingsSpy.count(), 0);
    QCOMPARE(effectComboBox->currentData().toInt(),
             static_cast<int>(ClipEffectKind::Blur));
    QCOMPARE(intensitySpinBox->value(), 40);
    QCOMPARE(intensitySlider->value(), 40);
    QVERIFY(intensityEditor->isEnabled());

    intensitySlider->setValue(75);
    QCOMPARE(clipSettingsSpy.count(), 1);
    const QList<QVariant> intensityEdit = clipSettingsSpy.takeFirst();
    QCOMPARE(intensityEdit[5].toInt(), static_cast<int>(ClipEffectKind::Blur));
    QCOMPARE(intensityEdit[6].toInt(), 75);

    effectComboBox->setCurrentIndex(
        effectComboBox->findData(static_cast<int>(ClipEffectKind::Grayscale)));
    QCOMPARE(clipSettingsSpy.count(), 1);
    QCOMPARE(clipSettingsSpy.takeFirst()[5].toInt(),
             static_cast<int>(ClipEffectKind::Grayscale));
}

void MiniEditorQtWidgetTests::previewEffectPipelineProcessesOffTheUiThreadAndConflates()
{
    QImage source(8, 8, QImage::Format_ARGB32);
    source.fill(QColor(200, 100, 50));

    // The worker's own function is checked first, so a later threading failure
    // cannot be mistaken for an arithmetic one.
    const QImage grayscale = QtFrameEffectProcessor::applyEffect(
        source, ClipEffectKind::Grayscale, 100);
    const int expectedGray = qGray(qRgb(200, 100, 50));
    QCOMPARE(qRed(grayscale.pixel(0, 0)), expectedGray);
    QCOMPARE(qGreen(grayscale.pixel(0, 0)), expectedGray);
    QCOMPARE(qBlue(grayscale.pixel(0, 0)), expectedGray);

    // Intensity blends the effect back over the untouched frame.
    const QImage halfGrayscale = QtFrameEffectProcessor::applyEffect(
        source, ClipEffectKind::Grayscale, 50);
    QCOMPARE(qRed(halfGrayscale.pixel(0, 0)), 200 + (expectedGray - 200) / 2);

    // An inactive effect must return the frame untouched, so the preview can
    // skip the thread hop entirely.
    QCOMPARE(QtFrameEffectProcessor::applyEffect(source, ClipEffectKind::None, 100),
             source);
    QCOMPARE(QtFrameEffectProcessor::applyEffect(source, ClipEffectKind::Invert, 0),
             source);

    // The worker really must run away from this thread. Ported from
    // ThreadedEffectPreview's tst_frameprocessor: move the worker onto a real
    // QThread, queue a request, and record where the result was emitted. The
    // direct connection runs the observer in the emitting thread.
    {
        QThread workerThread;
        QtFrameEffectProcessor processor;
        processor.moveToThread(&workerThread);
        QThread *observedThread = nullptr;
        QObject::connect(&processor, &QtFrameEffectProcessor::frameProcessed,
                         &processor,
                         [&observedThread](int, const QImage &) {
                             observedThread = QThread::currentThread();
                         },
                         Qt::DirectConnection);
        QSignalSpy workerSpy(&processor, &QtFrameEffectProcessor::frameProcessed);
        workerThread.start();
        QVERIFY(QMetaObject::invokeMethod(&processor, "processFrame",
                                          Qt::QueuedConnection,
                                          Q_ARG(int, 7), Q_ARG(QImage, source),
                                          Q_ARG(ClipEffectKind, ClipEffectKind::Grayscale),
                                          Q_ARG(int, 100)));
        QVERIFY(workerSpy.wait(5000));
        QCOMPARE(workerSpy.takeFirst().first().toInt(), 7);
        QVERIFY(observedThread != nullptr);
        QVERIFY(observedThread != QThread::currentThread());
        QCOMPARE(observedThread, &workerThread);
        workerThread.requestInterruption();
        workerThread.quit();
        QVERIFY(workerThread.wait(5000));
    }

    QtPreviewEffectPipeline pipeline;
    QSignalSpy processedSpy(&pipeline, &QtPreviewEffectPipeline::frameProcessed);

    QVERIFY(!pipeline.submit(source, ClipEffectKind::None, 100));
    QVERIFY(pipeline.submit(source, ClipEffectKind::Invert, 100));
    QVERIFY(processedSpy.wait(5000));
    const QImage result = processedSpy.takeFirst().first().value<QImage>();
    QCOMPARE(qRed(result.pixel(0, 0)), 55);
    QCOMPARE(qGreen(result.pixel(0, 0)), 155);
    QCOMPARE(qBlue(result.pixel(0, 0)), 205);

    // A live preview cannot stop producing frames, so the pipeline conflates:
    // while the worker is busy it keeps only the newest frame and drops the
    // rest instead of growing an unbounded queue.
    const int droppedBefore = pipeline.droppedFrameCount();
    for (int submission = 0; submission < 12; ++submission)
        QVERIFY(pipeline.submit(source, ClipEffectKind::Blur, 100));
    QVERIFY(pipeline.droppedFrameCount() > droppedBefore);
    QVERIFY(pipeline.droppedFrameCount() - droppedBefore < 12);

    // Clearing while a slow request is active invalidates that result. The
    // next source/effect must be the only image allowed back to the UI.
    QtPreviewEffectPipeline staleResultPipeline;
    QSignalSpy currentResultSpy(
        &staleResultPipeline, &QtPreviewEffectPipeline::frameProcessed);
    QImage slowFrame(1024, 1024, QImage::Format_ARGB32);
    slowFrame.fill(QColor(10, 20, 30));
    QVERIFY(staleResultPipeline.submit(slowFrame, ClipEffectKind::Blur, 100));
    staleResultPipeline.clear();
    QVERIFY(staleResultPipeline.submit(source, ClipEffectKind::Invert, 100));
    QVERIFY(currentResultSpy.wait(5000));
    QCOMPARE(currentResultSpy.count(), 1);
    const QImage currentResult = currentResultSpy.takeFirst().first().value<QImage>();
    QCOMPARE(currentResult.size(), source.size());
    QCOMPARE(qRed(currentResult.pixel(0, 0)), 55);
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

    QSlider *positionSlider = panel.findChild<QSlider *>(
        QStringLiteral("positionSlider"));
    panel.resize(700, 50);
    panel.show();
    QCoreApplication::processEvents();
    QTest::mouseClick(positionSlider, Qt::LeftButton, Qt::NoModifier,
                      QPoint(positionSlider->width() * 3 / 4,
                             positionSlider->height() / 2));
    QCOMPARE(positionSpy.count(), 1);
    QVERIFY(positionSpy.takeFirst()[0].toInt() > 60);

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
    QImage fallback(160, 90, QImage::Format_RGB32);
    fallback.fill(QColor(210, 45, 30));
    panel.setFallbackImage(fallback);
    panel.setPreviewState(state);

    // The decoder targets our QVideoSink. QtPreviewPanel paints the received
    // frame itself, which is what allows timeline opacity and placement.
    QVERIFY(panel.videoSink() != nullptr);

    // Selecting another library video clears the old decoded frame. Its own
    // thumbnail must remain visible while the new decoder source is loading,
    // instead of exposing the generated blue placeholder card.
    panel.setDecodedVideoVisible(false);
    panel.show();
    QCoreApplication::processEvents();
    const QImage rendered = panel.grab().toImage();
    // This test deliberately places a 50% preview in the bottom-right, so
    // sample inside that video rectangle rather than at the whole-panel centre.
    const QColor fallbackPixel = rendered.pixelColor(500, 300);
    QVERIFY(fallbackPixel.red() > fallbackPixel.blue());
    QVERIFY(fallbackPixel.red() > fallbackPixel.green());
}

void MiniEditorQtWidgetTests::realStillImagePlaybackDoesNotPaintAudioTimeOverlay()
{
    QtPreviewPanel panel;
    panel.resize(640, 400);

    QImage stillImage(640, 360, QImage::Format_RGB32);
    stillImage.fill(QColor(210, 45, 30));
    panel.setFallbackImage(stillImage);

    PreviewState preview;
    preview.mode = PreviewMode::Timeline;
    preview.hasMedia = true;
    preview.mediaKind = MediaKind::Image;
    preview.hasAudio = true;
    preview.audioSourceFrame = 90;
    preview.audioSourceDurationFrames = 300;
    panel.setPreviewState(preview);

    PlaybackState playback;
    playback.isPlaying = true;
    playback.currentFrame = 90;
    panel.setPlaybackState(playback);

    panel.show();
    QCoreApplication::processEvents();
    const QColor centerPixel = panel.grab().toImage().pixelColor(
        panel.width() / 2, panel.height() / 2);
    QVERIFY(centerPixel.red() > 180);
    QVERIFY(centerPixel.green() < 80);
    QVERIFY(centerPixel.blue() < 80);
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

void MiniEditorQtWidgetTests::timelineVideoAudioMuteSeparatesRefreshAndUserToggle()
{
    QtTimelineCanvas canvas;
    canvas.resize(1000, TimelineGeometry::kCanvasHeight);
    int muteEditCount = 0;
    bool requestedMute = false;
    canvas.setVideoTrackAudioMutedHandler([&](bool isMuted) {
        ++muteEditCount;
        requestedMute = isMuted;
    });

    TimelinePresentationState state;
    state.audioMix.isVideoTrackMuted = false;
    canvas.setPresentationState(state);
    QCOMPARE(muteEditCount, 0);

    QToolButton *audioButton = canvas.findChild<QToolButton *>(
        QStringLiteral("videoTrackAudioButton"));
    QVERIFY(audioButton != nullptr);
    QVERIFY(audioButton->isChecked());
    QVERIFY(!audioButton->icon().isNull());

    QTest::mouseClick(audioButton, Qt::LeftButton);
    QCOMPARE(muteEditCount, 1);
    QVERIFY(requestedMute);

    state.audioMix.isVideoTrackMuted = true;
    canvas.setPresentationState(state);
    QCOMPARE(muteEditCount, 1);
    QVERIFY(!audioButton->isChecked());
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
