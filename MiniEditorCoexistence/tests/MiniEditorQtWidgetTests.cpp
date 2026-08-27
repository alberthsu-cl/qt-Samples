#include "MediaLibrary.h"
#include "QtMediaLibraryPanel.h"
#include "QtPropertiesPanel.h"
#include "QtTransportPanel.h"

#include <QComboBox>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QtTest>

class MiniEditorQtWidgetTests final : public QObject
{
    Q_OBJECT

private slots:
    void propertiesModelRefreshDoesNotEmitUserEdit();
    void propertiesUserEditEmitsCompleteSettings();
    void transportRefreshAndButtonsUseSemanticCommands();
    void mediaLibrarySeparatesProgrammaticAndUserSelection();
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

QTEST_MAIN(MiniEditorQtWidgetTests)

#include "MiniEditorQtWidgetTests.moc"
