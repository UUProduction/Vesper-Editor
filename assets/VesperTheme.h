// ============================================================
//  VesperTheme.h — Design Tokens
// ============================================================
#pragma once

#include <QColor>
#include <QString>

namespace VesperTheme
{
    // ── Colors ────────────────────────────────────────────────
    inline const QColor Background     { "#0A0A0F" };
    inline const QColor Surface        { "#12121A" };
    inline const QColor SurfaceRaised  { "#1C1C28" };
    inline const QColor SurfaceBorder  { "#2A2A3A" };
    inline const QColor Accent         { "#7C6AF7" };
    inline const QColor AccentHover    { "#9B8BFF" };
    inline const QColor AccentGold     { "#F0C060" };
    inline const QColor AccentGoldDim  { "#A08040" };
    inline const QColor TextPrimary    { "#F0F0F8" };
    inline const QColor TextSecondary  { "#8080A0" };
    inline const QColor TextDisabled   { "#404055" };
    inline const QColor TrackVideo     { "#2A4A7F" };
    inline const QColor TrackVideoClip { "#3A6AAF" };
    inline const QColor TrackAudio     { "#7F4A1A" };
    inline const QColor TrackAudioClip { "#AF6A2A" };
    inline const QColor Danger         { "#E05555" };
    inline const QColor DangerHover    { "#FF6666" };
    inline const QColor Success        { "#55C077" };
    inline const QColor PlayheadColor  { "#F0C060" };
    inline const QColor SnapLineColor  { "#F0C060" };
    inline const QColor SelectionColor { "#7C6AF755" };

    // ── Dimensions ────────────────────────────────────────────
    inline constexpr int BorderRadius      = 8;
    inline constexpr int BorderRadiusSmall = 4;
    inline constexpr int TimelineHeight    = 56;   // px per track
    inline constexpr int TrackHeaderWidth  = 72;   // px
    inline constexpr int ToolbarHeight     = 52;   // px
    inline constexpr int TopBarHeight      = 48;   // px
    inline constexpr int PlaybackBarHeight = 48;   // px
    inline constexpr int MediaBrowserHeight= 220;  // px collapsed

    // ── Typography ────────────────────────────────────────────
    inline const QString FontFamily  = "Inter";
    inline constexpr int FontSizeXS  = 9;
    inline constexpr int FontSizeSM  = 11;
    inline constexpr int FontSizeMD  = 13;
    inline constexpr int FontSizeLG  = 16;
    inline constexpr int FontSizeXL  = 20;
    inline constexpr int FontSizeXXL = 28;

    // ── Stylesheet helpers ───────────────────────────────────
    inline QString iconButtonStyle()
    {
        return QString(
            "QPushButton {"
            "  background: transparent;"
            "  border: none;"
            "  border-radius: %1px;"
            "  padding: 6px;"
            "  color: %2;"
            "}"
            "QPushButton:hover {"
            "  background: %3;"
            "}"
            "QPushButton:pressed {"
            "  background: %4;"
            "}"
        ).arg(BorderRadiusSmall)
         .arg(TextSecondary.name())
         .arg(SurfaceRaised.name())
         .arg(Accent.name() + "44");
    }

    inline QString panelStyle()
    {
        return QString(
            "background-color: %1;"
            "border-radius: %2px;"
        ).arg(Surface.name())
         .arg(BorderRadius);
    }

    inline QString accentButtonStyle()
    {
        return QString(
            "QPushButton {"
            "  background: %1;"
            "  border: none;"
            "  border-radius: %2px;"
            "  padding: 8px 16px;"
            "  color: white;"
            "  font-weight: bold;"
            "}"
            "QPushButton:hover { background: %3; }"
            "QPushButton:pressed { background: %4; }"
        ).arg(Accent.name())
         .arg(BorderRadius)
         .arg(AccentHover.name())
         .arg(Accent.darker(120).name());
    }
}
