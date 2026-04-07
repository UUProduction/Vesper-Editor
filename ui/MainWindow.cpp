// ============================================================
//  MainWindow.cpp
// ============================================================
#include "MainWindow.h"
#include "ToolbarWidget.h"
#include "PreviewPlayer.h"
#include "TimelineWidget.h"
#include "MediaBrowser.h"
#include "PropertiesPanel.h"
#include "TransitionPanel.h"
#include "TextEditor.h"
#include "ExportDialog.h"
#include "assets/VesperTheme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QSvgWidget>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QElapsedTimer>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_timeline(std::make_unique<Timeline>(this))
{
    setWindowTitle("Vesper Editor");
    setMinimumSize(800, 600);

    buildUI();
    connectSignals();

    // Initialize with 2 tracks
    m_timeline->addVideoTrack("V1");
    m_timeline->addAudioTrack("A1");

    // Playback timer at ~60fps
    m_playTimer = new QTimer(this);
    m_playTimer->setInterval(16);
    connect(m_playTimer, &QTimer::timeout, this, &MainWindow::onPlaybackTick);

    // Init audio mixer
    AudioMixer::instance().initialize(m_timeline.get());

    // Init project manager
    ProjectManager::instance().setTimeline(m_timeline.get());
    ProjectManager::instance().newProject();

    updateWindowTitle();

    // Apply stylesheet
    setStyleSheet(QString(
        "QMainWindow, QWidget { background: %1; color: %2; }"
        "QSplitter::handle { background: %3; }"
        "QScrollBar:vertical { background: %4; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: %5; border-radius: 4px; }"
        "QScrollBar:horizontal { background: %4; height: 8px; border-radius: 4px; }"
        "QScrollBar::handle:horizontal { background: %5; border-radius: 4px; }"
        "QMenuBar { background: %1; color: %2; }"
        "QMenuBar::item:selected { background: %6; }"
        "QMenu { background: %4; color: %2; border: 1px solid %3; }"
        "QMenu::item:selected { background: %6; }"
    ).arg(VesperTheme::Background.name())
     .arg(VesperTheme::TextPrimary.name())
     .arg(VesperTheme::SurfaceBorder.name())
     .arg(VesperTheme::Surface.name())
     .arg(VesperTheme::SurfaceRaised.name())
     .arg(VesperTheme::Accent.name()));
}

MainWindow::~MainWindow()
{
    AudioMixer::instance().shutdown();
}

// ── Build UI ──────────────────────────────────────────────────
void MainWindow::buildUI()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    buildTopBar();
    root->addWidget(m_topBar);

    buildCenterArea();
    root->addWidget(m_centerArea, 1);

    buildPlaybackBar();
    root->addWidget(m_playbackBar);

    buildEditToolbar();
    root->addWidget(m_editToolbar);

    buildTimeline();

    // Main vertical splitter: preview / timeline / media browser
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    m_mainSplitter->addWidget(m_centerArea);
    m_mainSplitter->addWidget(m_timelineWgt);
    m_mainSplitter->addWidget(m_mediaBrowser);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 2);
    m_mainSplitter->setStretchFactor(2, 1);

    // Replace simple layout with splitter
    root->removeWidget(m_centerArea);
    root->addWidget(m_mainSplitter, 1);
    root->removeWidget(m_editToolbar);
    // Re-insert edit toolbar above timeline
    m_mainSplitter->insertWidget(1, m_editToolbar);
}

void MainWindow::buildTopBar()
{
    m_topBar = new QWidget(this);
    m_topBar->setFixedHeight(VesperTheme::TopBarHeight);
    m_topBar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;")
        .arg(VesperTheme::Surface.name())
        .arg(VesperTheme::SurfaceBorder.name()));

    auto* lay = new QHBoxLayout(m_topBar);
    lay->setContentsMargins(12, 0, 12, 0);
    lay->setSpacing(8);

    // Logo / Title
    auto* logo = new QLabel("⬡ Vesper", m_topBar);
    logo->setStyleSheet(QString(
        "font-size: %1px; font-weight: bold; color: %2; letter-spacing: 1px;")
        .arg(VesperTheme::FontSizeLG)
        .arg(VesperTheme::Accent.name()));

    // Undo / Redo
    m_btnUndo = new QPushButton("↩", m_topBar);
    m_btnRedo = new QPushButton("↪", m_topBar);
    m_btnUndo->setToolTip("Undo");
    m_btnRedo->setToolTip("Redo");
    m_btnUndo->setStyleSheet(VesperTheme::iconButtonStyle());
    m_btnRedo->setStyleSheet(VesperTheme::iconButtonStyle());
    m_btnUndo->setFixedSize(32, 32);
    m_btnRedo->setFixedSize(32, 32);

    lay->addWidget(logo);
    lay->addStretch();
    lay->addWidget(m_btnUndo);
    lay->addWidget(m_btnRedo);

    // Export button
    auto* btnExport = new QPushButton("Export ▶", m_topBar);
    btnExport->setStyleSheet(VesperTheme::accentButtonStyle());
    btnExport->setFixedHeight(32);
    lay->addWidget(btnExport);
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::onExport);
}

void MainWindow::buildPlaybackBar()
{
    m_playbackBar = new QWidget(this);
    m_playbackBar->setFixedHeight(VesperTheme::PlaybackBarHeight);
    m_playbackBar->setStyleSheet(
        QString("background: %1; border-top: 1px solid %2;")
        .arg(VesperTheme::Surface.name())
        .arg(VesperTheme::SurfaceBorder.name()));

    auto* lay = new QHBoxLayout(m_playbackBar);
    lay->setContentsMargins(12, 0, 12, 0);
    lay->setSpacing(4);

    auto makeBtn = [&](const QString& icon, const QString& tip) {
        auto* b = new QPushButton(icon, m_playbackBar);
        b->setToolTip(tip);
        b->setStyleSheet(VesperTheme::iconButtonStyle());
        b->setFixedSize(36, 36);
        return b;
    };

    auto* btnStart    = makeBtn("⏮", "Go to start");
    auto* btnBack     = makeBtn("◀◀", "Step back");
    m_btnPlay         = makeBtn("▶", "Play / Pause");
    auto* btnFwd      = makeBtn("▶▶", "Step forward");
    auto* btnEnd      = makeBtn("⏭", "Go to end");

    m_timecodeLabel = new QLabel("00:00:00:00", m_playbackBar);
    m_timecodeLabel->setStyleSheet(
        QString("font-family: monospace; font-size: %1px; color: %2; min-width: 90px;")
        .arg(VesperTheme::FontSizeMD)
        .arg(VesperTheme::TextPrimary.name()));

    lay->addStretch();
    lay->addWidget(btnStart);
    lay->addWidget(btnBack);
    lay->addWidget(m_btnPlay);
    lay->addWidget(btnFwd);
    lay->addWidget(btnEnd);
    lay->addSpacing(12);
    lay->addWidget(m_timecodeLabel);
    lay->addStretch();

    connect(btnStart, &QPushButton::clicked, [this]{ onSeekBackward(); });
    connect(btnBack,  &QPushButton::clicked, [this]{
        m_timeline->setPlayheadPosition(m_timeline->playheadPosition() - 1.0/30.0);
    });
    connect(m_btnPlay, &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(btnFwd,   &QPushButton::clicked, [this]{
        m_timeline->setPlayheadPosition(m_timeline->playheadPosition() + 1.0/30.0);
    });
    connect(btnEnd,   &QPushButton::clicked, [this]{
        m_timeline->setPlayheadPosition(m_timeline->duration());
    });
}

void MainWindow::buildEditToolbar()
{
    m_editToolbar = new QWidget(this);
    m_editToolbar->setFixedHeight(VesperTheme::ToolbarHeight);
    m_editToolbar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;")
        .arg(VesperTheme::SurfaceRaised.name())
        .arg(VesperTheme::SurfaceBorder.name()));

    auto* lay = new QHBoxLayout(m_editToolbar);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(4);

    auto makeToolBtn = [&](const QString& icon, const QString& tip) {
        auto* b = new QPushButton(icon, m_editToolbar);
        b->setToolTip(tip);
        b->setStyleSheet(VesperTheme::iconButtonStyle());
        b->setFixedSize(40, 40);
        return b;
    };

    auto* btnText     = makeToolBtn("T",  "Add Text");
    m_btnSplit        = makeToolBtn("✂",  "Split clip at playhead (S)");
    auto* btnCrop     = makeToolBtn("⊡",  "Crop / Transform");
    auto* btnTrans    = makeToolBtn("⟷",  "Transitions");
    auto* btnSpeed    = makeToolBtn("⚡",  "Speed / Retime");
    auto* btnAudio    = makeToolBtn("♫",  "Audio");
    auto* btnAddTrack = makeToolBtn("+",  "Add Track");
    auto* btnMedia    = makeToolBtn("⊞",  "Media Browser");

    lay->addWidget(btnText);
    lay->addWidget(m_btnSplit);
    lay->addWidget(btnCrop);
    lay->addWidget(btnTrans);
    lay->addWidget(btnSpeed);
    lay->addWidget(btnAudio);
    lay->addWidget(new QWidget(this));  // spacer
    lay->addWidget(btnAddTrack);
    lay->addWidget(btnMedia);
    lay->addStretch();

    connect(btnText,     &QPushButton::clicked, [this]{ onShowTextEditor(""); });
    connect(m_btnSplit,  &QPushButton::clicked, this, &MainWindow::onSplit);
    connect(btnTrans,    &QPushButton::clicked, [this]{ onShowTransitions("",""); });
    connect(btnMedia,    &QPushButton::clicked, this, &MainWindow::onMediaBrowserToggle);
    connect(btnAddTrack, &QPushButton::clicked, [this]{
        m_timeline->addVideoTrack();
    });
}

void MainWindow::buildCenterArea()
{
    m_centerArea = new QWidget(this);
    auto* lay    = new QVBoxLayout(m_centerArea);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_preview = new PreviewPlayer(m_timeline.get(), m_centerArea);
    lay->addWidget(m_preview, 1);
}

void MainWindow::buildTimeline()
{
    m_timelineWgt = new TimelineWidget(m_timeline.get(), this);
}

// ── Connect signals ───────────────────────────────────────────
void MainWindow::connectSignals()
{
    // Timeline ↔ window
    connect(m_timeline.get(), &Timeline::playheadMoved,
            this, &MainWindow::onPlayheadMoved);
    connect(m_timeline.get(), &Timeline::durationChanged,
            this, &MainWindow::onDurationChanged);
    connect(m_timeline.get(), &Timeline::undoStackChanged, [this]{
        m_btnUndo->setEnabled(m_timeline->canUndo());
        m_btnRedo->setEnabled(m_timeline->canRedo());
    });

    // Undo/redo buttons
    connect(m_btnUndo, &QPushButton::clicked, this, &MainWindow::onUndo);
    connect(m_btnRedo, &QPushButton::clicked, this, &MainWindow::onRedo);

    // Project
    connect(&ProjectManager::instance(), &ProjectManager::dirtyChanged,
            this, &MainWindow::updateWindowTitle);
}

// ── Playback ──────────────────────────────────────────────────
void MainWindow::onPlay()
{
    if (m_isPlaying) {
        onPause();
        return;
    }
    m_isPlaying = true;
    m_btnPlay->setText("⏸");
    AudioMixer::instance().play(m_timeline->playheadPosition());
    m_playTimer->start();
    emit m_timeline->playheadMoved(m_timeline->playheadPosition());
}

void MainWindow::onPause()
{
    m_isPlaying = false;
    m_btnPlay->setText("▶");
    AudioMixer::instance().pause();
    m_playTimer->stop();
}

void MainWindow::onStop()
{
    onPause();
    m_timeline->setPlayheadPosition(0.0);
}

void MainWindow::onSeekForward()
{
    m_timeline->setPlayheadPosition(
        m_timeline->playheadPosition() + 5.0);
}

void MainWindow::onSeekBackward()
{
    m_timeline->setPlayheadPosition(0.0);
}

void MainWindow::onPlaybackTick()
{
    if (!m_isPlaying) return;
    double t = AudioMixer::instance().currentTime();
    m_timeline->setPlayheadPosition(t);
    if (t >= m_timeline->duration()) {
        onPause();
        m_timeline->setPlayheadPosition(0.0);
    }
}

void MainWindow::onPlayheadMoved(double seconds)
{
    m_timecodeLabel->setText(formatTimecode(seconds));
    if (m_preview) m_preview->seekTo(seconds);
}

void MainWindow::onDurationChanged(double) {}

// ── Edit actions ─────────────────────────────────────────────
void MainWindow::onSplit()
{
    double t = m_timeline->playheadPosition();
    for (auto& trk : m_timeline->tracks()) {
        auto clip = trk->clipAt(t);
        if (clip) m_timeline->splitClip(trk->id, clip->id, t);
    }
}

void MainWindow::onUndo() { m_timeline->undo(); }
void MainWindow::onRedo() { m_timeline->redo(); }

void MainWindow::onDeleteSelected()
{
    auto sel = m_timeline->selectedClips();
    for (auto& [trkId, clipId] : sel)
        m_timeline->removeClip(trkId, clipId);
}

// ── Panel slots ──────────────────────────────────────────────
void MainWindow::onShowProperties(const QString& trkId, const QString& clipId)
{
    if (!m_properties) {
        m_properties = new PropertiesPanel(m_timeline.get(), this);
        m_properties->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    }
    m_properties->loadClip(trkId, clipId);
    m_properties->show();
    m_properties->raise();
}

void MainWindow::onShowTransitions(const QString& trkId, const QString& clipId)
{
    if (!m_transitions) {
        m_transitions = new TransitionPanel(m_timeline.get(), this);
        m_transitions->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    }
    if (!clipId.isEmpty()) m_transitions->loadClip(trkId, clipId);
    m_transitions->show();
    m_transitions->raise();
}

void MainWindow::onShowTextEditor(const QString& clipId)
{
    if (!m_textEditor) {
        m_textEditor = new TextEditor(m_timeline.get(), this);
        m_textEditor->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    }
    if (!clipId.isEmpty()) m_textEditor->loadClip(clipId);
    m_textEditor->show();
    m_textEditor->raise();
}

void MainWindow::onMediaBrowserToggle()
{
    if (m_mediaBrowser) {
        m_mediaBrowser->setVisible(!m_mediaBrowser->isVisible());
    }
}

void MainWindow::onClipSelected(const QString& trkId, const QString& clipId)
{
    m_timeline->selectClip(trkId, clipId);
}

// ── Project ──────────────────────────────────────────────────
void MainWindow::onNewProject()
{
    if (ProjectManager::instance().isDirty()) {
        int r = QMessageBox::question(this, "New Project",
            "You have unsaved changes. Continue?",
            QMessageBox::Yes | QMessageBox::No);
        if (r != QMessageBox::Yes) return;
    }
    ProjectManager::instance().newProject();
}

void MainWindow::onOpenProject()
{
    QString path = QFileDialog::getOpenFileName(this,
        "Open Project", {}, "Vesper Project (*.vsp);;All Files (*)");
    if (!path.isEmpty())
        ProjectManager::instance().openProject(path);
}

void MainWindow::onSaveProject()
{
    if (ProjectManager::instance().meta().filePath.isEmpty())
        onSaveProjectAs();
    else
        ProjectManager::instance().saveProject();
}

void MainWindow::onSaveProjectAs()
{
    QString path = QFileDialog::getSaveFileName(this,
        "Save Project As", {}, "Vesper Project (*.vsp)");
    if (!path.isEmpty())
        ProjectManager::instance().saveProjectAs(path);
}

void MainWindow::onExport()
{
    if (!m_exportDialog)
        m_exportDialog = new ExportDialog(m_timeline.get(), this);
    m_exportDialog->exec();
}

// ── Helpers ───────────────────────────────────────────────────
void MainWindow::updateWindowTitle()
{
    auto& pm = ProjectManager::instance();
    QString title = "Vesper Editor — " + pm.meta().name;
    if (pm.isDirty()) title += " *";
    setWindowTitle(title);
}

QString MainWindow::formatTimecode(double seconds) const
{
    int fps = 30;
    int  s  = static_cast<int>(seconds);
    int  f  = static_cast<int>((seconds - s) * fps);
    int  m  = s / 60; s %= 60;
    int  h  = m / 60; m %= 60;
    return QString("%1:%2:%3:%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(f, 2, 10, QChar('0'));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (ProjectManager::instance().isDirty()) {
        int r = QMessageBox::question(this, "Quit",
            "Save project before quitting?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (r == QMessageBox::Cancel) { event->ignore(); return; }
        if (r == QMessageBox::Save)   onSaveProject();
    }
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Space: onPlay();  break;
        case Qt::Key_S:     onSplit(); break;
        case Qt::Key_Delete:
        case Qt::Key_Backspace: onDeleteSelected(); break;
        case Qt::Key_Z:
            if (event->modifiers() & Qt::ControlModifier) {
                if (event->modifiers() & Qt::ShiftModifier) onRedo();
                else onUndo();
            }
            break;
        default: QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);
}
