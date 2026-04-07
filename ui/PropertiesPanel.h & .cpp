// ============================================================
//  PropertiesPanel.h
// ============================================================
#pragma once
#include <QWidget>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include "core/Timeline.h"

class PropertiesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PropertiesPanel(Timeline* tl, QWidget* parent = nullptr);
    void loadClip(const QString& trackId, const QString& clipId);

private slots:
    void onValueChanged();

private:
    void buildUI();
    void applyToClip();

    Timeline*     m_timeline;
    QString       m_trackId, m_clipId;
    bool          m_loading = false;

    QDoubleSpinBox* m_spnPosX;
    QDoubleSpinBox* m_spnPosY;
    QDoubleSpinBox* m_spnScaleX;
    QDoubleSpinBox* m_spnScaleY;
    QDoubleSpinBox* m_spnRotation;
    QSlider*        m_sldOpacity;
    QLabel*         m_lblOpacity;
    QDoubleSpinBox* m_spnCropL;
    QDoubleSpinBox* m_spnCropR;
    QDoubleSpinBox* m_spnCropT;
    QDoubleSpinBox* m_spnCropB;
};
// ============================================================
//  PropertiesPanel.cpp
// ============================================================
#include "PropertiesPanel.h"
#include "assets/VesperTheme.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>

PropertiesPanel::PropertiesPanel(Timeline* tl, QWidget* parent)
    : QWidget(parent), m_timeline(tl)
{
    setWindowTitle("Properties");
    setMinimumWidth(260);
    setStyleSheet(QString("background: %1; color: %2;")
        .arg(VesperTheme::Surface.name())
        .arg(VesperTheme::TextPrimary.name()));
    buildUI();
}

void PropertiesPanel::buildUI()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* root    = new QVBoxLayout(content);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    auto makeSpinBox = [&](double min, double max, double step,
                            const QString& suffix = "") {
        auto* s = new QDoubleSpinBox;
        s->setRange(min, max);
        s->setSingleStep(step);
        s->setSuffix(suffix);
        s->setStyleSheet(
            QString("QDoubleSpinBox { background: %1; border: 1px solid %2;"
                    " border-radius: 4px; padding: 2px 4px; color: %3; }")
            .arg(VesperTheme::SurfaceRaised.name())
            .arg(VesperTheme::SurfaceBorder.name())
            .arg(VesperTheme::TextPrimary.name()));
        return s;
    };

    // ── Transform ─────────────────────────────────────────────
    auto* grpTransform = new QGroupBox("Transform", content);
    grpTransform->setStyleSheet(
        QString("QGroupBox { color: %1; font-weight: bold; border: 1px solid %2;"
                " border-radius: 6px; margin-top: 6px; padding-top: 6px; }"
                "QGroupBox::title { subcontrol-origin: margin; padding: 0 4px; }")
        .arg(VesperTheme::TextPrimary.name())
        .arg(VesperTheme::SurfaceBorder.name()));
    auto* frmTransform = new QFormLayout(grpTransform);
    frmTransform->setLabelAlignment(Qt::AlignRight);

    m_spnPosX     = makeSpinBox(-4000, 4000, 1, " px");
    m_spnPosY     = makeSpinBox(-4000, 4000, 1, " px");
    m_spnScaleX   = makeSpinBox(0.01, 10, 0.01);
    m_spnScaleY   = makeSpinBox(0.01, 10, 0.01);
    m_spnRotation = makeSpinBox(-360, 360, 0.5, "°");

    frmTransform->addRow("X:", m_spnPosX);
    frmTransform->addRow("Y:", m_spnPosY);
    frmTransform->addRow("Scale X:", m_spnScaleX);
    frmTransform->addRow("Scale Y:", m_spnScaleY);
    frmTransform->addRow("Rotation:", m_spnRotation);

    // Opacity slider
    auto* opacRow = new QHBoxLayout;
    m_sldOpacity  = new QSlider(Qt::Horizontal);
    m_sldOpacity->setRange(0, 100);
    m_sldOpacity->setValue(100);
    m_lblOpacity  = new QLabel("100%");
    m_lblOpacity->setFixedWidth(36);
    opacRow->addWidget(m_sldOpacity);
    opacRow->addWidget(m_lblOpacity);
    frmTransform->addRow("Opacity:", opacRow->parentWidget());

    // ── Crop ──────────────────────────────────────────────────
    auto* grpCrop = new QGroupBox("Crop", content);
    grpCrop->setStyleSheet(grpTransform->styleSheet());
    auto* frmCrop = new QFormLayout(grpCrop);
    frmCrop->setLabelAlignment(Qt::AlignRight);
    m_spnCropL = makeSpinBox(0, 100, 0.5, "%");
    m_spnCropR = makeSpinBox(0, 100, 0.5, "%");
    m_spnCropT = makeSpinBox(0, 100, 0.5, "%");
    m_spnCropB = makeSpinBox(0, 100, 0.5, "%");
    frmCrop->addRow("Left:",   m_spnCropL);
    frmCrop->addRow("Right:",  m_spnCropR);
    frmCrop->addRow("Top:",    m_spnCropT);
    frmCrop->addRow("Bottom:", m_spnCropB);

    root->addWidget(grpTransform);
    root->addWidget(grpCrop);
    root->addStretch();

    scroll->setWidget(content);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->addWidget(scroll);

    // Connections
    auto notify = [this]{ if (!m_loading) onValueChanged(); };
    connect(m_spnPosX,     qOverload<double>(&QDoubleSpinBox::valueChanged), notify);
    connect(m_spnPosY,     qOverload<double>(&QDoubleSpinBox::valueChanged), notify);
    connect(m_spnScaleX,   qOverload<double>(&QDoubleSpinBox::valueChanged), notify);
    connect(m_spnScaleY,   qOverload<double>(&QDoubleSpinBox::valueChanged), notify);
    connect(m_spnRotation, qOverload<double>(&QDoubleSpinBox::valueChanged), notify);
    connect(m_sldOpacity,  &QSlider::valueChanged, [this, notify](int v){
        m_lblOpacity->setText(QString("%1%").arg(v));
        notify();
    });
}

void PropertiesPanel::loadClip(const QString& trackId, const QString& clipId)
{
    m_trackId = trackId;
    m_clipId  = clipId;
    auto* trk = m_timeline->trackById(trackId);
    if (!trk) return;
    auto clip = trk->clipById(clipId);
    if (!clip) return;

    m_loading = true;
    m_spnPosX->setValue(clip->posX);
    m_spnPosY->setValue(clip->posY);
    m_spnScaleX->setValue(clip->scaleX);
    m_spnScaleY->setValue(clip->scaleY);
    m_spnRotation->setValue(clip->rotation);
    m_sldOpacity->setValue(static_cast<int>(clip->opacity * 100));
    m_spnCropL->setValue(clip->cropLeft   * 100);
    m_spnCropR->setValue(clip->cropRight  * 100);
    m_spnCropT->setValue(clip->cropTop    * 100);
    m_spnCropB->setValue(clip->cropBottom * 100);
    m_loading = false;
}

void PropertiesPanel::onValueChanged() { applyToClip(); }

void PropertiesPanel::applyToClip()
{
    auto* trk = m_timeline->trackById(m_trackId);
    if (!trk) return;
    auto clip = trk->clipById(m_clipId);
    if (!clip) return;

    clip->posX      = m_spnPosX->value();
    clip->posY      = m_spnPosY->value();
    clip->scaleX    = m_spnScaleX->value();
    clip->scaleY    = m_spnScaleY->value();
    clip->rotation  = m_spnRotation->value();
    clip->opacity   = m_sldOpacity->value() / 100.0;
    clip->cropLeft  = m_spnCropL->value() / 100.0;
    clip->cropRight = m_spnCropR->value() / 100.0;
    clip->cropTop   = m_spnCropT->value() / 100.0;
    clip->cropBottom= m_spnCropB->value() / 100.0;
    emit m_timeline->clipModified(m_trackId, m_clipId);
    }
