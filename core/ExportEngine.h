// ============================================================
//  ExportEngine.h — FFmpeg export pipeline
// ============================================================
#pragma once

#include <QObject>
#include <QString>
#include <QThread>
#include "Timeline.h"

struct ExportSettings {
    QString  outputPath;
    int      width       = 1920;
    int      height      = 1080;
    double   frameRate   = 30.0;
    int      videoBitrate= 8000000;    // bps
    int      audioBitrate= 192000;
    int      audioRate   = 44100;
    enum class Format  { MP4_H264, MP4_H265, MOV, GIF } format = Format::MP4_H264;
    enum class Quality { Low, Medium, High, Lossless }  quality= Quality::High;
    bool     includeAudio = true;
};

class ExportEngine : public QObject
{
    Q_OBJECT
public:
    explicit ExportEngine(QObject* parent = nullptr);
    ~ExportEngine() override;

    void startExport(Timeline* timeline, const ExportSettings& settings);
    void cancelExport();
    bool isExporting() const { return m_exporting; }

signals:
    void progressChanged(int percent, double estimatedSecondsRemaining);
    void exportComplete(const QString& outputPath);
    void exportFailed  (const QString& error);
    void exportCancelled();

private:
    void doExport(Timeline* timeline, ExportSettings settings);

    bool m_exporting = false;
    bool m_cancel    = false;
    QThread* m_thread = nullptr;
};
