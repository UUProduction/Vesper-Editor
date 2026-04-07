// ============================================================
//  ProjectManager.h — Save/load project state
// ============================================================
#pragma once
#include <QObject>
#include <QString>
#include "Timeline.h"

struct ProjectMeta {
    QString name        = "Untitled Project";
    QString filePath;
    QString version     = "1.0";
    QString created;
    QString modified;
    int     canvasWidth  = 1920;
    int     canvasHeight = 1080;
    double  frameRate    = 30.0;
};

class ProjectManager : public QObject
{
    Q_OBJECT
public:
    static ProjectManager& instance();

    bool        newProject(const QString& name = "Untitled Project");
    bool        openProject(const QString& filePath);
    bool        saveProject();
    bool        saveProjectAs(const QString& filePath);
    bool        isDirty()     const { return m_dirty; }

    void        setTimeline(Timeline* tl) { m_timeline = tl; }
    Timeline*   timeline()  const { return m_timeline; }
    ProjectMeta meta()      const { return m_meta; }
    void        setMeta(const ProjectMeta& m);

    QString     recentProjectsJson() const;
    void        addRecentProject(const QString& path);

signals:
    void projectOpened(const QString& path);
    void projectSaved (const QString& path);
    void dirtyChanged (bool dirty);

public slots:
    void markDirty();

private:
    ProjectManager();
    ~ProjectManager() override = default;

    Timeline*    m_timeline = nullptr;
    ProjectMeta  m_meta;
    bool         m_dirty    = false;
    QStringList  m_recentProjects;
};
