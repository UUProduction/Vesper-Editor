// ============================================================
//  ProjectManager.cpp
// ============================================================
#include "ProjectManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <QDebug>

ProjectManager& ProjectManager::instance()
{
    static ProjectManager inst;
    return inst;
}

ProjectManager::ProjectManager() {}

bool ProjectManager::newProject(const QString& name)
{
    m_meta.name     = name;
    m_meta.filePath.clear();
    m_meta.created  = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_meta.modified = m_meta.created;
    m_dirty = false;
    if (m_timeline) {
        // Reset timeline
        while (m_timeline->trackCount())
            m_timeline->removeTrack(m_timeline->trackAt(0)->id);
        m_timeline->addVideoTrack("V1");
        m_timeline->addAudioTrack("A1");
    }
    emit dirtyChanged(false);
    return true;
}

bool ProjectManager::openProject(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "ProjectManager: cannot open" << filePath;
        return false;
    }
    QByteArray data = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;
    QJsonObject root = doc.object();

    m_meta.name        = root["name"].toString("Untitled");
    m_meta.filePath    = filePath;
    m_meta.version     = root["version"].toString("1.0");
    m_meta.created     = root["created"].toString();
    m_meta.modified    = root["modified"].toString();
    m_meta.canvasWidth = root["canvasWidth"].toInt(1920);
    m_meta.canvasHeight= root["canvasHeight"].toInt(1080);
    m_meta.frameRate   = root["frameRate"].toDouble(30.0);

    if (m_timeline)
        m_timeline->fromJson(root["timeline"].toString().toUtf8());

    m_dirty = false;
    addRecentProject(filePath);
    emit projectOpened(filePath);
    emit dirtyChanged(false);
    return true;
}

bool ProjectManager::saveProject()
{
    if (m_meta.filePath.isEmpty()) return false;
    return saveProjectAs(m_meta.filePath);
}

bool ProjectManager::saveProjectAs(const QString& filePath)
{
    m_meta.filePath  = filePath;
    m_meta.modified  = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject root;
    root["name"]        = m_meta.name;
    root["version"]     = m_meta.version;
    root["created"]     = m_meta.created;
    root["modified"]    = m_meta.modified;
    root["canvasWidth"] = m_meta.canvasWidth;
    root["canvasHeight"]= m_meta.canvasHeight;
    root["frameRate"]   = m_meta.frameRate;

    if (m_timeline)
        root["timeline"] = QString::fromUtf8(m_timeline->toJson());

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson());
    f.close();

    m_dirty = false;
    addRecentProject(filePath);
    emit projectSaved(filePath);
    emit dirtyChanged(false);
    return true;
}

void ProjectManager::markDirty()
{
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyChanged(true);
    }
}

void ProjectManager::setMeta(const ProjectMeta& m)
{
    m_meta = m;
    markDirty();
}

void ProjectManager::addRecentProject(const QString& path)
{
    m_recentProjects.removeAll(path);
    m_recentProjects.prepend(path);
    while (m_recentProjects.size() > 10)
        m_recentProjects.removeLast();
}

QString ProjectManager::recentProjectsJson() const
{
    QJsonArray arr;
    for (auto& p : m_recentProjects) arr.append(p);
    return QJsonDocument(arr).toJson();
}
