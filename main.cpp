// ============================================================
//  Vesper Editor — Entry Point
//  "Edit Like a Pro. Feel Like Magic."
// ============================================================

#include <QApplication>
#include <QSurfaceFormat>
#include <QScreen>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>

#include "ui/MainWindow.h"
#include "assets/VesperTheme.h"

// ── Apply global dark theme palette ──────────────────────────
static void applyVesperPalette(QApplication& app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette p;
    p.setColor(QPalette::Window,          VesperTheme::Background);
    p.setColor(QPalette::WindowText,      VesperTheme::TextPrimary);
    p.setColor(QPalette::Base,            VesperTheme::Surface);
    p.setColor(QPalette::AlternateBase,   VesperTheme::SurfaceRaised);
    p.setColor(QPalette::ToolTipBase,     VesperTheme::SurfaceRaised);
    p.setColor(QPalette::ToolTipText,     VesperTheme::TextPrimary);
    p.setColor(QPalette::Text,            VesperTheme::TextPrimary);
    p.setColor(QPalette::Button,          VesperTheme::SurfaceRaised);
    p.setColor(QPalette::ButtonText,      VesperTheme::TextPrimary);
    p.setColor(QPalette::BrightText,      Qt::white);
    p.setColor(QPalette::Highlight,       VesperTheme::Accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link,            VesperTheme::Accent);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, VesperTheme::TextSecondary);
    p.setColor(QPalette::Disabled, QPalette::WindowText, VesperTheme::TextSecondary);
    p.setColor(QPalette::Disabled, QPalette::Text,       VesperTheme::TextSecondary);
    app.setPalette(p);
}

// ── Configure OpenGL surface ──────────────────────────────────
static void configureOpenGL()
{
    QSurfaceFormat fmt;
    fmt.setVersion(3, 0);                          // OpenGL ES 3.0
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);                             // 4x MSAA
    fmt.setSwapInterval(1);                        // VSync
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(fmt);
}

int main(int argc, char* argv[])
{
    // High-DPI support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName("Vesper Editor");
    app.setApplicationDisplayName("Vesper Editor");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Vesper Studio");
    app.setOrganizationDomain("vespereditor.app");

    configureOpenGL();
    applyVesperPalette(app);

    // Load bundled fonts
    QFontDatabase::addApplicationFont(":/assets/fonts/Inter-Regular.ttf");
    QFontDatabase::addApplicationFont(":/assets/fonts/Inter-Bold.ttf");
    QFontDatabase::addApplicationFont(":/assets/fonts/Inter-Medium.ttf");

    QFont appFont("Inter", 11);
    appFont.setHintingPreference(QFont::PreferFullHinting);
    app.setFont(appFont);

    MainWindow window;

    // Maximize on desktop; fullscreen on mobile
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    window.showFullScreen();
#else
    window.resize(1280, 800);
    window.show();
#endif

    return app.exec();
}
