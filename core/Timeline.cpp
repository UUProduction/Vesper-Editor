// ============================================================
//  Timeline.cpp
// ============================================================
#include "Timeline.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <algorithm>

// ── Clip helpers ──────────────────────────────────────────────
QString Clip::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

// ── Track helpers ─────────────────────────────────────────────
QString Track::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

std::shared_ptr<Clip> Track::clipAt(double t) const
{
    for (auto& c : clips)
        if (t >= c->timelineStart && t < c->timelineEnd)
            return c;
    return nullptr;
}

std::shared_ptr<Clip> Track::clipById(const QString& id) const
{
    for (auto& c : clips)
        if (c->id == id) return c;
    return nullptr;
}

void Track::sortClips()
{
    std::sort(clips.begin(), clips.end(),
        [](const std::shared_ptr<Clip>& a, const std::shared_ptr<Clip>& b){
            return a->timelineStart < b->timelineStart;
        });
}

bool Track::wouldOverlap(double start, double end, const QString& excludeId) const
{
    for (auto& c : clips) {
        if (c->id == excludeId) continue;
        if (start < c->timelineEnd && end > c->timelineStart)
            return true;
    }
    return false;
}

// ── Timeline ──────────────────────────────────────────────────
Timeline::Timeline(QObject* parent) : QObject(parent) {}

Track* Timeline::addVideoTrack(const QString& name)
{
    auto t       = std::make_shared<Track>();
    t->id        = Track::newId();
    t->type      = Track::TrackType::Video;
    t->name      = name.isEmpty() ? QString("V%1").arg(++m_videoCount) : name;
    t->sortOrder = static_cast<int>(m_tracks.size());
    m_tracks.append(t);
    emit trackAdded(t->id);
    return t.get();
}

Track* Timeline::addAudioTrack(const QString& name)
{
    auto t       = std::make_shared<Track>();
    t->id        = Track::newId();
    t->type      = Track::TrackType::Audio;
    t->name      = name.isEmpty() ? QString("A%1").arg(++m_audioCount) : name;
    t->sortOrder = static_cast<int>(m_tracks.size());
    m_tracks.append(t);
    emit trackAdded(t->id);
    return t.get();
}

bool Timeline::removeTrack(const QString& trackId)
{
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i]->id == trackId) {
            pushUndo("Remove track");
            m_tracks.removeAt(i);
            emit trackRemoved(trackId);
            return true;
        }
    }
    return false;
}

Track* Timeline::trackById(const QString& id)
{
    for (auto& t : m_tracks)
        if (t->id == id) return t.get();
    return nullptr;
}

Track* Timeline::trackAt(int index)
{
    if (index < 0 || index >= m_tracks.size()) return nullptr;
    return m_tracks[index].get();
}

int Timeline::trackCount()      const { return m_tracks.size(); }
int Timeline::videoTrackCount() const {
    int n = 0;
    for (auto& t : m_tracks) if (t->type == Track::TrackType::Video) ++n;
    return n;
}
int Timeline::audioTrackCount() const {
    int n = 0;
    for (auto& t : m_tracks) if (t->type == Track::TrackType::Audio) ++n;
    return n;
}

std::shared_ptr<Clip> Timeline::addClip(const QString& trackId,
                                        const QString& filePath,
                                        double         timelineStart,
                                        Clip::MediaType type)
{
    Track* trk = trackById(trackId);
    if (!trk) return nullptr;

    auto clip              = std::make_shared<Clip>();
    clip->id               = Clip::newId();
    clip->filePath         = filePath;
    clip->mediaType        = type;
    clip->timelineStart    = timelineStart;
    clip->timelineEnd      = timelineStart + 5.0;  // default 5 sec; MediaEngine updates
    clip->sourceDuration   = 5.0;
    clip->sourceOut        = 5.0;
    clip->name             = QFileInfo(filePath).baseName();
    clip->thumbnailDirty   = true;

    if (trk->wouldOverlap(clip->timelineStart, clip->timelineEnd))
        return nullptr;   // caller should ripple/shift if desired

    pushUndo("Add clip");
    trk->clips.append(clip);
    trk->sortClips();
    emit clipAdded(trackId, clip->id);
    emit durationChanged(duration());
    return clip;
}

bool Timeline::removeClip(const QString& trackId, const QString& clipId)
{
    Track* trk = trackById(trackId);
    if (!trk) return false;
    for (int i = 0; i < trk->clips.size(); ++i) {
        if (trk->clips[i]->id == clipId) {
            pushUndo("Remove clip");
            trk->clips.removeAt(i);
            emit clipRemoved(trackId, clipId);
            emit durationChanged(duration());
            return true;
        }
    }
    return false;
}

bool Timeline::moveClip(const QString& fromTrackId, const QString& clipId,
                        const QString& toTrackId,   double         newStart)
{
    Track* from = trackById(fromTrackId);
    Track* to   = trackById(toTrackId);
    if (!from || !to) return false;

    auto clip = from->clipById(clipId);
    if (!clip) return false;

    double dur = clip->duration();
    if (to->wouldOverlap(newStart, newStart + dur, clipId)) return false;

    pushUndo("Move clip");
    if (fromTrackId != toTrackId) {
        from->clips.removeIf([&](const std::shared_ptr<Clip>& c){ return c->id == clipId; });
        to->clips.append(clip);
    }
    clip->timelineStart = newStart;
    clip->timelineEnd   = newStart + dur;
    to->sortClips();
    emit clipMoved(toTrackId, clipId);
    emit durationChanged(duration());
    return true;
}

bool Timeline::trimClipStart(const QString& trackId, const QString& clipId, double newStart)
{
    Track* trk = trackById(trackId);
    if (!trk) return false;
    auto clip = trk->clipById(clipId);
    if (!clip) return false;

    double delta = newStart - clip->timelineStart;
    double newSourceIn = clip->sourceIn + delta;
    if (newSourceIn < 0 || newSourceIn >= clip->sourceOut) return false;

    pushUndo("Trim start");
    clip->timelineStart = newStart;
    clip->sourceIn      = newSourceIn;
    emit clipModified(trackId, clipId);
    return true;
}

bool Timeline::trimClipEnd(const QString& trackId, const QString& clipId, double newEnd)
{
    Track* trk = trackById(trackId);
    if (!trk) return false;
    auto clip = trk->clipById(clipId);
    if (!clip) return false;

    double newSourceOut = clip->sourceIn + (newEnd - clip->timelineStart);
    if (newSourceOut <= clip->sourceIn || newSourceOut > clip->sourceDuration) return false;

    pushUndo("Trim end");
    clip->timelineEnd = newEnd;
    clip->sourceOut   = newSourceOut;
    emit clipModified(trackId, clipId);
    emit durationChanged(duration());
    return true;
}

std::shared_ptr<Clip> Timeline::splitClip(const QString& trackId,
                                          const QString& clipId,
                                          double         splitTime)
{
    Track* trk = trackById(trackId);
    if (!trk) return nullptr;
    auto clip = trk->clipById(clipId);
    if (!clip) return nullptr;
    if (splitTime <= clip->timelineStart || splitTime >= clip->timelineEnd) return nullptr;

    pushUndo("Split clip");

    // Create right half
    auto right = std::make_shared<Clip>(*clip);
    right->id            = Clip::newId();
    right->timelineStart = splitTime;
    right->sourceIn      = clip->sourceIn + (splitTime - clip->timelineStart);
    right->thumbnailDirty = true;

    // Truncate left half
    clip->timelineEnd = splitTime;
    clip->sourceOut   = right->sourceIn;

    trk->clips.append(right);
    trk->sortClips();
    emit clipAdded(trackId, right->id);
    emit clipModified(trackId, clipId);
    return right;
}

bool Timeline::rippleDelete(const QString& trackId, const QString& clipId)
{
    Track* trk = trackById(trackId);
    if (!trk) return false;
    auto clip = trk->clipById(clipId);
    if (!clip) return false;

    double gapStart = clip->timelineStart;
    double gapSize  = clip->duration();

    pushUndo("Ripple delete");
    trk->clips.removeIf([&](const std::shared_ptr<Clip>& c){ return c->id == clipId; });

    // Shift all clips that come after
    for (auto& c : trk->clips)
        if (c->timelineStart >= gapStart) {
            c->timelineStart -= gapSize;
            c->timelineEnd   -= gapSize;
        }

    emit clipRemoved(trackId, clipId);
    emit durationChanged(duration());
    return true;
}

void Timeline::selectClip(const QString& trackId, const QString& clipId, bool add)
{
    if (!add) deselectAll();
    Track* trk = trackById(trackId);
    if (!trk) return;
    auto clip = trk->clipById(clipId);
    if (clip) clip->isSelected = true;
    emit selectionChanged();
}

void Timeline::deselectAll()
{
    for (auto& t : m_tracks)
        for (auto& c : t->clips)
            c->isSelected = false;
    emit selectionChanged();
}

QVector<std::pair<QString,QString>> Timeline::selectedClips() const
{
    QVector<std::pair<QString,QString>> out;
    for (auto& t : m_tracks)
        for (auto& c : t->clips)
            if (c->isSelected) out.append({t->id, c->id});
    return out;
}

void Timeline::setPlayheadPosition(double t)
{
    m_playhead = qMax(0.0, qMin(t, duration()));
    emit playheadMoved(m_playhead);
}

double Timeline::duration() const
{
    double d = 0.0;
    for (auto& t : m_tracks)
        for (auto& c : t->clips)
            d = qMax(d, c->timelineEnd);
    return d;
}

Marker* Timeline::addMarker(double timeSeconds, const QString& label)
{
    Marker m;
    m.id          = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    m.label       = label;
    m.timeSeconds = timeSeconds;
    m_markers.append(m);
    emit markerAdded(m.id);
    return &m_markers.last();
}

void Timeline::removeMarker(const QString& markerId)
{
    m_markers.removeIf([&](const Marker& m){ return m.id == markerId; });
}

// ── Undo/Redo ────────────────────────────────────────────────
QByteArray Timeline::snapshotJson() const { return toJson(); }

void Timeline::pushUndo(const QString& description)
{
    // Truncate redo branch
    while (m_undoStack.size() > m_undoIndex + 1)
        m_undoStack.removeLast();

    UndoEntry e;
    e.description = description;
    e.snapshot    = snapshotJson();
    m_undoStack.append(e);
    if (m_undoStack.size() > MaxUndo) m_undoStack.removeFirst();
    m_undoIndex = m_undoStack.size() - 1;
    emit undoStackChanged();
}

void Timeline::undo()
{
    if (!canUndo()) return;
    --m_undoIndex;
    if (m_undoIndex >= 0)
        fromJson(m_undoStack[m_undoIndex].snapshot);
    emit undoStackChanged();
}

void Timeline::redo()
{
    if (!canRedo()) return;
    ++m_undoIndex;
    fromJson(m_undoStack[m_undoIndex].snapshot);
    emit undoStackChanged();
}

bool Timeline::canUndo() const { return m_undoIndex > 0; }
bool Timeline::canRedo() const { return m_undoIndex < m_undoStack.size() - 1; }

QString Timeline::undoDescription() const
{
    if (!canUndo()) return {};
    return m_undoStack[m_undoIndex].description;
}

QString Timeline::redoDescription() const
{
    if (!canRedo()) return {};
    return m_undoStack[m_undoIndex + 1].description;
}

// ── JSON Serialization ────────────────────────────────────────
QByteArray Timeline::toJson() const
{
    QJsonObject root;
    root["playhead"] = m_playhead;

    QJsonArray tracksArr;
    for (auto& t : m_tracks) {
        QJsonObject tj;
        tj["id"]      = t->id;
        tj["name"]    = t->name;
        tj["type"]    = (t->type == Track::TrackType::Video) ? "video" : "audio";
        tj["muted"]   = t->isMuted;
        tj["locked"]  = t->isLocked;
        tj["visible"] = t->isVisible;

        QJsonArray clipsArr;
        for (auto& c : t->clips) {
            QJsonObject cj;
            cj["id"]             = c->id;
            cj["filePath"]       = c->filePath;
            cj["name"]           = c->name;
            cj["timelineStart"]  = c->timelineStart;
            cj["timelineEnd"]    = c->timelineEnd;
            cj["sourceIn"]       = c->sourceIn;
            cj["sourceOut"]      = c->sourceOut;
            cj["posX"]           = c->posX;
            cj["posY"]           = c->posY;
            cj["scaleX"]         = c->scaleX;
            cj["scaleY"]         = c->scaleY;
            cj["rotation"]       = c->rotation;
            cj["opacity"]        = c->opacity;
            cj["volume"]         = c->volume;
            cj["muted"]          = c->isMuted;
            cj["textContent"]    = c->textContent;
            cj["fontFamily"]     = c->fontFamily;
            cj["fontSize"]       = c->fontSize;
            cj["textColor"]      = c->textColor;
            clipsArr.append(cj);
        }
        tj["clips"] = clipsArr;
        tracksArr.append(tj);
    }
    root["tracks"] = tracksArr;

    QJsonArray markersArr;
    for (auto& m : m_markers) {
        QJsonObject mj;
        mj["id"]    = m.id;
        mj["label"] = m.label;
        mj["time"]  = m.timeSeconds;
        mj["color"] = m.color;
        markersArr.append(mj);
    }
    root["markers"] = markersArr;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool Timeline::fromJson(const QByteArray& data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;
    QJsonObject root = doc.object();

    m_playhead = root["playhead"].toDouble();
    m_tracks.clear();
    m_markers.clear();
    m_videoCount = 0;
    m_audioCount = 0;

    for (auto tv : root["tracks"].toArray()) {
        QJsonObject tj = tv.toObject();
        auto trk      = std::make_shared<Track>();
        trk->id       = tj["id"].toString();
        trk->name     = tj["name"].toString();
        trk->type     = (tj["type"].toString() == "video")
                        ? Track::TrackType::Video : Track::TrackType::Audio;
        trk->isMuted  = tj["muted"].toBool();
        trk->isLocked = tj["locked"].toBool();
        trk->isVisible= tj["visible"].toBool();

        if (trk->type == Track::TrackType::Video) ++m_videoCount;
        else                                       ++m_audioCount;

        for (auto cv : tj["clips"].toArray()) {
            QJsonObject cj = cv.toObject();
            auto c = std::make_shared<Clip>();
            c->id            = cj["id"].toString();
            c->filePath      = cj["filePath"].toString();
            c->name          = cj["name"].toString();
            c->timelineStart = cj["timelineStart"].toDouble();
            c->timelineEnd   = cj["timelineEnd"].toDouble();
            c->sourceIn      = cj["sourceIn"].toDouble();
            c->sourceOut     = cj["sourceOut"].toDouble();
            c->posX          = cj["posX"].toDouble();
            c->posY          = cj["posY"].toDouble();
            c->scaleX        = cj["scaleX"].toDouble(1.0);
            c->scaleY        = cj["scaleY"].toDouble(1.0);
            c->rotation      = cj["rotation"].toDouble();
            c->opacity       = cj["opacity"].toDouble(1.0);
            c->volume        = cj["volume"].toDouble(1.0);
            c->isMuted       = cj["muted"].toBool();
            c->textContent   = cj["textContent"].toString();
            c->fontFamily    = cj["fontFamily"].toString("Inter");
            c->fontSize      = cj["fontSize"].toInt(48);
            c->textColor     = cj["textColor"].toString("#FFFFFF");
            c->thumbnailDirty = true;
            trk->clips.append(c);
        }
        trk->sortClips();
        m_tracks.append(trk);
    }

    for (auto mv : root["markers"].toArray()) {
        QJsonObject mj = mv.toObject();
        Marker m;
        m.id          = mj["id"].toString();
        m.label       = mj["label"].toString();
        m.timeSeconds = mj["time"].toDouble();
        m.color       = mj["color"].toString("#F0C060");
        m_markers.append(m);
    }
    return true;
}
