// ============================================================
//  TimelineWidget.cpp
// ============================================================
#include "TimelineWidget.h"
#include <QPainter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <cmath>

// ── TimelineWidget ────────────────────────────────────────────
TimelineWidget::TimelineWidget(Timeline* timeline, QWidget* parent)
    : QWidget(parent), m_timeline(timeline)
{
    setMinimumHeight(150);
    buildUI();

    connect(timeline, &Timeline::trackAdded,    this, &TimelineWidget::onTrackAdded);
    connect(timeline, &Timeline::trackRemoved,  this, &TimelineWidget::onTrackRemoved);
    connect(timeline, &Timeline::clipAdded,     this, &TimelineWidget::onClipAdded);
    connect(timeline, &Timeline::clipMoved,     this, &TimelineWidget::onClipMoved);
    connect(timeline, &Timeline::playheadMoved, this, &TimelineWidget::onPlayheadMoved);
    connect(timeline, &Timeline::durationChanged,this,&TimelineWidget::onDurationChanged);
}

void TimelineWidget::buildUI()
{
    setStyleSheet(QString("background: %1;").arg(VesperTheme::Surface.name()));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Ruler
    m_ruler = new TimelineRuler();  // defined below inline
    m_ruler->setFixedHeight(24);

    // Middle row: headers + content
    auto* midRow = new QHBoxLayout;
    midRow->setContentsMargins(0, 0, 0, 0);
    midRow->setSpacing(0);

    m_headers = new TrackHeaderArea(m_timeline, this);
    m_headers->setFixedWidth(VesperTheme::TrackHeaderWidth);

    m_content = new TrackContentArea(m_timeline, this, this);

    midRow->addWidget(m_headers);
    midRow->addWidget(m_content, 1);

    m_hScroll = new QScrollBar(Qt::Horizontal, this);
    m_vScroll = new QScrollBar(Qt::Vertical, this);

    // Assemble
    auto* midWidget = new QWidget(this);
    midWidget->setLayout(midRow);

    auto* mainRow = new QHBoxLayout;
    mainRow->setContentsMargins(0, 0, 0, 0);
    mainRow->setSpacing(0);
    mainRow->addWidget(midWidget, 1);
    mainRow->addWidget(m_vScroll);

    auto* mainWidget = new QWidget(this);
    mainWidget->setLayout(mainRow);

    rootLayout->addWidget(m_ruler);
    rootLayout->addWidget(mainWidget, 1);
    rootLayout->addWidget(m_hScroll);

    connect(m_hScroll, &QScrollBar::valueChanged, [this](int v){
        m_scrollX = v;
        m_content->update();
    });

    connect(m_content, &TrackContentArea::clipClicked,
            this,      &TimelineWidget::clipSelected);
    connect(m_content, &TrackContentArea::clipDoubleClicked,
            this,      &TimelineWidget::clipDoubleClicked);
}

double TimelineWidget::timeAtX(int x) const
{
    return (x + m_scrollX) / m_pixPerSec;
}

int TimelineWidget::xAtTime(double t) const
{
    return static_cast<int>(t * m_pixPerSec - m_scrollX);
}

void TimelineWidget::setPixelsPerSecond(double pps)
{
    m_pixPerSec = qBound(10.0, pps, 2000.0);
    syncScrollbars();
    m_content->update();
}

void TimelineWidget::syncScrollbars()
{
    double totalWidth = m_timeline->duration() * m_pixPerSec;
    m_hScroll->setMaximum(qMax(0, static_cast<int>(totalWidth - m_content->width())));
    m_hScroll->setPageStep(m_content->width());
}

void TimelineWidget::wheelEvent(QWheelEvent* e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        double factor = e->angleDelta().y() > 0 ? 1.2 : 0.8;
        setPixelsPerSecond(m_pixPerSec * factor);
    } else {
        m_hScroll->setValue(m_hScroll->value() - e->angleDelta().y());
    }
    e->accept();
}

void TimelineWidget::keyPressEvent(QKeyEvent* e)
{
    QWidget::keyPressEvent(e);
}

void TimelineWidget::onTrackAdded   (const QString&) { m_headers->refresh(); m_content->refresh(); }
void TimelineWidget::onTrackRemoved (const QString&) { m_headers->refresh(); m_content->refresh(); }
void TimelineWidget::onClipAdded    (const QString&, const QString&) { m_content->refresh(); }
void TimelineWidget::onClipMoved    (const QString&, const QString&) { m_content->refresh(); }
void TimelineWidget::onPlayheadMoved(double) { m_content->update(); }
void TimelineWidget::onDurationChanged(double) { syncScrollbars(); m_content->update(); }

// ── TimelineRuler (simple inline widget) ─────────────────────
class TimelineRuler : public QWidget
{
public:
    explicit TimelineRuler(QWidget* p = nullptr) : QWidget(p) {}
    TimelineWidget* m_tw = nullptr;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), VesperTheme::SurfaceRaised);
        p.setPen(VesperTheme::TextSecondary);
        // Simple ruler
        if (!m_tw) return;
        int w = width();
        double pps = m_tw->pixelsPerSecond();
        double scroll = m_tw->xAtTime(0) * -1;
        double startT = m_tw->timeAtX(0);
        double step   = 1.0;
        if (pps < 20) step = 10;
        else if (pps < 50) step = 5;
        else if (pps > 500) step = 0.1;
        else if (pps > 200) step = 0.5;

        for (double t = std::floor(startT / step) * step;
             m_tw->xAtTime(t) < w; t += step)
        {
            int x = m_tw->xAtTime(t);
            if (x < 0) continue;
            int s = static_cast<int>(t);
            int m = s / 60; s %= 60;
            QString lbl = QString("%1:%2").arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
            p.drawLine(x, height() - 8, x, height());
            p.drawText(x + 2, height() - 10, lbl);
        }
    }
};

// ── TrackHeaderArea ───────────────────────────────────────────
TrackHeaderArea::TrackHeaderArea(Timeline* tl, QWidget* parent)
    : QWidget(parent), m_timeline(tl)
{
    setStyleSheet(QString("background: %1;").arg(VesperTheme::SurfaceRaised.name()));
}

void TrackHeaderArea::refresh() { update(); }

void TrackHeaderArea::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), VesperTheme::SurfaceRaised);

    int y = 0;
    for (auto& trk : m_timeline->tracks()) {
        QRect r(0, y, width(), trk->height);
        bool isVideo = trk->type == Track::TrackType::Video;
        QColor trackColor = isVideo ? VesperTheme::TrackVideo : VesperTheme::TrackAudio;

        p.fillRect(r.adjusted(0, 1, 0, -1), trackColor.darker(150));
        p.setPen(VesperTheme::TextPrimary);
        p.setFont(QFont(VesperTheme::FontFamily, VesperTheme::FontSizeXS,
                        QFont::Bold));
        p.drawText(r.adjusted(8, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   trk->name);

        // Mute indicator
        if (trk->isMuted) {
            p.setPen(VesperTheme::Danger);
            p.drawText(r.adjusted(0, 0, -4, 0), Qt::AlignVCenter | Qt::AlignRight, "M");
        }

        // Border
        p.setPen(VesperTheme::SurfaceBorder);
        p.drawLine(0, y + trk->height - 1, width(), y + trk->height - 1);
        y += trk->height;
    }
}

// ── TrackContentArea ──────────────────────────────────────────
TrackContentArea::TrackContentArea(Timeline* tl, TimelineWidget* tw, QWidget* parent)
    : QWidget(parent), m_timeline(tl), m_tw(tw)
{
    setMouseTracking(true);
    setStyleSheet(QString("background: %1;").arg(VesperTheme::Surface.name()));
}

void TrackContentArea::refresh() { update(); }

void TrackContentArea::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), VesperTheme::Surface);

    // Grid lines
    {
        p.setPen(QPen(VesperTheme::SurfaceBorder, 1));
        double pps  = m_tw->pixelsPerSecond();
        double step = 1.0;
        if (pps < 20) step = 10;
        double startT = m_tw->timeAtX(0);
        for (double t = std::floor(startT / step) * step;
             m_tw->xAtTime(t) < width(); t += step)
        {
            int x = m_tw->xAtTime(t);
            if (x >= 0) p.drawLine(x, 0, x, height());
        }
    }

    // Tracks
    int y = 0;
    const auto& tracks = m_timeline->tracks();
    for (int ti = 0; ti < tracks.size(); ++ti) {
        auto& trk = tracks[ti];
        bool isVideo = trk->type == Track::TrackType::Video;

        // Track background
        QRect bgRect(0, y, width(), trk->height);
        QColor bg = isVideo ? VesperTheme::TrackVideo.darker(180)
                            : VesperTheme::TrackAudio.darker(180);
        p.fillRect(bgRect, bg);

        // Clips
        for (auto& clip : trk->clips) {
            QRect cr = clipRect(trk.get(), y, clip.get());
            if (cr.right() < 0 || cr.left() > width()) { continue; }
            drawClip(p, clip.get(), cr, isVideo, clip->isSelected);
        }

        // Track divider
        p.setPen(VesperTheme::SurfaceBorder);
        p.drawLine(0, y + trk->height - 1, width(), y + trk->height - 1);
        y += trk->height;
    }

    // Snap lines
    if (m_showSnap) drawSnapLines(p);

    // Playhead
    drawPlayhead(p);
}

QRect TrackContentArea::clipRect(const Track* trk, int trackY, const Clip* clip) const
{
    int x1 = m_tw->xAtTime(clip->timelineStart);
    int x2 = m_tw->xAtTime(clip->timelineEnd);
    return QRect(x1, trackY + 2, x2 - x1, trk->height - 4);
}

void TrackContentArea::drawClip(QPainter& p, Clip* clip, const QRect& rect,
                                bool isVideo, bool isSelected)
{
    QColor base  = isVideo ? VesperTheme::TrackVideoClip : VesperTheme::TrackAudioClip;
    if (isSelected) base = VesperTheme::Accent;

    // Background
    p.setBrush(base);
    p.setPen(isSelected ? VesperTheme::AccentGold : base.lighter(130));
    p.drawRoundedRect(rect, VesperTheme::BorderRadiusSmall,
                      VesperTheme::BorderRadiusSmall);

    // Thumbnail/waveform
    if (isVideo)  drawThumbnails(p, clip, rect);
    else          drawWaveform  (p, clip, rect);

    // Name label
    p.setPen(VesperTheme::TextPrimary);
    p.setFont(QFont(VesperTheme::FontFamily, VesperTheme::FontSizeXS));
    p.drawText(rect.adjusted(4, 2, -4, 0),
               Qt::AlignTop | Qt::AlignLeft,
               clip->name);

    // Trim handles
    p.setPen(QPen(Qt::white, 2));
    int handleW = 4;
    p.drawRect(rect.left(), rect.top(), handleW, rect.height());
    p.drawRect(rect.right() - handleW, rect.top(), handleW, rect.height());
}

void TrackContentArea::drawThumbnails(QPainter& p, Clip* clip, const QRect& rect)
{
    if (clip->thumbnail.isNull()) return;
    int thumbW = clip->thumbnail.width();
    int x = rect.left();
    while (x < rect.right()) {
        int w = qMin(thumbW, rect.right() - x);
        p.drawImage(QRect(x, rect.top(), w, rect.height()),
                    clip->thumbnail, QRect(0, 0, w, clip->thumbnail.height()));
        x += thumbW;
    }
}

void TrackContentArea::drawWaveform(QPainter& p, Clip* clip, const QRect& rect)
{
    if (clip->waveformData.isEmpty()) return;
    p.setPen(QPen(VesperTheme::AccentGold.lighter(), 1));
    int n    = clip->waveformData.size();
    int mid  = rect.top() + rect.height() / 2;
    int half = rect.height() / 2 - 2;
    for (int i = 0; i < n; ++i) {
        int x = rect.left() + i * rect.width() / n;
        int h = static_cast<int>(clip->waveformData[i] * half);
        p.drawLine(x, mid - h, x, mid + h);
    }
}

void TrackContentArea::drawPlayhead(QPainter& p)
{
    int x = m_tw->xAtTime(m_timeline->playheadPosition());
    if (x < 0 || x > width()) return;
    p.setPen(QPen(VesperTheme::PlayheadColor, 2));
    p.drawLine(x, 0, x, height());
    // Triangle
    QPolygon tri;
    tri << QPoint(x - 6, 0) << QPoint(x + 6, 0) << QPoint(x, 10);
    p.setBrush(VesperTheme::PlayheadColor);
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);
}

void TrackContentArea::drawSnapLines(QPainter& p)
{
    p.setPen(QPen(VesperTheme::SnapLineColor, 1, Qt::DashLine));
    int x = m_tw->xAtTime(m_snapTime);
    p.drawLine(x, 0, x, height());
}

std::pair<QString,QString> TrackContentArea::clipAt(const QPoint& pos) const
{
    int y = 0;
    for (auto& trk : m_timeline->tracks()) {
        for (auto& clip : trk->clips) {
            QRect r = clipRect(trk.get(), y, clip.get());
            if (r.contains(pos))
                return { trk->id, clip->id };
        }
        y += trk->height;
    }
    return {};
}

void TrackContentArea::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        auto [trkId, clipId] = clipAt(e->pos());
        if (!trkId.isEmpty()) {
            m_dragMode     = DragMode::MovingClip;
            m_dragTrackId  = trkId;
            m_dragClipId   = clipId;
            m_dragStart    = e->pos();
            auto* trk  = m_timeline->trackById(trkId);
            auto  clip = trk->clipById(clipId);
            m_dragOrigStart = clip->timelineStart;
            emit clipClicked(trkId, clipId);
        } else {
            // Click on empty area = move playhead
            m_dragMode = DragMode::Playhead;
            double t = m_tw->timeAtX(e->pos().x());
            m_timeline->setPlayheadPosition(t);
        }
    }
    QWidget::mousePressEvent(e);
}

void TrackContentArea::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragMode == DragMode::Playhead) {
        double t = m_tw->timeAtX(e->pos().x());
        m_timeline->setPlayheadPosition(qMax(0.0, t));
    } else if (m_dragMode == DragMode::MovingClip) {
        int dx  = e->pos().x() - m_dragStart.x();
        double dt = dx / m_tw->pixelsPerSecond();
        double newStart = qMax(0.0, m_dragOrigStart + dt);
        m_timeline->moveClip(m_dragTrackId, m_dragClipId,
                             m_dragTrackId, newStart);
        // Check snapping
        m_showSnap = false;
        for (auto& t : m_timeline->tracks()) {
            for (auto& c : t->clips) {
                if (c->id == m_dragClipId) continue;
                double diff = qAbs(newStart - c->timelineEnd);
                if (diff * m_tw->pixelsPerSecond() < m_snapThreshold) {
                    m_snapTime = c->timelineEnd;
                    m_showSnap = true;
                }
            }
        }
    }
    update();
    QWidget::mouseMoveEvent(e);
}

void TrackContentArea::mouseReleaseEvent(QMouseEvent* e)
{
    m_dragMode = DragMode::None;
    m_showSnap = false;
    update();
    QWidget::mouseReleaseEvent(e);
}

void TrackContentArea::mouseDoubleClickEvent(QMouseEvent* e)
{
    auto [trkId, clipId] = clipAt(e->pos());
    if (!trkId.isEmpty())
        emit clipDoubleClicked(trkId, clipId);
    QWidget::mouseDoubleClickEvent(e);
}
