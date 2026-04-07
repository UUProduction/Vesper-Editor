// ============================================================
//  Renderer.h — OpenGL ES 3.0 Frame Compositor
// ============================================================
#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_0>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QVector>
#include <memory>
#include "Timeline.h"

// ── Render Layer (one per video clip at current time) ─────────
struct RenderLayer {
    std::shared_ptr<Clip> clip;
    QImage                frame;
    GLuint                textureId = 0;
};

// ── Transition Types (mapped to GLSL shaders) ─────────────────
enum class TransitionType {
    Cut, Dissolve, FadeBlack, FadeWhite,
    SlideLeft, SlideRight, SlideUp, SlideDown,
    ZoomIn, ZoomOut,
    Glitch, LightLeak, FilmBurn, Spin, WhipPan,
    CubeRotate, PageFlip, Warp
};

// ── Renderer ─────────────────────────────────────────────────
class Renderer : public QOpenGLWidget,
                 protected QOpenGLFunctions_3_0
{
    Q_OBJECT
public:
    explicit Renderer(QWidget* parent = nullptr);
    ~Renderer() override;

    void setTimeline(Timeline* timeline);
    void setCurrentTime(double seconds);
    void setCanvasAspect(float aspect);   // 16/9, 9/16, 1.0, etc.
    void setBackgroundColor(const QColor& c);

    // Render a single frame to QImage (for export pipeline)
    QImage renderToImage(double timeSeconds, int width, int height);

    // Upload a new frame for a given clip id
    void updateClipFrame(const QString& clipId, const QImage& frame);

    QSize canvasSize() const;

signals:
    void frameRendered();

protected:
    void initializeGL()  override;
    void resizeGL(int w, int h) override;
    void paintGL()       override;

private:
    // ── Shaders ───────────────────────────────────────────────
    void buildShaders();
    void buildTransitionShaders();
    const char* transitionFragSrc(TransitionType t);

    // ── Geometry ──────────────────────────────────────────────
    void buildQuad();
    void drawQuad(QOpenGLShaderProgram& prog, GLuint texA,
                  GLuint texB = 0, float progress = 0.0f);

    // ── Texture helpers ──────────────────────────────────────
    GLuint uploadTexture(const QImage& img);
    void   deleteTexture(GLuint id);

    // ── Compositing ──────────────────────────────────────────
    void compositeLayers(const QVector<RenderLayer>& layers, double t);

    // ── Members ──────────────────────────────────────────────
    Timeline*                          m_timeline   = nullptr;
    double                             m_currentTime= 0.0;
    float                              m_aspect     = 16.0f / 9.0f;
    QColor                             m_bgColor    { "#0A0A0F" };

    QOpenGLShaderProgram               m_baseShader;
    QOpenGLShaderProgram               m_transShader;
    QMap<TransitionType,
         QOpenGLShaderProgram*>        m_transShaders;

    GLuint                             m_vao = 0;
    GLuint                             m_vbo = 0;

    QMap<QString, GLuint>              m_textures;   // clipId → GL texture
    GLuint                             m_blackTex = 0;
    GLuint                             m_whiteTex = 0;

    bool                               m_initialized = false;
};
