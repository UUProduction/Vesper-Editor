// ============================================================
//  PreviewPlayer.h — Real-time video preview
// ============================================================
#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <memory>
#include "core/Timeline.h"
#include "core/Renderer.h"
#include "core/MediaEngine.h"

class PreviewPlayer : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewPlayer(Timeline* timeline, QWidget* parent = nullptr);
    ~PreviewPlayer() override = default;

    void seekTo(double seconds);
    void setTimeline(Timeline* tl);

public slots:
    void onClipNeedsFrame(const QString& trackId, const QString& clipId);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent (QPaintEvent*  event) override;

private:
    void scheduleFrameDecode(double seconds);
    void decodeAndUploadFrame(double seconds);

    Timeline*          m_timeline = nullptr;
    Renderer*          m_renderer = nullptr;
    double             m_currentTime = 0.0;

    // Per-clip decoder cache
    QMap<QString, std::unique_ptr<MediaDecoder>> m_decoders;

    QTimer*            m_decodeThrottle = nullptr;
    double             m_pendingTime    = 0.0;
};
