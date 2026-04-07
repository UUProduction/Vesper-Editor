// ============================================================
//  MainWindow.h — Root layout controller
// ============================================================
#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <memory>

#include "core/Timeline.h"
#include "core/AudioMixer.h"
#include "core/ProjectManager.h"

class ToolbarWidget;
class PreviewPlayer;
class TimelineWidget;
class MediaBrowser;
class PropertiesPanel;
class TransitionPanel;
class TextEditor;
class ExportDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    // Playback
    void onPlay();
    void onPause();
    void onStop();
    void onSeekForward();
    void onSeekBackward();
    void onPlayheadMoved(double seconds);
    void onPlaybackTick();

    // Editing
    void onSplit();
    void onUndo();
    void onRedo();
    void onDeleteSelected();

    // Project
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onExport();

    // UI panels
    void onShowProperties(const QString& trackId, const QString& clipId);
    void onShowTransitions(const QString& trackId, const QString& clipId);
    void onShowTextEditor(const QString& clipId);
    void onMediaBrowserToggle();

    // Timeline signals
    void onClipSelected(const QString& trackId, const QString& clipId);
    void onDurationChanged(double seconds);

private:
    void buildUI();
    void buildTopBar();
    void buildPlaybackBar();
    void buildEditToolbar();
    void buildCenterArea();
    void buildTimeline();
    void buildMediaBrowser();
    void connectSignals();
    void updateWindowTitle();
    void updatePlaybackControls();
    QString formatTimecode(double seconds) const;

    // ── Core objects ─────────────────────────────────────────
    std::unique_ptr<Timeline>        m_timeline;

    // ── Widgets ──────────────────────────────────────────────
    QWidget*          m_topBar       = nullptr;
    QWidget*          m_playbackBar  = nullptr;
    QWidget*          m_editToolbar  = nullptr;
    QWidget*          m_centerArea   = nullptr;
    QSplitter*        m_mainSplitter = nullptr;

    PreviewPlayer*    m_preview      = nullptr;
    TimelineWidget*   m_timelineWgt  = nullptr;
    MediaBrowser*     m_mediaBrowser = nullptr;
    PropertiesPanel*  m_properties   = nullptr;
    TransitionPanel*  m_transitions  = nullptr;
    TextEditor*       m_textEditor   = nullptr;
    ExportDialog*     m_exportDialog = nullptr;

    // ── Playback ─────────────────────────────────────────────
    QTimer*    m_playTimer  = nullptr;
    bool       m_isPlaying  = false;
    double     m_playStart  = 0.0;

    // ── Buttons (kept for enable/disable) ────────────────────
    QPushButton* m_btnUndo   = nullptr;
    QPushButton* m_btnRedo   = nullptr;
    QPushButton* m_btnPlay   = nullptr;
    QPushButton* m_btnSplit  = nullptr;
    QLabel*      m_timecodeLabel = nullptr;
};
