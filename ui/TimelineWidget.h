// ============================================================
//  TimelineWidget.h — Scrollable multi-track timeline
// ============================================================
#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QElapsedTimer>
#include "core/Timeline.h"
#include "assets/VesperTheme.h"

class TimelineRuler;
class TrackHeaderArea;
class TrackContentArea;

class TimelineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineWidget(Timeline* timeline, QWidget* parent = nullptr);
    ~TimelineWidget() override = default;

    // Pixels per second (zoom)
    double pixelsPerSecond() const { return m_pixPerSec; }
    void   setPixelsPerSecond(double pps);
    double timeAtX(int x) const;
    int    xAtTime(double t) const;

signals:
    void clipSelected  (const QString& trackId, const QString& clipId);
    void clipDoubleClicked(const QString& trackId, const QString& clipId);
    void showTransition(const QString& trackId, const QString& clipId);
    void showProperties(const QString& trackId, const QString& clipId);

protected:
    void wheelEvent  (QWheelEvent*   e) override;
    void keyPressEvent(QKeyEvent*    e) override;

private slots:
    void onTrackAdded   (const QString& id);
    void onTrackRemoved (const QString& id);
    void onClipAdded    (const QString& trkId, const QString& clipId);
    void onClipMoved    (const QString& trkId, const QString& clipId);
    void onPlayheadMoved(double seconds);
    void onDurationChanged(double seconds);

private:
    void buildUI();
    void syncScrollbars();

    Timeline*         m_timeline   = nullptr;
    double            m_pixPerSec  = 100.0;
    double            m_scrollX    = 0.0;

    TimelineRuler*    m_ruler      = nullptr;
    TrackHeaderArea*  m_headers    = nullptr;
    TrackContentArea* m_content    = nullptr;
    QScrollBar*       m_hScroll    = nullptr;
    QScrollBar*       m_vScroll    = nullptr;
};

// ── Track Header Area ────────────────────────────────────────
class TrackHeaderArea : public QWidget
{
    Q_OBJECT
public:
    explicit TrackHeaderArea(Timeline* tl, QWidget* parent = nullptr);
    void refresh();

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    Timeline* m_timeline;
};

// ── Track Content (clip canvas) ──────────────────────────────
class TrackContentArea : public QWidget
{
    Q_OBJECT
public:
    explicit TrackContentArea(Timeline* tl, TimelineWidget* tw, QWidget* parent = nullptr);
    void refresh();

signals:
    void clipClicked      (const QString& trkId, const QString& clipId);
    void clipDoubleClicked(const QString& trkId, const QString& clipId);
    void clipMoveRequested(const QString& trkId, const QString& clipId,
                           double newStart, const QString& toTrackId);
    void trimStartRequested(const QString& trkId, const QString& clipId, double newStart);
    void trimEndRequested  (const QString& trkId, const QString& clipId, double newEnd);

protected:
    void paintEvent      (QPaintEvent*  e) override;
    void mousePressEvent (QMouseEvent*  e) override;
    void mouseMoveEvent  (QMouseEvent*  e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    enum class DragMode { None, MovingClip, TrimStart, TrimEnd, Playhead };

    void drawTrackBackground(QPainter& p, int trackIndex, const QRect& rect);
    void drawClip           (QPainter& p, Clip* clip, const QRect& rect,
                             bool isVideo, bool isSelected);
    void drawThumbnails     (QPainter& p, Clip* clip, const QRect& rect);
    void drawWaveform       (QPainter& p, Clip* clip, const QRect& rect);
    void drawPlayhead       (QPainter& p);
    void drawSnapLines      (QPainter& p);

    std::pair<QString,QString> clipAt(const QPoint& pos) const;
    QRect clipRect(const Track* trk, int trackY, const Clip* clip) const;
    bool  nearTrimEdge(const QPoint& pos, bool& isStart) const;

    Timeline*       m_timeline;
    TimelineWidget* m_tw;
    DragMode        m_dragMode    = DragMode::None;
    QString         m_dragTrackId;
    QString         m_dragClipId;
    QPoint          m_dragStart;
    double          m_dragOrigStart = 0.0;
    double          m_snapThreshold = 8.0;   // pixels
    QVector<double> m_snapPoints;
    bool            m_showSnap    = false;
    double          m_snapTime    = 0.0;
};
