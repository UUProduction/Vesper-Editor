// ============================================================
//  ExportDialog.h
// ============================================================
#pragma once
#include <QDialog>
#include <QComboBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include "core/Timeline.h"
#include "core/ExportEngine.h"

class ExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportDialog(Timeline* tl, QWidget* parent = nullptr);
    ~ExportDialog() override;

private slots:
    void onStartExport();
    void onCancelExport();
    void onProgress(int pct, double eta);
    void onComplete(const QString& path);
    void onFailed(const QString& msg);

private:
    void buildUI();
    ExportSettings buildSettings() const;

    Timeline*       m_timeline;
    ExportEngine*   m_engine;

    QComboBox*      m_cmbResolution;
    QComboBox*      m_cmbFrameRate;
    QComboBox*      m_cmbFormat;
    QComboBox*      m_cmbQuality;
    QProgressBar*   m_progress;
    QLabel*         m_lblStatus;
    QPushButton*    m_btnExport;
    QPushButton*    m_btnCancel;
    bool            m_exporting = false;
};
// ============================================================
//  ExportDialog.cpp
// ============================================================
#include "ExportDialog.h"
#include "assets/VesperTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

ExportDialog::ExportDialog(Timeline* tl, QWidget* parent)
    : QDialog(parent), m_timeline(tl), m_engine(new ExportEngine(this))
{
    setWindowTitle("Export Video");
    setModal(true);
    setMinimumWidth(360);
    setStyleSheet(QString("background: %1; color: %2;")
        .arg(VesperTheme::Surface.name())
        .arg(VesperTheme::TextPrimary.name()));
    buildUI();

    connect(m_engine, &ExportEngine::progressChanged, this, &ExportDialog::onProgress);
    connect(m_engine, &ExportEngine::exportComplete,  this, &ExportDialog::onComplete);
    connect(m_engine, &ExportEngine::exportFailed,    this, &ExportDialog::onFailed);
    connect(m_engine, &ExportEngine::exportCancelled, [this]{
        m_lblStatus->setText("Export cancelled.");
        m_progress->setValue(0);
        m_btnExport->setEnabled(true);
        m_btnCancel->setEnabled(false);
        m_exporting = false;
    });
}

ExportDialog::~ExportDialog() {}

void ExportDialog::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    auto* title = new QLabel("Export Settings", this);
    title->setStyleSheet(QString("font-size: %1px; font-weight: bold;")
        .arg(VesperTheme::FontSizeXL));
    root->addWidget(title);

    auto* frm = new QFormLayout;
    frm->setLabelAlignment(Qt::AlignRight);
    frm->setSpacing(8);

    auto makeCombo = [&](const QStringList& items) {
        auto* c = new QComboBox(this);
        c->addItems(items);
        c->setStyleSheet(
            QString("QComboBox { background: %1; border: 1px solid %2;"
                    " border-radius: 4px; padding: 4px 8px; color: %3; }"
                    "QComboBox::drop-down { border: none; }"
                    "QComboBox QAbstractItemView { background: %1; color: %3; }")
            .arg(VesperTheme::SurfaceRaised.name())
            .arg(VesperTheme::SurfaceBorder.name())
            .arg(VesperTheme::TextPrimary.name()));
        return c;
    };

    m_cmbResolution = makeCombo({"480p (854×480)","720p (1280×720)",
                                  "1080p (1920×1080)","2K (2560×1440)",
                                  "4K (3840×2160)"});
    m_cmbResolution->setCurrentIndex(2);

    m_cmbFrameRate = makeCombo({"24 fps","25 fps","30 fps","60 fps","120 fps"});
    m_cmbFrameRate->setCurrentIndex(2);

    m_cmbFormat = makeCombo({"MP4 (H.264)","MP4 (H.265/HEVC)","MOV","GIF"});

    m_cmbQuality = makeCombo({"Low","Medium","High","Lossless"});
    m_cmbQuality->setCurrentIndex(2);

    frm->addRow("Resolution:", m_cmbResolution);
    frm->addRow("Frame Rate:", m_cmbFrameRate);
    frm->addRow("Format:",     m_cmbFormat);
    frm->addRow("Quality:",    m_cmbQuality);
    root->addLayout(frm);

    // Progress
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setStyleSheet(
        QString("QProgressBar { background: %1; border-radius: 4px; height: 8px; }"
                "QProgressBar::chunk { background: %2; border-radius: 4px; }")
        .arg(VesperTheme::SurfaceRaised.name())
        .arg(VesperTheme::Accent.name()));
    root->addWidget(m_progress);

    m_lblStatus = new QLabel("Ready to export.", this);
    m_lblStatus->setStyleSheet(
        QString("color: %1; font-size: %2px;")
        .arg(VesperTheme::TextSecondary.name())
        .arg(VesperTheme::FontSizeSM));
    root->addWidget(m_lblStatus);

    // Buttons
    auto* btnRow = new QHBoxLayout;
    m_btnCancel = new QPushButton("Cancel", this);
    m_btnExport = new QPushButton("Export ▶", this);
    m_btnCancel->setStyleSheet(VesperTheme::iconButtonStyle());
    m_btnExport->setStyleSheet(VesperTheme::accentButtonStyle());
    m_btnCancel->setEnabled(false);
    btnRow->addWidget(m_btnCancel);
    btnRow->addStretch();
    btnRow->addWidget(m_btnExport);
    root->addLayout(btnRow);

    connect(m_btnExport, &QPushButton::clicked, this, &ExportDialog::onStartExport);
    connect(m_btnCancel, &QPushButton::clicked, this, &ExportDialog::onCancelExport);
}

ExportSettings ExportDialog::buildSettings() const
{
    ExportSettings s;
    static const QVector<QPair<int,int>> resolutions = {
        {854,480},{1280,720},{1920,1080},{2560,1440},{3840,2160}
    };
    auto [w,h]   = resolutions[m_cmbResolution->currentIndex()];
    s.width      = w;
    s.height     = h;

    static const double fps[] = {24,25,30,60,120};
    s.frameRate  = fps[m_cmbFrameRate->currentIndex()];

    static const ExportSettings::Format fmts[] = {
        ExportSettings::Format::MP4_H264,
        ExportSettings::Format::MP4_H265,
        ExportSettings::Format::MOV,
        ExportSettings::Format::GIF
    };
    s.format = fmts[m_cmbFormat->currentIndex()];

    static const ExportSettings::Quality quals[] = {
        ExportSettings::Quality::Low,
        ExportSettings::Quality::Medium,
        ExportSettings::Quality::High,
        ExportSettings::Quality::Lossless
    };
    s.quality = quals[m_cmbQuality->currentIndex()];
    return s;
}

void ExportDialog::onStartExport()
{
    static const QStringList exts = {"mp4","mp4","mov","gif"};
    int fi = m_cmbFormat->currentIndex();
    QString filter = QString("Video Files (*.%1)").arg(exts[fi]);
    QString path   = QFileDialog::getSaveFileName(this, "Save Export As",
                         "output." + exts[fi], filter);
    if (path.isEmpty()) return;

    ExportSettings s = buildSettings();
    s.outputPath = path;

    m_exporting = true;
    m_btnExport->setEnabled(false);
    m_btnCancel->setEnabled(true);
    m_progress->setValue(0);
    m_lblStatus->setText("Exporting…");
    m_engine->startExport(m_timeline, s);
}

void ExportDialog::onCancelExport()
{
    m_engine->cancelExport();
}

void ExportDialog::onProgress(int pct, double eta)
{
    m_progress->setValue(pct);
    m_lblStatus->setText(
        QString("Encoding… %1%  (ETA: %2s)")
        .arg(pct)
        .arg(static_cast<int>(eta)));
}

void ExportDialog::onComplete(const QString& path)
{
    m_progress->setValue(100);
    m_lblStatus->setText("Export complete! ✓");
    m_btnExport->setEnabled(true);
    m_btnCancel->setEnabled(false);
    m_exporting = false;
    QMessageBox::information(this, "Export Complete",
        QString("Video saved to:\n%1").arg(path));
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).dir().absolutePath()));
}

void ExportDialog::onFailed(const QString& msg)
{
    m_lblStatus->setText("Export failed: " + msg);
    m_btnExport->setEnabled(true);
    m_btnCancel->setEnabled(false);
    m_exporting = false;
    QMessageBox::critical(this, "Export Failed", msg);
}
