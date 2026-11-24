#pragma once
// OpenGL renderer (3.3 core / GLES 3.0) running entirely offscreen on a
// dedicated thread. Each layer renders into its own framebuffer, layers are
// composited with blend modes and masks, two compositions can be crossfaded
// (program/preview take, scene transitions), and a master pass applies
// intensity, tint, strobe safety limiting and output calibration.
#include "RenderTypes.h"
#include <QHash>
#include <QMutex>
#include <memory>
#include <unordered_map>

class QOpenGLContext;
class QOffscreenSurface;
class QOpenGLShaderProgram;
class QOpenGLFramebufferObject;
class QOpenGLTexture;
class QOpenGLExtraFunctions;

namespace lf {

class GpuRenderer {
public:
    GpuRenderer();
    ~GpuRenderer();
    bool initialize(QString* err = nullptr);   // must be called on the render thread
    bool isInitialized() const { return m_ctx != nullptr; }
    RenderOutput render(const RenderInput& in);
    // Compiles an effect in an isolated scratch program (does not touch the cache).
    bool validateEffect(const QString& glsl, QString* log);
    void invalidateProgram(const QString& key);
    void releaseResources();
    RenderStats lastStats() const { return m_stats; }
    QString glInfo() const;
    bool contextLost() const;
private:
    struct LayerTargets { std::unique_ptr<QOpenGLFramebufferObject> fbo[2]; int cur = 0; std::unique_ptr<QOpenGLTexture> tex; qint64 texKey = -1; };
    struct CachedProgram { std::unique_ptr<QOpenGLShaderProgram> prog; quint64 hash = 0; QString error; QHash<QByteArray, int> loc; };
    CachedProgram* program(const LayerSnapshot& l);
    QOpenGLShaderProgram* buildProgram(const QString& fragmentBody, QString* log);
    void renderComposition(const CompositionSnapshot& c, const RenderInput& in, const QString& slot, QOpenGLFramebufferObject* target, int w, int h);
    void drawQuad();
    QOpenGLFramebufferObject* fbo(const QString& name, int w, int h);
    QString versionHeader() const;
    void setCommonUniforms(CachedProgram* p, const LayerSnapshot& l, const RenderInput& in, int w, int h);
    int loc(CachedProgram* p, const char* name);

    QOpenGLContext* m_ctx = nullptr;
    QOffscreenSurface* m_surface = nullptr;
    QOpenGLExtraFunctions* f = nullptr;
    unsigned m_vao = 0, m_vbo = 0;
    std::unordered_map<QString, CachedProgram> m_programs;
    std::unique_ptr<QOpenGLShaderProgram> m_composite, m_master, m_blit;
    std::unordered_map<QString, std::unique_ptr<QOpenGLFramebufferObject>> m_fbos;
    std::unordered_map<QString, LayerTargets> m_layers;
    std::unique_ptr<QOpenGLTexture> m_blank;
    RenderStats m_stats;
    bool m_gles = false;
    double m_lastFrameT = -1;
    qint64 m_lastWall = 0;
};

} // namespace lf
