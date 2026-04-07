// ============================================================
//  TransitionPanel.h
// ============================================================
#pragma once
#include <QWidget>
#include <QListWidget>
#include <QSlider>
#include <QLabel>
#include "core/Timeline.h"

class TransitionPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TransitionPanel(Timeline* tl, QWidget* parent = nullptr);
    void loadClip(const QString& trackId, const QString& clipId);

private slots:
    void onTransitionSelected(QListWidgetItem*);
    void onDurationChanged(int v);

private:
    void buildUI();
    void populateList();

    Timeline*    m_timeline;
    QString      m_trackId, m_clipId;
    QListWidget* m_list;
    QSlider*     m_durSlider;
    QLabel*      m_durLabel;

    struct TransDef { QString id; QString name; QString category; };
    QVector<TransDef> m_defs;
};
// ============================================================
//  TransitionPanel.cpp
// ============================================================
#include "TransitionPanel.h"
#include "assets/VesperTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>

TransitionPanel::TransitionPanel(Timeline* tl, QWidget* parent)
    : QWidget(parent), m_timeline(tl)
{
    setWindowTitle("Transitions");
    setMinimumSize(280, 400);
    setStyleSheet(QString("background: %1; color: %2;")
        .arg(VesperTheme::Surface.name())
        .arg(VesperTheme::TextPrimary.name()));
    buildUI();
}

void TransitionPanel::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto* title = new QLabel("Transitions", this);
    title->setStyleSheet(QString("font-size: %1px; font-weight: bold;")
        .arg(VesperTheme::FontSizeLG));
    root->addWidget(title);

    m_list = new QListWidget(this);
    m_list->setStyleSheet(
        QString("QListWidget { background: %1; border: 1px solid %2; border-radius: 6px; }"
                "QListWidget::item { padding: 6px; }"
                "QListWidget::item:selected { background: %3; border-radius: 4px; }"
                "QListWidget::item:hover { background: %4; border-radius: 4px; }")
        .arg(VesperTheme::SurfaceRaised.name())
        .arg(VesperTheme::SurfaceBorder.name())
        .arg(VesperTheme::Accent.name())
        .arg(VesperTheme::SurfaceBorder.name()));
    populateList();
    root->addWidget(m_list, 1);

    // Duration slider
    auto* durRow = new QHBoxLayout;
    auto* durLbl = new QLabel("Duration:", this);
    durLbl->setFixedWidth(60);
    m_durSlider = new QSlider(Qt::Horizontal, this);
    m_durSlider->setRange(1, 60);   // 0.1 — 3.0 seconds (×0.05)
    m_durSlider->setValue(10);
    m_durLabel  = new QLabel("0.5s", this);
    m_durLabel->setFixedWidth(36);
    durRow->addWidget(durLbl);
    durRow->addWidget(m_durSlider);
    durRow->addWidget(m_durLabel);
    root->addLayout(durRow);

    connect(m_list, &QListWidget::itemClicked,
            this,   &TransitionPanel::onTransitionSelected);
    connect(m_durSlider, &QSlider::valueChanged,
            this,        &TransitionPanel::onDurationChanged);
}

void TransitionPanel::populateList()
{
    m_defs = {
        {"cut",        "Cut",          "Basic"},
        {"dissolve",   "Dissolve",     "Basic"},
        {"fade-black", "Fade to Black","Basic"},
        {"fade-white", "Fade to White","Basic"},
        {"slide-left", "Slide Left",   "Motion"},
        {"slide-right","Slide Right",  "Motion"},
        {"slide-up",   "Slide Up",     "Motion"},
        {"slide-down", "Slide Down",   "Motion"},
        {"zoom-in",    "Zoom In",      "Motion"},
        {"zoom-out",   "Zoom Out",     "Motion"},
        {"glitch",     "Glitch",       "Stylized"},
        {"light-leak", "Light Leak",   "Stylized"},
        {"film-burn",  "Film Burn",    "Stylized"},
        {"spin",       "Spin",         "Stylized"},
        {"whip-pan",   "Whip Pan",     "Stylized"},
        {"cube",       "Cube Rotate",  "3D"},
        {"page-flip",  "Page Flip",    "3D"},
        {"warp",       "Warp",         "3D"},
    };

    QString curCategory;
    for (auto& d : m_defs) {
        if (d.category != curCategory) {
            curCategory = d.category;
            auto* sep = new QListWidgetItem(QString("── %1 ──").arg(curCategory));
            sep->setFlags(Qt::NoItemFlags);
            sep->setForeground(VesperTheme::TextSecondary);
            m_list->addItem(sep);
        }
        auto* item = new QListWidgetItem("  " + d.name);
        item->setData(Qt::UserRole, d.id);
        m_list->addItem(item);
    }
}

void TransitionPanel::loadClip(const QString& trackId, const QString& clipId)
{
    m_trackId = trackId;
    m_clipId  = clipId;
    auto* trk = m_timeline->trackById(trackId);
    if (!trk) return;
    auto clip = trk->clipById(clipId);
    if (!clip) return;

    // Select current transition
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == clip->transitionOut.type)
            m_list->setCurrentRow(i);
    }
    int sliderVal = static_cast<int>(clip->transitionOut.duration / 0.05);
    m_durSlider->setValue(sliderVal);
}

void TransitionPanel::onTransitionSelected(QListWidgetItem* item)
{
    if (!item || !(item->flags() & Qt::ItemIsSelectable)) return;
    QString type = item->data(Qt::UserRole).toString();
    auto* trk = m_timeline->trackById(m_trackId);
    if (!trk) return;
    auto clip = trk->clipById(m_clipId);
    if (!clip) return;
    clip->transitionOut.type = type;
    emit m_timeline->clipModified(m_trackId, m_clipId);
}

void TransitionPanel::onDurationChanged(int v)
{
    double dur = v * 0.05;
    m_durLabel->setText(QString("%1s").arg(dur, 0, 'f', 2));
    auto* trk = m_timeline->trackById(m_trackId);
    if (!trk) return;
    auto clip = trk->clipById(m_clipId);
    if (!clip) return;
    clip->transitionOut.duration = dur;
    emit m_timeline->clipModified(m_trackId, m_clipId);
    }
