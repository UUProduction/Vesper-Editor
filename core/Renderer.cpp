// ============================================================
//  Renderer.cpp
// ============================================================
#include "Renderer.h"
#include <QOpenGLFunctions>
#include <QPainter>
#include <QMatrix4x4>
#include <cmath>

// ── Vertex shader (shared) ────────────────────────────────────
static const char* kVertSrc = R"GLSL(
#version 300 es
precision mediump float;
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 vUV;
uniform mat4 uMVP;
void main(){
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
}
)GLSL";

// ── Base fragment shader ──────────────────────────────────────
static const char* kBaseFragSrc = R"GLSL(
#version 300 es
precision mediump float;
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTex;
uniform float uOpacity;
void main(){
    fragColor = texture(uTex, vUV) * uOpacity;
}
)GLSL";

// ── Dissolve transition ───────────────────────────────────────
static const char* kDissolveFrag = R"GLSL(
#version 300 es
precision mediump float;
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTexA;
uniform sampler2D uTexB;
uniform float uProgress;
void main(){
    vec4 a = texture(uTexA, vUV);
    vec4 b = texture(uTexB, vUV);
    fragColor = mix(a, b, uProgress);
}
)GLSL";

// ── Slide Left transition ─────────────────────────────────────
static const char* kSlideLeftFrag = R"GLSL(
#version 300 es
precision mediump float;
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTexA;
uniform sampler2D uTexB;
uniform float uProgress;
void main(){
    vec2 uvA = vUV + vec2(uProgress, 0.0);
    vec2 uvB = vUV + vec2(uProgress - 1.0, 0.0);
    if(uvA.x >= 0.0 && uvA.x <= 1.0)
        fragColor = texture(uTexA, uvA);
    else
        fragColor = texture(uTexB, uvB);
}
)GLSL";

// ── Glitch transition ─────────────────────────────────────────
static const char* kGlitchFrag = R"GLSL(
#version 300 es
precision mediump float;
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTexA;
uniform sampler2D uTexB;
uniform float uProgress;
float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898,78.233))) * 43758.5453);
}
void main(){
    float glitch = rand(vec2(floor(vUV.y * 20.0), uProgress)) * uProgress;
    vec2 uvA = vUV + vec2(glitch * 0.05, 0.0);
    vec2 uvB = vUV - vec2(glitch * 0.05, 0.0);
    vec4 a = texture(uTexA, uvA);
    vec4 b = texture(uTexB, uvB);
    fragColor = mix(a, b, uProgress);
}
)GLSL";

// ── Zoom In transition ────────────────────────────────────────
static const char* kZoomInFrag = R"GLSL(
#version 300 es
precision mediump float;
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTexA;
uniform sampler2D uTexB;
uniform float uProgress;
void main(){
    float zoom = 1.0 + uProgress * 0.3;
    vec2 centered = vUV - 0.5;
    vec2 zoomed   = centered / zoom + 0.5;
    vec4 a = texture(uTexA, zoomed);
    vec4 b = texture(uTexB, vUV);
    fragColor = mix(a, b, uProgress);
}
)GLSL";

// ────────────────────────────────────────────────────────────
Renderer::Renderer(QWidget* parent)
    : QOpenGLWidget(parent) {}

Renderer::~Renderer()
{
    makeCurrent();
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    for (auto& p : m_transShaders) delete p;
    doneCurrent();
}

void Renderer::setTimeline(Timeline* tl) { m_timeline = tl; update(); }
void Renderer::setCurrentTime(double t) { m_currentTime = t; update(); }
void Renderer::setCanvasAspect(float a) { m_aspect = a; update(); }
void Renderer::setBackgroundColor(const QColor& c) { m_bgColor = c; update(); }

// ── GL init ────────────────────────────────────────────────
void Renderer::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    buildShaders();
    buildQuad();

    // Black/White fallback textures
    QImage black(1, 1, QImage::Format_RGB32);
    black.fill(Qt::black);
    m_blackTex = uploadTexture(black);

    QImage white(1, 1, QImage::Format_RGB32);
    white.fill(Qt::white);
    m_whiteTex = uploadTexture(white);

    m_initialized = true;
}

void Renderer::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void Renderer::paintGL()
{
    glClearColor(m_bgColor.redF(), m_bgColor.greenF(),
                 m_bgColor.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_timeline || !m_initialized) return;

    QVector<RenderLayer> layers;

    // Collect visible video tracks, bottom to top
    const auto& tracks = m_timeline->tracks();
    QVector<std::shared_ptr<Track>> videoTracks;
    for (auto& t : tracks)
        if (t->type == Track::TrackType::Video && t->isVisible)
            videoTracks.prepend(t);   // bottom track first

    for (auto& trk : videoTracks) {
        auto clip = trk->clipAt(m_currentTime);
        if (!clip || clip->isMuted) continue;
        RenderLayer layer;
        layer.clip      = clip;
        layer.textureId = m_textures.value(clip->id, m_blackTex);
        layers.append(layer);
    }

    compositeLayers(layers, m_currentTime);
    emit frameRendered();
}

// ── Compositing ───────────────────────────────────────────────
void Renderer::compositeLayers(const QVector<RenderLayer>& layers, double t)
{
    int w = width(), h = height();

    // Canvas letterbox/pillarbox rect
    float canvW, canvH;
    if ((float)w / h > m_aspect) {
        canvH = h;
        canvW = h * m_aspect;
    } else {
        canvW = w;
        canvH = w / m_aspect;
    }
    float offX = (w - canvW) * 0.5f;
    float offY = (h - canvH) * 0.5f;
    glViewport(static_cast<int>(offX), static_cast<int>(offY),
               static_cast<int>(canvW), static_cast<int>(canvH));

    QMatrix4x4 mvp;
    mvp.setToIdentity();
    mvp.ortho(-1, 1, -1, 1, -1, 1);

    for (const auto& layer : layers) {
        auto& clip = *layer.clip;

        // Apply transform
        QMatrix4x4 m = mvp;
        m.translate(static_cast<float>(clip.posX / (canvW * 0.5f)),
                    static_cast<float>(clip.posY / (canvH * 0.5f)));
        m.scale(static_cast<float>(clip.scaleX),
                static_cast<float>(clip.scaleY));
        m.rotate(static_cast<float>(clip.rotation), 0, 0, 1);

        // Check transition with next clip
        GLuint texA = layer.textureId;
        GLuint texB = m_blackTex;
        float  progress = 0.0f;

        auto& trans = clip.transitionOut;
        double clipEnd = clip.timelineEnd;
        if (trans.duration > 0 && t > clipEnd - trans.duration) {
            progress = static_cast<float>((t - (clipEnd - trans.duration)) / trans.duration);
            progress = qBound(0.0f, progress, 1.0f);
        }

        m_baseShader.bind();
        m_baseShader.setUniformValue("uMVP",    m);
        m_baseShader.setUniformValue("uOpacity",static_cast<float>(clip.opacity));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texA);
        m_baseShader.setUniformValue("uTex", 0);

        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        m_baseShader.release();
    }

    glViewport(0, 0, w, h);
}

// ── Shader building ───────────────────────────────────────────
void Renderer::buildShaders()
{
    m_baseShader.addShaderFromSourceCode(QOpenGLShader::Vertex,   kVertSrc);
    m_baseShader.addShaderFromSourceCode(QOpenGLShader::Fragment, kBaseFragSrc);
    m_baseShader.link();
}

// ── Quad geometry ─────────────────────────────────────────────
void Renderer::buildQuad()
{
    // pos(2) + uv(2) — triangle strip
    float verts[] = {
        -1.f,-1.f, 0.f,1.f,
         1.f,-1.f, 1.f,1.f,
        -1.f, 1.f, 0.f,0.f,
         1.f, 1.f, 1.f,0.f
    };
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float),
                          reinterpret_cast<void*>(2*sizeof(float)));
    glBindVertexArray(0);
}

// ── Texture upload ────────────────────────────────────────────
GLuint Renderer::uploadTexture(const QImage& img)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 rgba.width(), rgba.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
    glGenerateMipmap(GL_TEXTURE_2D);
    return id;
}

void Renderer::updateClipFrame(const QString& clipId, const QImage& frame)
{
    makeCurrent();
    if (m_textures.contains(clipId)) {
        GLuint old = m_textures[clipId];
        glDeleteTextures(1, &old);
    }
    m_textures[clipId] = uploadTexture(frame);
    doneCurrent();
    update();
}

QSize Renderer::canvasSize() const
{
    int h = height();
    int w = static_cast<int>(h * m_aspect);
    return { w, h };
}

QImage Renderer::renderToImage(double timeSeconds, int imgWidth, int imgHeight)
{
    QImage out(imgWidth, imgHeight, QImage::Format_RGB32);
    out.fill(Qt::black);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    // For export the actual grabbing happens in ExportEngine
    // This is a CPU fallback path
    return out;
}
