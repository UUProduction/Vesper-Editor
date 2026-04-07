// ============================================================
//  TextEditor.h
// ============================================================
#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include "core/Timeline.h"

class TextEditor : public QWidget
{
    Q_OBJECT
public:
    explicit TextEditor(Timeline* tl, QWidget* parent = nullptr);
    void loadClip(const QString& clipId);

private slots:
    void onAddTextClip();
    void onApply();
    void onColorPick();

private:
    void buildUI();
    void applyToClip();

    Timeline*   m_timeline;
    QString     m_clipId;
    bool        m_loading = false;

    QTextEdit*  m_textInput;
    QComboBox*  m_fontFamily;
    QSpinBox*   m_fontSize;
    QPushButton* m_btnBold;
    QPushButton* m_btnItalic;
    QPushButton* m_btnUnder;
    QPushButton* m_btnColorPick;
    QLabel*      m_colorPreview;
    QComboBox*   m_bgMode;
    QComboBox*   m_alignBox;
    QComboBox*   m_animBox;
    QSlider*     m_opacitySlider;
    QString      m_textColor = "#FFFFFF";
};
// ============================================================
//  TextEditor.cpp
// ============================================================
#include "TextEditor.h"
#include "assets/VesperTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QColorDialog>
#include <QFontDatabase>

TextEditor::TextEditor(Timeline* tl, QWidget* parent)
    : QWidget(parent), m_timeline(tl)
{
    setWindowTitle("Text / Title");
    setMinimumSize(300, 500);
    setStyleSheet(QString("background: %1; color: %2;")
        .arg(VesperTheme::Surface.name())
        .arg(VesperTheme::TextPrimary.name()));
    buildUI();
}

void TextEditor::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto* title = new QLabel("Title / Text", this);
    title->setStyleSheet(QString("font-size: %1px; font-weight: bold;")
        .arg(VesperTheme::FontSizeLG));
    root->addWidget(title);

    // Text input
    m_textInput = new QTextEdit(this);
    m_textInput->setPlaceholderText("Type your text here...");
    m_textInput->setMaximumHeight(80);
    m_textInput->setStyleSheet(
        QString("QTextEdit { background: %1; border: 1px solid %2;"
                " border-radius: 6px; color: %3; padding: 4px; }")
        .arg(VesperTheme::SurfaceRaised.name())
        .arg(VesperTheme::SurfaceBorder.name())
        .arg(VesperTheme::TextPrimary.name()));
    root->addWidget(m_textInput);

    // Font controls
    auto* frmFont = new QFormLayout;
    frmFont->setLabelAlignment(Qt::AlignRight);

    m_fontFamily = new QComboBox(this);
    QStringList families = {"Inter","Helvetica","Georgia","Courier New",
                             "Impact","Times New Roman","Arial Black",
                             "Comic Sans MS","Trebuchet MS","Verdana"};
    m_fontFamily->addItems(families);
    m_fontFamily->setStyleSheet(
        QString("QComboBox { background: %1; border: 1px solid %2;"
                " border-radius: 4px; padding: 2px 4px; color: %3; }")
        .arg(VesperTheme::SurfaceRaised.name())
        .arg(VesperTheme::SurfaceBorder.name())
        .arg(VesperTheme::TextPrimary.name()));

    m_fontSize = new QSpinBox(this);
    m_fontSize->setRange(8, 300);
    m_fontSize->setValue(48);
    m_fontSize->setSuffix(" pt");
    m_fontSize->setStyleSheet(m_fontFamily->styleSheet());

    frmFont->addRow("Font:", m_fontFamily);
    frmFont->addRow("Size:", m_fontSize);
    root->addLayout(frmFont);

    // Style buttons row
    auto* styleRow = new QHBoxLayout;
    auto makeToggle = [&](const QString& lbl, const QString& tip) {
        auto* b = new QPushButton(lbl, this);
        b->setToolTip(tip);
        b->setCheckable(true);
        b->setFixedSize(36, 36);
        b->setStyleSheet(
            QString("QPushButton { background: %1; border: 1px solid %2;"
                    " border-radius: 4px; color: %3; font-weight: bold; }"
                    "QPushButton:checked { background: %4; color: white; }")
            .arg(VesperTheme::SurfaceRaised.name())
            .arg(VesperTheme::SurfaceBorder.name())
            .arg(VesperTheme::TextPrimary.name())
            .arg(VesperTheme::Accent.name()));
        return b;
    };
    m_btnBold   = makeToggle("B", "Bold");
    m_btnItalic = makeToggle("I", "Italic");
    m_btnUnder  = makeToggle("U", "Underline");
    styleRow->addWidget(m_btnBold);
    styleRow->addWidget(m_btnItalic);
    styleRow->addWidget(m_btnUnder);
    styleRow->addStretch();

    // Color picker
    m_colorPreview = new QLabel(this);
    m_colorPreview->setFixedSize(28, 28);
    m_colorPreview->setStyleSheet(
        QString("background: white; border: 2px solid %1; border-radius: 4px;")
        .arg(VesperTheme::SurfaceBorder.name()));
    m_btnColorPick = new QPushButton("Color", this);
    m_btnColorPick->setStyleSheet(VesperTheme::iconButtonStyle());
    styleRow->addWidget(m_colorPreview);
    styleRow->addWidget(m_btnColorPick);
    root->addLayout(styleRow);

    // Alignment + background
    auto* frm2 = new QFormLayout;
    frm2->setLabelAlignment(Qt::AlignRight);

    m_alignBox = new QComboBox(this);
    m_alignBox->addItems({"Left", "Center", "Right"});
    m_alignBox->setCurrentIndex(1);
    m_alignBox->setStyleSheet(m_fontFamily->styleSheet());

    m_bgMode = new QComboBox(this);
    m_bgMode->addItems({"None", "Solid", "Gradient", "Blur Box"});
    m_bgMode->setStyleSheet(m_fontFamily->styleSheet());

    m_animBox = new QComboBox(this);
    m_animBox->addItems({"None","Fade In","Slide Up","Slide Down",
                          "Typewriter","Pop","Bounce","Glitch"});
    m_animBox->setStyleSheet(m_fontFamily->styleSheet());

    frm2->addRow("Align:", m_alignBox);
    frm2->addRow("Background:", m_bgMode);
    frm2->addRow("Animation:", m_animBox);
    root->addLayout(frm2);

    // Opacity
    auto* opacRow = new QHBoxLayout;
    auto* opacLbl = new QLabel("Opacity:", this);
    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    opacRow->addWidget(opacLbl);
    opacRow->addWidget(m_opacitySlider);
    root->addLayout(opacRow);

    root->addStretch();

    // Buttons
    auto* btnRow = new QHBoxLayout;
    auto* btnAdd   = new QPushButton("Add to Timeline", this);
    auto* btnApply = new QPushButton("Apply", this);
    btnAdd->setStyleSheet(VesperTheme::accentButtonStyle());
    btnApply->setStyleSheet(VesperTheme::iconButtonStyle());
    btnRow->addWidget(btnApply);
    btnRow->addWidget(btnAdd);
    root->addLayout(btnRow);

    connect(m_btnColorPick, &QPushButton::clicked, this, &TextEditor::onColorPick);
    connect(btnAdd,         &QPushButton::clicked, this, &TextEditor::onAddTextClip);
    connect(btnApply,       &QPushButton::clicked, this, &TextEditor::onApply);
}

void TextEditor::loadClip(const QString& clipId)
{
    m_clipId = clipId;
    for (auto& trk : m_timeline->tracks()) {
        auto clip = trk->clipById(clipId);
        if (!clip) continue;
        m_loading = true;
        m_textInput->setPlainText(clip->textContent);
        m_fontFamily->setCurrentText(clip->fontFamily);
        m_fontSize->setValue(clip->fontSize);
        m_btnBold->setChecked(clip->fontBold);
        m_btnItalic->setChecked(clip->fontItalic);
        m_btnUnder->setChecked(clip->fontUnder);
        m_textColor = clip->textColor;
        m_colorPreview->setStyleSheet(
            QString("background: %1; border: 2px solid %2; border-radius: 4px;")
            .arg(m_textColor).arg(VesperTheme::SurfaceBorder.name()));
        m_alignBox->setCurrentIndex(clip->textAlign);
        m_opacitySlider->setValue(static_cast<int>(clip->opacity * 100));
        m_loading = false;
        return;
    }
}

void TextEditor::onColorPick()
{
    QColor c = QColorDialog::getColor(QColor(m_textColor), this, "Text Color");
    if (!c.isValid()) return;
    m_textColor = c.name();
    m_colorPreview->setStyleSheet(
        QString("background: %1; border: 2px solid %2; border-radius: 4px;")
        .arg(m_textColor).arg(VesperTheme::SurfaceBorder.name()));
    if (!m_clipId.isEmpty()) applyToClip();
}

void TextEditor::onAddTextClip()
{
    // Find first video track
    Track* trk = nullptr;
    for (auto& t : m_timeline->tracks())
        if (t->type == Track::TrackType::Video && !t->isLocked) { trk = t.get(); break; }
    if (!trk) trk = m_timeline->addVideoTrack("Text");

    double pos  = m_timeline->playheadPosition();
    auto   clip = m_timeline->addClip(trk->id, "", pos, Clip::MediaType::Text);
    if (!clip) return;

    clip->timelineEnd  = pos + 5.0;
    clip->sourceDuration = 5.0;
    clip->sourceOut    = 5.0;
    clip->name         = "Text";
    m_clipId = clip->id;
    applyToClip();
}

void TextEditor::onApply() { applyToClip(); }

void TextEditor::applyToClip()
{
    if (m_clipId.isEmpty()) return;
    for (auto& trk : m_timeline->tracks()) {
        auto clip = trk->clipById(m_clipId);
        if (!clip) continue;
        clip->textContent = m_textInput->toPlainText();
        clip->fontFamily  = m_fontFamily->currentText();
        clip->fontSize    = m_fontSize->value();
        clip->fontBold    = m_btnBold->isChecked();
        clip->fontItalic  = m_btnItalic->isChecked();
        clip->fontUnder   = m_btnUnder->isChecked();
        clip->textColor   = m_textColor;
        clip->textAlign   = m_alignBox->currentIndex();
        clip->textBgMode  = m_bgMode->currentText().toLower();
        clip->textAnim    = m_animBox->currentText().toLower();
        clip->opacity     = m_opacitySlider->value() / 100.0;
        emit m_timeline->clipModified(trk->id, clip->id);
        return;
    }
}
