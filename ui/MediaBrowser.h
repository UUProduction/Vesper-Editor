// ============================================================
//  MediaBrowser.h — Import panel (bottom drawer)
// ============================================================
#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QFileSystemWatcher>
#include "core/Timeline.h"
#include "core/MediaEngine.h"

class MediaThumbnailItem;

class MediaBrowser : public QWidget
{
    Q_OBJECT
public:
    explicit MediaBrowser(Timeline* timeline, QWidget* parent = nullptr);

    void addFiles(const QStringList& paths);

signals:
    void fileDroppedToTimeline(const QString& filePath, double timelinePosition,
                               const QString& trackId);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent     (QDropEvent*      e) override;

private slots:
    void onImportClicked();
    void onItemDoubleClicked(QListWidgetItem* item);
    void onAddToTimeline();

private:
    void buildItem(const QString& filePath);
    QImage iconForType(Clip::MediaType t, const QString& path);

    Timeline*     m_timeline;
    QListWidget*  m_list;
    QPushButton*  m_btnImport;
    QPushButton*  m_btnAdd;
    QLabel*       m_dropHint;
};
