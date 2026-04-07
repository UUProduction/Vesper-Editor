// ============================================================
//  MediaBrowser.cpp
// ============================================================
#include "MediaBrowser.h"
#include "assets/VesperTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QPainter>
#include <QListWidgetItem>

MediaBrowser::MediaBrowser(Timeline* timeline, QWidget* parent)
    : QWidget(parent), m_timeline(timeline)
{
    setAcceptDrops(true);
    setMinimumHeight(100);
    setMaximumHeight(VesperTheme::MediaBrowserHeight);

    setStyleSheet(QString(
        "background: %1; border-top: 1px solid %2;")
        .arg(VesperTheme::SurfaceRaised.name())
        .arg(VesperTheme::SurfaceBorder.name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 6);
    root->setSpacing(6);

    // Header
    auto* header = new QHBoxLayout;
    auto* title  = new QLabel("Media", this);
    title->setStyleSheet(
        QString("font-size: %1px; font-weight: bold; color: %2;")
        .arg(VesperTheme::FontSizeSM)
        .arg(VesperTheme::TextSecondary.name()));

    m_btnImport = new QPushButton("+ Import", this);
    m_btnImport->setStyleSheet(VesperTheme::accentButtonStyle());
    m_btnImport->setFixedHeight(28);

    m_btnAdd = new QPushButton("Add to Timeline", this);
    m_btnAdd->setStyleSheet(VesperTheme::iconButtonStyle());
    m_btnAdd->setFixedHeight(28);

    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_btnAdd);
    header->addWidget(m_btnImport);

    // List
    m_list = new QListWidget(this);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(80, 50));
    m_list->setGridSize(QSize(90, 70));
    m_list->setDragEnabled(true);
    m_list->setStyleSheet(
        QString("QListWidget { background: %1; border: none; }"
                "QListWidget::item:selected { background: %2; border-radius: 4px; }"
                "QListWidget::item:hover { background: %3; border-radius: 4px; }")
        .arg(VesperTheme::Surface.name())
        .arg(VesperTheme::Accent.name() + "66")
        .arg(VesperTheme::SurfaceRaised.name()));

    m_dropHint = new QLabel("Drop media files here or click Import", this);
    m_dropHint->setAlignment(Qt::AlignCenter);
    m_dropHint->setStyleSheet(
        QString("color: %1; font-size: %2px;")
        .arg(VesperTheme::TextSecondary.name())
        .arg(VesperTheme::FontSizeSM));
    m_dropHint->setVisible(true);

    root->addLayout(header);
    root->addWidget(m_list, 1);
    root->addWidget(m_dropHint);

    connect(m_btnImport, &QPushButton::clicked, this, &MediaBrowser::onImportClicked);
    connect(m_btnAdd,    &QPushButton::clicked, this, &MediaBrowser::onAddToTimeline);
    connect(m_list, &QListWidget::itemDoubleClicked,
            this,   &MediaBrowser::onItemDoubleClicked);
}

void MediaBrowser::onImportClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
        "Import Media", {},
        "Media Files (*.mp4 *.mov *.avi *.mkv *.mp3 *.wav *.aac "
                       "*.png *.jpg *.jpeg *.gif *.webm);;"
        "All Files (*)");
    if (!files.isEmpty()) addFiles(files);
}

void MediaBrowser::addFiles(const QStringList& paths)
{
    m_dropHint->setVisible(false);
    for (auto& path : paths) {
        buildItem(path);
    }
}

void MediaBrowser::buildItem(const QString& filePath)
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    // Determine type
    Clip::MediaType type = Clip::MediaType::Video;
    if (QStringList{"mp3","wav","aac","flac","ogg","m4a"}.contains(ext))
        type = Clip::MediaType::Audio;
    else if (QStringList{"png","jpg","jpeg","bmp","gif","tiff","webp"}.contains(ext))
        type = Clip::MediaType::Image;

    // Thumbnail
    QImage thumb;
    if (type == Clip::MediaType::Video) {
        MediaDecoder dec(filePath);
        if (dec.open()) thumb = dec.thumbnailAt(0.5, 80, 50);
    } else if (type == Clip::MediaType::Image) {
        thumb = QImage(filePath).scaled(80, 50,
                    Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (thumb.isNull()) {
        // Placeholder colored by type
        thumb = QImage(80, 50, QImage::Format_RGB32);
        thumb.fill(type == Clip::MediaType::Audio
                   ? VesperTheme::TrackAudio : VesperTheme::TrackVideo);
    }

    auto* item = new QListWidgetItem(QIcon(QPixmap::fromImage(thumb)),
                                     fi.baseName(), m_list);
    item->setToolTip(filePath);
    item->setData(Qt::UserRole, filePath);
    item->setData(Qt::UserRole + 1, static_cast<int>(type));
}

void MediaBrowser::onItemDoubleClicked(QListWidgetItem* item)
{
    onAddToTimeline();
}

void MediaBrowser::onAddToTimeline()
{
    auto sel = m_list->selectedItems();
    if (sel.isEmpty()) return;

    for (auto* item : sel) {
        QString path = item->data(Qt::UserRole).toString();
        int     type = item->data(Qt::UserRole + 1).toInt();
        Clip::MediaType mt = static_cast<Clip::MediaType>(type);

        // Find the first compatible track
        Track* target = nullptr;
        for (auto& t : m_timeline->tracks()) {
            bool wantVideo = (mt != Clip::MediaType::Audio);
            bool isVideo   = (t->type == Track::TrackType::Video);
            if (wantVideo == isVideo && !t->isLocked) { target = t.get(); break; }
        }
        if (!target) {
            if (mt == Clip::MediaType::Audio)
                target = m_timeline->addAudioTrack();
            else
                target = m_timeline->addVideoTrack();
        }

        double pos = m_timeline->playheadPosition();
        auto   clip= m_timeline->addClip(target->id, path, pos, mt);
        if (clip) {
            // Update duration from media engine
            MediaInfo info = MediaEngine::instance().probe(path);
            if (info.duration > 0) {
                clip->sourceDuration = info.duration;
                clip->sourceOut      = info.duration;
                clip->timelineEnd    = pos + info.duration;
            }
        }
    }
}

void MediaBrowser::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MediaBrowser::dropEvent(QDropEvent* e)
{
    QStringList files;
    for (auto& url : e->mimeData()->urls())
        if (url.isLocalFile() &&
            MediaEngine::instance().isSupported(url.toLocalFile()))
            files << url.toLocalFile();
    if (!files.isEmpty()) addFiles(files);
    e->acceptProposedAction();
}
