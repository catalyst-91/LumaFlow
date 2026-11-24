#include "GpuRenderer.h"
#include "effects/EffectLibrary.h"
#include "fixtures/ColorEngine.h"
#include "core/Log.h"
#include "recovery/Recovery.h"
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLTexture>
#include <QOpenGLExtraFunctions>
#include <QElapsedTimer>
#include <QDateTime>
#include <QSurfaceFormat>

namespace lf {

static const char* kCompositeFrag = R"(
uniform sampler2D uLayer; uniform sampler2D uBelow; uniform int uBlend; uniform float uOpacity;
uniform int uMask; uniform float uMaskSize; uniform float uMaskFeather; uniform vec2 uRes;
in vec2 vUV; out vec4 fragColor;
void main() {
    vec4 L = texture(uLayer, vUV); vec4 B = texture(uBelow, vUV);
    float m = 1.0; vec2 c = vUV - 0.5; c.x *= uRes.x / max(uRes.y, 1.0);
    float fe = max(uMaskFeather, 0.001);
    if (uMask == 1) m = 1.0 - smoothstep(uMaskSize * 0.5 - fe * 0.5, uMaskSize * 0.5 + fe * 0.5, length(c));
    else if (uMask == 2) { vec2 d = abs(c) - vec2(uMaskSize * 0.5 * uRes.x / max(uRes.y, 1.0), uMaskSize * 0.5); m = 1.0 - smoothstep(-fe * 0.5, fe * 0.5, max(d.x, d.y)); }
    else if (uMask == 3) m = smoothstep(uMaskSize - fe, uMaskSize + fe, vUV.x);
    else if (uMask == 4) m = dot(B.rgb, vec3(0.2126, 0.7152, 0.0722));
    float a = clamp(L.a * uOpacity * m, 0.0, 1.0);
    vec3 r;
    if (uBlend == 1) r = B.rgb + L.rgb * a;
    else if (uBlend == 2) r = mix(B.rgb, B.rgb * L.rgb, a);
    else if (uBlend == 3) r = mix(B.rgb, 1.0 - (1.0 - B.rgb) * (1.0 - L.rgb), a);
    else if (uBlend == 4) r = mix(B.rgb, L.rgb, a * clamp(dot(B.rgb, vec3(0.2126, 0.7152, 0.0722)) * 2.0, 0.0, 1.0));
    else r = mix(B.rgb, L.rgb, a);
    fragColor = vec4(clamp(r, 0.0, 1.0), 1.0);
}
)";

static const char* kMasterFrag = R"(
uniform sampler2D uA; uniform sampler2D uB; uniform float uMix;
uniform float uIntensity; uniform vec3 uTint; uniform float uSat; uniform float uStrobeOn; uniform float uBlackout;
uniform float uGamma; uniform vec3 uGain; uniform float uBright; uniform float uLimit;
in vec2 vUV; out vec4 fragColor;
void main() {
    vec3 c = mix(texture(uA, vUV).rgb, texture(uB, vUV).rgb, uMix);
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(vec3(l), c, uSat) * uTint;
    c *= uIntensity * uStrobeOn * (1.0 - uBlackout);
    c = clamp(c * uGain * uBright, 0.0, 1.0);
    c = pow(c, vec3(1.0 / max(uGamma, 0.05)));
    c = min(c, vec3(uLimit));
    fragColor = vec4(c, 1.0);
}
)";

static const char* kMediaBody = R"(
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 tuv = vec2(p.x / max(uTexAspect, 0.01) + 0.5, 0.5 - p.y);
    float inside = step(0.0, tuv.x) * step(tuv.x, 1.0) * step(0.0, tuv.y) * step(tuv.y, 1.0);
    vec4 c = texture(uTex, tuv) * inside;
    return vec4(c.rgb, c.a * inside);
}
)";

GpuRenderer::GpuRenderer() {}
GpuRenderer::~GpuRenderer() { releaseResources(); }

QString GpuRenderer::versionHeader() const {
    return m_gles ? QStringLiteral("#version 300 es\nprecision highp float;\nprecision highp int;\n") : QStringLiteral("#version 330 core\n");
}

bool GpuRenderer::initialize(QString* err) {
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(3, 3); fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(0); fmt.setStencilBufferSize(0);
    m_ctx = new QOpenGLContext;
    m_ctx->setFormat(fmt);
    if (!m_ctx->create()) {
        // fall back to OpenGL ES 3.0 (e.g. ANGLE / embedded drivers)
        fmt.setRenderableType(QSurfaceFormat::OpenGLES); fmt.setVersion(3, 0); fmt.setProfile(QSurfaceFormat::NoProfile);
        m_ctx->setFormat(fmt);
        if (!m_ctx->create()) { if (err) *err = "cannot create OpenGL 3.3 core or GLES 3.0 context"; delete m_ctx; m_ctx = nullptr; return false; }
    }
    m_surface = new QOffscreenSurface;
    m_surface->setFormat(m_ctx->format());
    m_surface->create();
    if (!m_ctx->makeCurrent(m_surface)) { if (err) *err = "makeCurrent failed"; return false; }
    m_gles = m_ctx->isOpenGLES();
    f = m_ctx->extraFunctions();
    const QSurfaceFormat got = m_ctx->format();
    if (!m_gles && (got.majorVersion() < 3 || (got.majorVersion() == 3 && got.minorVersion() < 3))) {
        if (err) *err = QString("OpenGL %1.%2 available, 3.3 required").arg(got.majorVersion()).arg(got.minorVersion());
        return false;
    }
    m_stats.glRenderer = QString::fromLatin1(reinterpret_cast<const char*>(f->glGetString(GL_RENDERER)));
    m_stats.glVersion = QString::fromLatin1(reinterpret_cast<const char*>(f->glGetString(GL_VERSION)));
    // fullscreen quad
    const float quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
    f->glGenVertexArrays(1, &m_vao); f->glBindVertexArray(m_vao);
    f->glGenBuffers(1, &m_vbo); f->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    f->glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    f->glEnableVertexAttribArray(0); f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    QString log;
    auto mk = [&](const char* src) {
        auto p = std::make_unique<QOpenGLShaderProgram>();
        p->addShaderFromSourceCode(QOpenGLShader::Vertex, versionHeader() + EffectLibrary::vertexShader());
        p->addShaderFromSourceCode(QOpenGLShader::Fragment, versionHeader() + src);
        p->bindAttributeLocation("aPos", 0);
        if (!p->link()) log += p->log();
        return p;
    };
    m_composite = mk(kCompositeFrag); m_master = mk(kMasterFrag);
    if (!log.isEmpty()) { if (err) *err = "internal shader error: " + log; return false; }
    QImage black(2, 2, QImage::Format_RGBA8888); black.fill(Qt::transparent);
    m_blank = std::make_unique<QOpenGLTexture>(black);
    logInfo("renderer", QString("OpenGL renderer: %1 / %2").arg(m_stats.glRenderer, m_stats.glVersion));
    return true;
}

QString GpuRenderer::glInfo() const { return m_stats.glRenderer + " | " + m_stats.glVersion; }
bool GpuRenderer::contextLost() const { return m_ctx && !m_ctx->isValid(); }

void GpuRenderer::releaseResources() {
    if (!m_ctx) return;
    m_ctx->makeCurrent(m_surface);
    m_programs.clear(); m_composite.reset(); m_master.reset(); m_blit.reset(); m_fbos.clear(); m_layers.clear(); m_blank.reset();
    if (m_vbo) f->glDeleteBuffers(1, &m_vbo);
    if (m_vao) f->glDeleteVertexArrays(1, &m_vao);
    m_vbo = m_vao = 0;
    m_ctx->doneCurrent();
    delete m_ctx; m_ctx = nullptr;
    delete m_surface; m_surface = nullptr;
}

QOpenGLShaderProgram* GpuRenderer::buildProgram(const QString& body, QString* log) {
    auto* p = new QOpenGLShaderProgram;
    const QString fs = versionHeader() + EffectLibrary::prelude() + "\n#line 1 1\n" + body + "\n#line 1 2\n" + EffectLibrary::epilogue();
    bool ok = p->addShaderFromSourceCode(QOpenGLShader::Vertex, versionHeader() + EffectLibrary::vertexShader());
    ok = ok && p->addShaderFromSourceCode(QOpenGLShader::Fragment, fs);
    p->bindAttributeLocation("aPos", 0);
    ok = ok && p->link();
    if (!ok) { if (log) *log = p->log().trimmed(); delete p; return nullptr; }
    return p;
}

bool GpuRenderer::validateEffect(const QString& glsl, QString* log) {
    if (!m_ctx) { if (log) *log = "renderer not initialised"; return false; }
    m_ctx->makeCurrent(m_surface);
    // static checks first: forbid unbounded constructs we cannot recover from on the GPU
    if (glsl.contains("while") && !glsl.contains("break")) { if (log) *log = "unbounded while loop rejected (use bounded for loops)"; return false; }
    std::unique_ptr<QOpenGLShaderProgram> p(buildProgram(glsl, log));
    return p != nullptr;
}

void GpuRenderer::invalidateProgram(const QString& key) { m_programs.erase(key); }

int GpuRenderer::loc(CachedProgram* p, const char* name) {
    auto it = p->loc.find(name);
    if (it != p->loc.end()) return *it;
    const int l = p->prog->uniformLocation(name);
    p->loc.insert(name, l);
    return l;
}

GpuRenderer::CachedProgram* GpuRenderer::program(const LayerSnapshot& l) {
    auto it = m_programs.find(l.programKey);
    const bool needBuild = it == m_programs.end() || (!l.glsl.isEmpty() && it->second.hash != l.glslHash);
    if (!needBuild) return it->second.prog ? &it->second : nullptr;
    const QString body = l.programKey == "__media" ? QString(kMediaBody) : l.glsl;
    if (body.isEmpty()) return nullptr;
    QString log;
    QOpenGLShaderProgram* p = buildProgram(body, &log);
    if (!p) {
        // keep the last working version of this effect running; report the error
        const QString msg = QString("shader '%1' failed to compile: %2").arg(l.programKey, log.left(400));
        if (it == m_programs.end() || it->second.error != msg) FaultRegistry::instance().report(FaultSeverity::Error, "shader", msg, "Fix the shader; the previous version (or black) is shown meanwhile. Other layers are unaffected.");
        if (it == m_programs.end()) { CachedProgram cp; cp.hash = l.glslHash; cp.error = msg; m_programs.emplace(l.programKey, std::move(cp)); return nullptr; }
        it->second.hash = l.glslHash; it->second.error = msg;
        return it->second.prog ? &it->second : nullptr;
    }
    CachedProgram cp; cp.prog.reset(p); cp.hash = l.glslHash;
    FaultRegistry::instance().resolve("shader", QString("shader '%1'").arg(l.programKey));
    m_programs[l.programKey] = std::move(cp);
    return &m_programs[l.programKey];
}

QOpenGLFramebufferObject* GpuRenderer::fbo(const QString& name, int w, int h) {
    auto& slot = m_fbos[name];
    if (!slot || slot->width() != w || slot->height() != h) {
        QOpenGLFramebufferObjectFormat ff; ff.setInternalTextureFormat(GL_RGBA8);
        slot = std::make_unique<QOpenGLFramebufferObject>(w, h, ff);
        f->glBindTexture(GL_TEXTURE_2D, slot->texture());
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    return slot.get();
}

void GpuRenderer::drawQuad() { f->glBindVertexArray(m_vao); f->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); }

void GpuRenderer::setCommonUniforms(CachedProgram* p, const LayerSnapshot& l, const RenderInput& in, int w, int h) {
    auto* g = p->prog.get();
    g->setUniformValue(loc(p, "uTime"), l.time);
    g->setUniformValue(loc(p, "uSpeed"), 1.0f);
    // audio reactivity modulates scale and brightness of the layer
    const float aud = l.audioAmount * (in.audio[0] * 0.6f + in.beat * 0.4f);
    g->setUniformValue(loc(p, "uScale"), l.scale * (1.0f + aud * 0.35f));
    g->setUniformValue(loc(p, "uRotation"), l.rotation);
    g->setUniformValue(loc(p, "uBrightness"), l.brightness * (1.0f + aud * 0.8f));
    g->setUniformValue(loc(p, "uContrast"), l.contrast);
    g->setUniformValue(loc(p, "uSaturation"), l.saturation);
    g->setUniformValue(loc(p, "uHue"), l.hue);
    g->setUniformValue(loc(p, "uHue2"), l.hue2);
    g->setUniformValue(loc(p, "uResolution"), QVector2D(float(w), float(h)));
    g->setUniformValue(loc(p, "uPos"), QVector2D(l.posX * 0.5f, l.posY * 0.5f));
    g->setUniformValue(loc(p, "uP"), QVector4D(l.p[0], l.p[1], l.p[2], l.p[3]));
    g->setUniformValue(loc(p, "uAudio"), QVector4D(in.audio[0], in.audio[1], in.audio[2], in.audio[3]));
    g->setUniformValue(loc(p, "uBeat"), in.beat);
    g->setUniformValueArray(loc(p, "uSpec"), in.spectrum.data(), 32, 1);
    g->setUniformValueArray(loc(p, "uExt"), in.ext.data(), 8, 1);
    g->setUniformValue(loc(p, "uTimecode"), in.timecode);
    // colour B hue shift
    RGB cb{l.colorB[0], l.colorB[1], l.colorB[2]};
    if (l.hue2 != 0) { const QColor c = QColor::fromRgbF(float(cb.r), float(cb.g), float(cb.b)); float hh, ss, vv; c.getHsvF(&hh, &ss, &vv); cb = ColorEngine::fromHsv((hh < 0 ? 0 : hh) + l.hue2, ss, vv); }
    g->setUniformValue(loc(p, "uColorA"), QVector3D(l.colorA[0], l.colorA[1], l.colorA[2]));
    g->setUniformValue(loc(p, "uColorB"), QVector3D(float(cb.r), float(cb.g), float(cb.b)));
    g->setUniformValue(loc(p, "uPrev"), 0);
    g->setUniformValue(loc(p, "uTex"), 1);
}

void GpuRenderer::renderComposition(const CompositionSnapshot& c, const RenderInput& in, const QString& slot, QOpenGLFramebufferObject* target, int w, int h) {
    const int lw = std::max(16, int(w * in.qualityScale)), lh = std::max(16, int(h * in.qualityScale));
    QOpenGLFramebufferObject* acc[2] = {fbo(slot + ":acc0", w, h), fbo(slot + ":acc1", w, h)};
    int cur = 0;
    acc[0]->bind(); f->glViewport(0, 0, w, h); f->glClearColor(0, 0, 0, 1); f->glClear(GL_COLOR_BUFFER_BIT);
    for (int i = 0; i < c.layers.size(); ++i) {
        const LayerSnapshot& l = c.layers[i];
        if (!l.visible || l.opacity <= 0.0005f) continue;
        CachedProgram* p = program(l);
        if (!p) continue;                                   // compile error: skip only this layer
        LayerTargets& lt = m_layers[slot + QString::number(i)];
        for (auto& b : lt.fbo) if (!b || b->width() != lw || b->height() != lh) {
            QOpenGLFramebufferObjectFormat ff; ff.setInternalTextureFormat(GL_RGBA8);
            b = std::make_unique<QOpenGLFramebufferObject>(lw, lh, ff);
            f->glBindTexture(GL_TEXTURE_2D, b->texture());
            f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            b->bind(); f->glClearColor(0, 0, 0, 0); f->glClear(GL_COLOR_BUFFER_BIT);
        }
        // media / text texture
        if (!l.texture.isNull() && l.texture.cacheKey() != lt.texKey) {
            lt.tex = std::make_unique<QOpenGLTexture>(l.texture.convertToFormat(QImage::Format_RGBA8888));
            lt.tex->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
            lt.tex->setWrapMode(QOpenGLTexture::ClampToEdge);
            lt.texKey = l.texture.cacheKey();
        }
        const int prev = lt.cur, next = 1 - lt.cur;
        lt.fbo[next]->bind();
        f->glViewport(0, 0, lw, lh);
        f->glClearColor(0, 0, 0, 0); f->glClear(GL_COLOR_BUFFER_BIT);
        p->prog->bind();
        setCommonUniforms(p, l, in, lw, lh);
        const float aspect = l.texture.isNull() ? 1.0f : float(l.texture.width()) / std::max(1, l.texture.height());
        p->prog->setUniformValue(loc(p, "uTexAspect"), aspect);
        f->glActiveTexture(GL_TEXTURE0); f->glBindTexture(GL_TEXTURE_2D, lt.fbo[prev]->texture());
        f->glActiveTexture(GL_TEXTURE1); f->glBindTexture(GL_TEXTURE_2D, lt.tex && !l.texture.isNull() ? lt.tex->textureId() : m_blank->textureId());
        drawQuad();
        lt.cur = next;
        // composite into accumulator
        acc[1 - cur]->bind(); f->glViewport(0, 0, w, h);
        m_composite->bind();
        m_composite->setUniformValue("uLayer", 0); m_composite->setUniformValue("uBelow", 1);
        m_composite->setUniformValue("uBlend", int(l.blend)); m_composite->setUniformValue("uOpacity", l.opacity);
        m_composite->setUniformValue("uMask", int(l.mask)); m_composite->setUniformValue("uMaskSize", l.maskSize); m_composite->setUniformValue("uMaskFeather", l.maskFeather);
        m_composite->setUniformValue("uRes", QVector2D(float(w), float(h)));
        f->glActiveTexture(GL_TEXTURE0); f->glBindTexture(GL_TEXTURE_2D, lt.fbo[next]->texture());
        f->glActiveTexture(GL_TEXTURE1); f->glBindTexture(GL_TEXTURE_2D, acc[cur]->texture());
        drawQuad();
        cur = 1 - cur;
    }
    // copy final accumulator into the target via composite with opacity 1 over black
    QOpenGLFramebufferObject::blitFramebuffer(target, acc[cur]);
}

static QImage readback(QOpenGLExtraFunctions* f, QOpenGLFramebufferObject* fb) {
    QImage img(fb->width(), fb->height(), QImage::Format_RGBA8888);
    fb->bind();
    f->glPixelStorei(GL_PACK_ALIGNMENT, 4);
    f->glReadPixels(0, 0, fb->width(), fb->height(), GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
    return img.mirrored();
}

RenderOutput GpuRenderer::render(const RenderInput& in) {
    RenderOutput out;
    if (!m_ctx) return out;
    QElapsedTimer t; t.start();
    if (!m_ctx->makeCurrent(m_surface)) { FaultRegistry::instance().report(FaultSeverity::Critical, "renderer", "OpenGL context lost", "The renderer will attempt to re-create the context."); return out; }
    const int w = std::max(16, in.width), h = std::max(16, in.height);
    f->glDisable(GL_BLEND); f->glDisable(GL_DEPTH_TEST);
    QOpenGLFramebufferObject* prog = fbo("program", w, h);
    renderComposition(in.program, in, "P", prog, w, h);
    QOpenGLFramebufferObject* outgoing = prog;
    if (in.transition < 0.999f) { outgoing = fbo("outgoing", w, h); renderComposition(in.outgoing, in, "O", outgoing, w, h); }
    // master pass
    QOpenGLFramebufferObject* fin = fbo("final", w, h);
    fin->bind(); f->glViewport(0, 0, w, h);
    m_master->bind();
    m_master->setUniformValue("uA", 0); m_master->setUniformValue("uB", 1);
    m_master->setUniformValue("uMix", std::clamp(in.transition, 0.0f, 1.0f));
    m_master->setUniformValue("uIntensity", in.master.intensity);
    m_master->setUniformValue("uTint", QVector3D(in.master.tint[0], in.master.tint[1], in.master.tint[2]));
    m_master->setUniformValue("uSat", in.master.saturation);
    float strobeOn = 1.0f;
    if (in.master.strobe > 0.01f) {
        const double maxHz = in.safety.limiterEnabled ? in.safety.maxStrobeHz : 12.0;
        const double hz = 0.5 + in.master.strobe * (maxHz - 0.5);
        strobeOn = std::fmod(in.master.time * hz, 1.0) < 0.5 ? 1.0f : 0.0f;
    }
    m_master->setUniformValue("uStrobeOn", strobeOn);
    m_master->setUniformValue("uBlackout", in.master.blackout ? 1.0f : 0.0f);
    const RGB wp = ColorEngine::fromKelvin(in.calibration.whitePointK), ref = ColorEngine::fromKelvin(6500);
    m_master->setUniformValue("uGain", QVector3D(float(in.calibration.gainR * wp.r / std::max(0.01, ref.r)), float(in.calibration.gainG * wp.g / std::max(0.01, ref.g)), float(in.calibration.gainB * wp.b / std::max(0.01, ref.b))));
    m_master->setUniformValue("uGamma", float(in.calibration.gamma));
    m_master->setUniformValue("uBright", float(in.calibration.brightness));
    m_master->setUniformValue("uLimit", float(in.safety.maxBrightness));
    f->glActiveTexture(GL_TEXTURE0); f->glBindTexture(GL_TEXTURE_2D, outgoing->texture());
    f->glActiveTexture(GL_TEXTURE1); f->glBindTexture(GL_TEXTURE_2D, prog->texture());
    drawQuad();
    const double cpuMs = t.nsecsElapsed() / 1e6;
    QElapsedTimer rb; rb.start();
    out.program = readback(f, fin);
    out.stats.readbackMs = rb.nsecsElapsed() / 1e6;
    if (in.renderPreview) {
        const int pw = std::max(16, w / 2), ph = std::max(16, h / 2);
        QOpenGLFramebufferObject* pv = fbo("preview", pw, ph);
        renderComposition(in.preview, in, "V", pv, pw, ph);
        out.preview = readback(f, pv);
    }
    QOpenGLFramebufferObject::bindDefault();
    // statistics
    GLint mem = -1;
    if (m_ctx->hasExtension("GL_NVX_gpu_memory_info")) { f->glGetIntegerv(0x9049, &mem); }   // CURRENT_AVAILABLE_VIDMEM_NVX
    else if (m_ctx->hasExtension("GL_ATI_meminfo")) { GLint v[4] = {-1}; f->glGetIntegerv(0x87FC, v); mem = v[0]; }
    qint64 texKB = 0;
    for (auto& kv : m_fbos) texKB += qint64(kv.second->width()) * kv.second->height() * 4 / 1024;
    for (auto& kv : m_layers) for (auto& b : kv.second.fbo) if (b) texKB += qint64(b->width()) * b->height() * 4 / 1024;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double frameMs = t.nsecsElapsed() / 1e6;
    m_stats.frameMs = frameMs; m_stats.cpuMs = cpuMs; m_stats.gpuMs = frameMs - cpuMs; m_stats.readbackMs = out.stats.readbackMs;
    if (m_lastWall > 0) { const double inst = 1000.0 / std::max<qint64>(1, now - m_lastWall); m_stats.fps = m_stats.fps > 0 ? m_stats.fps * 0.9 + inst * 0.1 : inst; }
    m_lastWall = now;
    ++m_stats.frames; m_stats.gpuMemoryKB = mem; m_stats.textureMemoryKB = texKB; m_stats.qualityScale = in.qualityScale;
    m_stats.programsCached = int(m_programs.size());
    m_stats.shaderErrors.clear();
    for (auto& kv : m_programs) if (!kv.second.error.isEmpty()) m_stats.shaderErrors << kv.second.error;
    out.stats = m_stats;
    out.time = in.master.time;
    return out;
}

} // namespace lf
