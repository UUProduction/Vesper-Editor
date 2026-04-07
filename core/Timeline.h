// ============================================================
//  Timeline.h — Track/Clip Data Model
// ============================================================
#pragma once

#include <QString>
#include <QVector>
#include <QMap>
#include <QVariant>
#include <QImage>
#include <memory>
#include <functional>

// ── Keyframe ─────────────────────────────────────────────────
struct Keyframe {
    double  timeSeconds = 0.0;
    double  value       = 0.0;
    QString easingType  = "linear";  // linear, ease-in, ease-out, bezier
};

// ── Clip ──────────────────────────────────────────────────────
struct Clip {
    // Identity
    QString id;
    QString name;
    QString filePath;

    // Timeline position
    double  timelineStart  = 0.0;   // seconds on timeline
    double  timelineEnd    = 0.0;

    // Source in/out (for trimming)
    double  sourceIn       = 0.0;
    double  sourceOut      = 0.0;
    double  sourceDuration = 0.0;

    // Type
    enum class MediaType { Video, Audio, Image, Text, Effect };
    MediaType mediaType = MediaType::Video;

    // Transform (all keyframeable)
    double  posX        = 0.0;
    double  posY        = 0.0;
    double  scaleX      = 1.0;
    double  scaleY      = 1.0;
    double  rotation    = 0.0;
    double  opacity     = 1.0;
    double  volume      = 1.0;
    double  pan         = 0.0;     // -1.0 left, 0.0 center, 1.0 right

    // Crop
    double  cropLeft    = 0.0;
    double  cropRight   = 0.0;
    double  cropTop     = 0.0;
    double  cropBottom  = 0.0;

    // Aspect ratio lock for crop
    bool    cropLocked  = false;
    double  cropAspect  = 16.0 / 9.0;

    // Text properties (for text clips)
    QString textContent;
    QString fontFamily  = "Inter";
    int     fontSize    = 48;
    bool    fontBold    = false;
    bool    fontItalic  = false;
    bool    fontUnder   = false;
    QString textColor   = "#FFFFFF";
    QString textBgMode  = "none";   // none, solid, gradient, blur
    QString textBgColor = "#000000";
    int     textAlign   = 1;        // 0=left,1=center,2=right
    double  letterSpacing = 0.0;
    double  lineHeight  = 1.2;
    QString textAnim    = "none";   // fade-in, slide-up, typewriter...

    // Transition between THIS clip and the NEXT
    struct Transition {
        QString type     = "cut";   // cut, dissolve, slide-left, glitch...
        double  duration = 0.5;
    } transitionOut;

    // Keyframe tracks
    QMap<QString, QVector<Keyframe>> keyframes;

    // Thumbnail cache (for timeline display)
    QImage  thumbnail;
    bool    thumbnailDirty = true;

    // Audio waveform data (normalized 0..1 samples)
    QVector<float> waveformData;

    // Computed helpers
    double duration()    const { return timelineEnd - timelineStart; }
    bool   isSelected    = false;
    bool   isMuted       = false;
    bool   isLocked      = false;

    // Generate a unique id
    static QString newId();
};

// ── Track ─────────────────────────────────────────────────────
struct Track {
    QString id;
    QString name;

    enum class TrackType { Video, Audio };
    TrackType type = TrackType::Video;

    QVector<std::shared_ptr<Clip>> clips;

    bool isMuted    = false;
    bool isLocked   = false;
    bool isVisible  = true;

    int  height     = 56;          // px (matches VesperTheme::TimelineHeight)
    int  sortOrder  = 0;           // lower = bottom of stack

    // Find clip at a specific timeline time
    std::shared_ptr<Clip> clipAt(double timeSeconds) const;

    // Find clip by id
    std::shared_ptr<Clip> clipById(const QString& id) const;

    // Sort clips by timelineStart (call after moving)
    void sortClips();

    // Check for overlap with a proposed clip placement
    bool wouldOverlap(double start, double end, const QString& excludeId = "") const;

    static QString newId();
};

// ── Marker ────────────────────────────────────────────────────
struct Marker {
    QString id;
    QString label;
    double  timeSeconds = 0.0;
    QString color       = "#F0C060";
};

// ── Timeline ──────────────────────────────────────────────────
class Timeline : public QObject
{
    Q_OBJECT
public:
    explicit Timeline(QObject* parent = nullptr);
    ~Timeline() override = default;

    // ── Tracks ──────────────────────────────────────────────
    Track*  addVideoTrack(const QString& name = "");
    Track*  addAudioTrack(const QString& name = "");
    bool    removeTrack(const QString& trackId);
    Track*  trackById(const QString& id);
    Track*  trackAt(int index);
    int     trackCount()      const;
    int     videoTrackCount() const;
    int     audioTrackCount() const;

    const QVector<std::shared_ptr<Track>>& tracks() const { return m_tracks; }

    // ── Clips ───────────────────────────────────────────────
    std::shared_ptr<Clip> addClip(const QString& trackId,
                                  const QString& filePath,
                                  double         timelineStart,
                                  Clip::MediaType type);
    bool   removeClip(const QString& trackId, const QString& clipId);
    bool   moveClip  (const QString& fromTrackId, const QString& clipId,
                      const QString& toTrackId,   double         newStart);
    bool   trimClipStart(const QString& trackId, const QString& clipId, double newStart);
    bool   trimClipEnd  (const QString& trackId, const QString& clipId, double newEnd);
    std::shared_ptr<Clip> splitClip(const QString& trackId,
                                    const QString& clipId,
                                    double         splitTime);
    bool   rippleDelete (const QString& trackId, const QString& clipId);

    // ── Selection ───────────────────────────────────────────
    void   selectClip  (const QString& trackId, const QString& clipId, bool add = false);
    void   deselectAll ();
    QVector<std::pair<QString,QString>> selectedClips() const;

    // ── Playhead ────────────────────────────────────────────
    double playheadPosition()     const { return m_playhead; }
    void   setPlayheadPosition(double t);

    double duration() const;         // max clip end across all tracks

    // ── Markers ─────────────────────────────────────────────
    Marker* addMarker(double timeSeconds, const QString& label = "");
    void    removeMarker(const QString& markerId);
    const QVector<Marker>& markers() const { return m_markers; }

    // ── Undo / Redo ─────────────────────────────────────────
    void    pushUndo(const QString& description);
    void    undo();
    void    redo();
    bool    canUndo() const;
    bool    canRedo() const;
    QString undoDescription() const;
    QString redoDescription() const;

    // ── Serialization ───────────────────────────────────────
    QByteArray  toJson()            const;
    bool        fromJson(const QByteArray& data);

signals:
    void trackAdded     (const QString& trackId);
    void trackRemoved   (const QString& trackId);
    void clipAdded      (const QString& trackId, const QString& clipId);
    void clipRemoved    (const QString& trackId, const QString& clipId);
    void clipMoved      (const QString& trackId, const QString& clipId);
    void clipModified   (const QString& trackId, const QString& clipId);
    void playheadMoved  (double seconds);
    void durationChanged(double seconds);
    void selectionChanged();
    void undoStackChanged();
    void markerAdded    (const QString& markerId);

private:
    struct UndoEntry {
        QString   description;
        QByteArray snapshot;
    };

    QVector<std::shared_ptr<Track>> m_tracks;
    QVector<Marker>                 m_markers;
    double                          m_playhead = 0.0;

    QVector<UndoEntry>              m_undoStack;
    int                             m_undoIndex = -1;
    static constexpr int            MaxUndo = 100;

    int  m_videoCount = 0;
    int  m_audioCount = 0;

    QByteArray snapshotJson() const;
};
