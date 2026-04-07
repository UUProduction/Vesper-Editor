// ============================================================
//  PreviewPlayer.cpp
// ============================================================
#include "PreviewPlayer.h"
#include "assets/VesperTheme.h"
#include <QVBoxLayout>
#include <QPainter>
#include <QResizeEvent>

PreviewPlayer::PreviewPlayer(Timeline* timeline, QWidget* parent)
    : QWidget(parent), m_timeline(timeline)
{
    setMinimumHeight(200);
    setStyleSheet(QString("background: %1;").arg(VesperTheme::Background.name()));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    m_renderer = new Renderer(this);
    m_renderer->setTimeline(timeline);
    lay->addWidget(m_renderer, 1);

    // Throttle frame decoding to avoid spamming decoders on seek
    m_decodeThrottle = new QTimer(this);
    m_decodeThrottle->setSingleShot(true);
    m_decodeThrottle->setInterval(30);   // ms
    connect(m_decodeThrottle, &QTimer::timeout, [this]{
        decodeAndUploadFrame(m_pendingTime);
    });
}

void PreviewPlayer::setTimeline(Timeline* tl)
{
    m_timeline = tl;
    m_renderer->setTimeline(tl);
}

void PreviewPlayer::seekTo(double seconds)
{
    m_currentTime  = seconds;
    m_pendingTime  = seconds;
    m_renderer->setCurrentTime(seconds);
    m_decodeThrottle->start();
}

void PreviewPlayer::decodeAndUploadFrame(double seconds)
{
    if (!m_timeline) return;

    for (auto& trk : m_timeline->tracks()) {
        if (trk->type != Track::TrackType::Video || !trk->isVisible) continue;
        auto clip = trk->clipAt(seconds);
        if (!clip) continue;

        // Skip if thumbnail fresh enough
        if (!clip->thumbnailDirty && m_decoders.contains(clip->id)) continue;

        // Create/reuse decoder
        if (!m_decoders.contains(clip->id)) {
            auto dec = MediaEngine::instance().createDecoder(clip->filePath);
            if (!dec->open()) continue;
            m_decoders[clip->id] = std::move(dec);
        }

        auto& dec    = m_decoders[clip->id];
        double srcT  = clip->sourceIn + (seconds - clip->timelineStart);
        dec->seekTo(srcT);
        VideoFrame vf = dec->nextVideoFrame();
        if (vf.isValid) {
            m_renderer->updateClipFrame(clip->id, vf.image);
            clip->thumbnailDirty = false;
        }
    }
}

void PreviewPlayer::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
}

void PreviewPlayer::paintEvent(QPaintEvent* e)
{
    QWidget::paintEvent(e);
}
