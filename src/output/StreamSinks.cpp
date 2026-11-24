#include "StreamSinks.h"
#include "plugins/PluginHost.h"
#include "core/Log.h"
#include "recovery/Recovery.h"
#include <QLibrary>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutexLocker>
#include <cstdint>
#include <climits>
#include <chrono>
#include <vector>
#include <cmath>
#ifdef LF_HAVE_SPOUT
#include "SpoutDX.h"
#endif

namespace lf {

QJsonObject StreamConfig::toJson() const { return {{"type", type}, {"name", name}, {"enabled", enabled}, {"region", region}, {"groups", groups}}; }
StreamConfig StreamConfig::fromJson(const QJsonObject& o) {
    StreamConfig c; c.type = o["type"].toString("ndi"); c.name = o["name"].toString("LUMAFLOW Program"); c.enabled = o["enabled"].toBool();
    c.region = o["region"].toInt(-1); c.groups = o["groups"].toString(); return c;
}

// ================================================================ NDI
// Minimal declarations matching the NDI SDK C ABI (Processing.NDI.structs.h /
// Processing.NDI.Send.h, NDI SDK v5/v6). The NDI runtime is NOT bundled: users
// install it from https://ndi.video (NDI Tools / NDI Runtime). NDI® is a
// registered trademark of Vizrt NDI AB.
namespace ndi {
constexpr int32_t fourcc(char a, char b, char c, char d) { return int32_t(uint32_t(uint8_t(a)) | (uint32_t(uint8_t(b)) << 8) | (uint32_t(uint8_t(c)) << 16) | (uint32_t(uint8_t(d)) << 24)); }
constexpr int32_t kFourCC_RGBA = fourcc('R', 'G', 'B', 'A');
constexpr int32_t kProgressive = 1;
constexpr int64_t kTimecodeSynthesize = INT64_MAX;
struct SendCreate { const char* p_ndi_name; const char* p_groups; bool clock_video; bool clock_audio; };
struct VideoFrameV2 {
    int xres, yres; int32_t FourCC; int frame_rate_N, frame_rate_D; float picture_aspect_ratio; int32_t frame_format_type;
    int64_t timecode; uint8_t* p_data; int line_stride_in_bytes; const char* p_metadata; int64_t timestamp;
};
using InitializeFn = bool (*)();
using DestroyFn = void (*)();
using SendCreateFn = void* (*)(const SendCreate*);
using SendDestroyFn = void (*)(void*);
using SendVideoFn = void (*)(void*, const VideoFrameV2*);
using ConnectionsFn = int (*)(void*, uint32_t);

struct Runtime {
    QLibrary lib; bool ok = false; int users = 0; QString path, error;
    InitializeFn initialize = nullptr; DestroyFn destroy = nullptr; SendCreateFn sendCreate = nullptr;
    SendDestroyFn sendDestroy = nullptr; SendVideoFn sendVideo = nullptr; ConnectionsFn connections = nullptr;
};
static QMutex g_mutex;
static Runtime& rt() { static Runtime r; return r; }

static bool acquire(QString* err) {
    QMutexLocker l(&g_mutex);
    Runtime& r = rt();
    if (r.ok) { ++r.users; return true; }
    const QString path = findNdiRuntime();
    if (path.isEmpty()) { r.error = "NDI runtime not installed (get NDI Tools / NDI Runtime from https://ndi.video)"; if (err) *err = r.error; return false; }
    r.lib.setFileName(path);
    if (!r.lib.load()) { r.error = "cannot load " + path + ": " + r.lib.errorString(); if (err) *err = r.error; return false; }
    r.initialize = reinterpret_cast<InitializeFn>(r.lib.resolve("NDIlib_initialize"));
    r.destroy = reinterpret_cast<DestroyFn>(r.lib.resolve("NDIlib_destroy"));
    r.sendCreate = reinterpret_cast<SendCreateFn>(r.lib.resolve("NDIlib_send_create"));
    r.sendDestroy = reinterpret_cast<SendDestroyFn>(r.lib.resolve("NDIlib_send_destroy"));
    r.sendVideo = reinterpret_cast<SendVideoFn>(r.lib.resolve("NDIlib_send_send_video_v2"));
    r.connections = reinterpret_cast<ConnectionsFn>(r.lib.resolve("NDIlib_send_get_no_connections"));
    if (!r.initialize || !r.destroy || !r.sendCreate || !r.sendDestroy || !r.sendVideo) { r.error = "NDI runtime is missing required functions"; r.lib.unload(); if (err) *err = r.error; return false; }
    if (!r.initialize()) { r.error = "NDIlib_initialize failed (CPU not supported?)"; r.lib.unload(); if (err) *err = r.error; return false; }
    r.ok = true; r.users = 1; r.path = path;
    logInfo("streams", "NDI runtime loaded from " + path);
    return true;
}
static void release() {
    QMutexLocker l(&g_mutex);
    Runtime& r = rt();
    if (!r.ok || --r.users > 0) return;
    r.destroy(); r.lib.unload(); r.ok = false;
}
} // namespace ndi

QString findNdiRuntime(QStringList* searched) {
    QStringList candidates;
    const QByteArray explicitPath = qgetenv("LUMAFLOW_NDI_LIB");
    if (!explicitPath.isEmpty()) candidates << QString::fromLocal8Bit(explicitPath);
#if defined(Q_OS_WIN)
    const QString name = "Processing.NDI.Lib.x64.dll";
#elif defined(Q_OS_MACOS)
    const QString name = "libndi.dylib";
#else
    const QString name = "libndi.so.6";
#endif
    for (const char* env : {"NDI_RUNTIME_DIR_V6", "NDI_RUNTIME_DIR_V5"}) {
        const QByteArray d = qgetenv(env);
        if (!d.isEmpty()) candidates << QDir(QString::fromLocal8Bit(d)).filePath(name);
    }
#if defined(Q_OS_MACOS)
    candidates << "/usr/local/lib/libndi.dylib" << "/Library/NDI SDK for Apple/lib/macOS/libndi.dylib" << "/Applications/NDI Video Monitor.app/Contents/Frameworks/libndi.dylib";
#elif defined(Q_OS_WIN)
    candidates << name;                                  // PATH / application directory
#else
    candidates << "/usr/lib/libndi.so.6" << "/usr/local/lib/libndi.so.6" << "/usr/lib/x86_64-linux-gnu/libndi.so.6" << "/usr/local/lib/libndi.so.5" << "/usr/lib/libndi.so.5";
#endif
    if (searched) *searched = candidates;
    for (const auto& c : candidates) if (QFileInfo::exists(c)) return c;
#if defined(Q_OS_WIN)
    QLibrary probe(name); if (probe.load()) { const QString f = probe.fileName(); probe.unload(); return f; }
#endif
    return {};
}

class NdiSink : public VideoSink {
public:
    ~NdiSink() override { close(); }
    bool open(const StreamConfig& cfg, int w, int h, double fps, QString* err) override {
        if (!ndi::acquire(err)) return false;
        m_acquired = true;
        m_name = cfg.name.toUtf8(); m_groups = cfg.groups.toUtf8();
        ndi::SendCreate c{m_name.constData(), m_groups.isEmpty() ? nullptr : m_groups.constData(), false, false};   // we pace frames ourselves
        m_send = ndi::rt().sendCreate(&c);
        if (!m_send) { if (err) *err = "NDIlib_send_create failed"; close(); return false; }
        m_fps = fps; Q_UNUSED(w); Q_UNUSED(h);
        return true;
    }
    bool send(const QImage& img, double t) override {
        if (!m_send) return false;
        ndi::VideoFrameV2 f{};
        f.xres = img.width(); f.yres = img.height(); f.FourCC = ndi::kFourCC_RGBA;
        f.frame_rate_N = int(std::lround(m_fps * 1000)); f.frame_rate_D = 1000;
        f.picture_aspect_ratio = float(img.width()) / std::max(1, img.height());
        f.frame_format_type = ndi::kProgressive; f.timecode = ndi::kTimecodeSynthesize;
        f.p_data = const_cast<uint8_t*>(img.constBits()); f.line_stride_in_bytes = int(img.bytesPerLine());
        f.p_metadata = nullptr; f.timestamp = int64_t(t * 1e7);   // 100 ns units
        ndi::rt().sendVideo(m_send, &f);                          // synchronous: data only needs to live for the call
        return true;
    }
    void close() override {
        if (m_send) { ndi::rt().sendDestroy(m_send); m_send = nullptr; }
        if (m_acquired) { ndi::release(); m_acquired = false; }
    }
    int connections() override { return m_send && ndi::rt().connections ? ndi::rt().connections(m_send, 0) : -1; }
private:
    void* m_send = nullptr; bool m_acquired = false; QByteArray m_name, m_groups; double m_fps = 60;
};

// ================================================================ Spout (Windows)
#ifdef LF_HAVE_SPOUT
class SpoutSink : public VideoSink {
public:
    ~SpoutSink() override { close(); }
    bool open(const StreamConfig& cfg, int, int, double, QString* err) override {
        if (!m_dx.OpenDirectX11()) { if (err) *err = "DirectX 11 initialisation failed"; return false; }
        m_open = true;
        m_dx.SetSenderFormat(DXGI_FORMAT_R8G8B8A8_UNORM);         // our frames are RGBA8888
        if (!m_dx.SetSenderName(cfg.name.toUtf8().constData())) { if (err) *err = "Spout sender name rejected"; close(); return false; }
        return true;
    }
    bool send(const QImage& img, double) override { return m_open && m_dx.SendImage(img.constBits(), unsigned(img.width()), unsigned(img.height()), unsigned(img.bytesPerLine())); }
    void close() override { if (m_open) { m_dx.ReleaseSender(); m_dx.CloseDirectX11(); m_open = false; } }
private:
    spoutDX m_dx; bool m_open = false;
};
#endif

// ================================================================ Syphon (macOS) - implemented in SyphonSink.mm
#ifdef LF_HAVE_SYPHON
std::unique_ptr<VideoSink> createSyphonSink();
#endif

// ================================================================ plugin output modules
class PluginOutputSink : public VideoSink {
public:
    explicit PluginOutputSink(LfOutputDesc d) : m_d(d) {}
    ~PluginOutputSink() override { close(); }
    bool open(const StreamConfig& cfg, int w, int h, double fps, QString* err) override {
        const QByteArray json = QJsonDocument(QJsonObject{{"name", cfg.name}, {"fps", fps}}).toJson(QJsonDocument::Compact);
        if (!m_d.open || m_d.open(w, h, json.constData()) != 0) { if (err) *err = "plugin output failed to open"; return false; }
        m_open = true; return true;
    }
    bool send(const QImage& img, double t) override { if (m_open && m_d.frame) m_d.frame(img.constBits(), img.width(), img.height(), t); return m_open; }
    void close() override { if (m_open && m_d.close) m_d.close(); m_open = false; }
private:
    LfOutputDesc m_d; bool m_open = false;
};

// ================================================================ factory / availability
std::unique_ptr<VideoSink> StreamOutputManager::create(const QString& type, PluginHost* plugins, QString* why) {
    if (type == "ndi") return std::make_unique<NdiSink>();
    if (type == "spout") {
#ifdef LF_HAVE_SPOUT
        return std::make_unique<SpoutSink>();
#else
        if (why) *why = "Spout is available in Windows builds only"; return nullptr;
#endif
    }
    if (type == "syphon") {
#ifdef LF_HAVE_SYPHON
        return createSyphonSink();
#else
        if (why) *why = "Syphon is available in macOS builds only"; return nullptr;
#endif
    }
    if (type.startsWith("plugin:") && plugins) {
        const QString id = type.mid(7).section('/', 1);
        for (const auto& d : plugins->outputModules()) if (QString::fromUtf8(d.id ? d.id : "") == id) return std::make_unique<PluginOutputSink>(d);
        if (why) *why = "output plugin '" + id + "' not loaded"; return nullptr;
    }
    if (why) *why = "unknown stream type " + type;
    return nullptr;
}

SinkAvailability StreamOutputManager::probe(const QString& type, PluginHost* plugins) {
    SinkAvailability a; a.type = type;
    if (type == "ndi") {
        a.label = "NDI®";
        const QString p = findNdiRuntime();
        a.available = !p.isEmpty();
        a.detail = a.available ? "runtime: " + p : "NDI runtime not found - install NDI Tools or the NDI Runtime from ndi.video (not bundled for licensing reasons)";
    } else if (type == "spout") {
        a.label = "Spout";
#ifdef LF_HAVE_SPOUT
        a.available = true; a.detail = "Spout 2 (DirectX 11 texture sharing)";
#else
        a.detail = "Windows builds only";
#endif
    } else if (type == "syphon") {
        a.label = "Syphon";
#ifdef LF_HAVE_SYPHON
        a.available = true; a.detail = "Syphon (Metal texture sharing)";
#else
        a.detail = "macOS builds only";
#endif
    } else if (type.startsWith("plugin:")) {
        a.label = type.mid(7); a.available = create(type, plugins, nullptr) != nullptr; a.detail = "output plugin";
    }
    return a;
}

QVector<SinkAvailability> StreamOutputManager::availability() const {
    QVector<SinkAvailability> v{probe("ndi", m_plugins), probe("spout", m_plugins), probe("syphon", m_plugins)};
    if (m_plugins) for (const auto& r : m_plugins->records()) if (r.loaded && r.outputs > 0)
        for (const auto& d : m_plugins->outputModules()) v << probe(QString("plugin:%1/%2").arg(r.id, QString::fromUtf8(d.id ? d.id : "")), m_plugins);
    return v;
}

// ================================================================ manager
StreamOutputManager::StreamOutputManager(PluginHost* plugins) : m_plugins(plugins) { m_thread = std::thread([this] { run(); }); }
StreamOutputManager::~StreamOutputManager() { stop(); }

void StreamOutputManager::stop() {
    m_run = false; m_cond.wakeAll();
    if (m_thread.joinable()) m_thread.join();
}

void StreamOutputManager::setConfigs(const QVector<StreamConfig>& c) { QMutexLocker l(&m_mutex); m_cfg = c; m_cfgDirty = true; m_cond.wakeAll(); }
QVector<StreamConfig> StreamOutputManager::configs() const { QMutexLocker l(&m_mutex); return m_cfg; }
void StreamOutputManager::setRegions(const QVector<OutputRegion>& r) { QMutexLocker l(&m_mutex); m_regions = r; }
QVector<StreamStatus> StreamOutputManager::status() const { QMutexLocker l(&m_mutex); return m_status; }

void StreamOutputManager::submit(const QImage& program, double t) {
    QMutexLocker l(&m_mutex);
    bool any = false; for (const auto& c : m_cfg) any |= c.enabled;
    if (!any) return;
    if (m_hasFrame) for (auto& s : m_status) if (s.state == "running") ++s.dropped;   // previous frame not consumed yet
    m_pending = program; m_pendingTime = t; m_hasFrame = true;                       // implicitly shared, no copy
    m_cond.wakeAll();
}

void StreamOutputManager::run() {
    struct Active { StreamConfig cfg; std::unique_ptr<VideoSink> sink; int w = 0, h = 0; bool failed = false; QString error; quint64 frames = 0; };
    std::vector<Active> active;
    auto shutdown = [&] { for (auto& a : active) if (a.sink) a.sink->close(); active.clear(); };
    while (m_run) {
        QImage frame; double t = 0; QVector<StreamConfig> cfg; QVector<OutputRegion> regions; bool reconfigure = false;
        {
            QMutexLocker l(&m_mutex);
            while (m_run && !m_hasFrame && !m_cfgDirty) m_cond.wait(&m_mutex, 200);
            if (!m_run) break;
            if (m_cfgDirty) { cfg = m_cfg; m_cfgDirty = false; reconfigure = true; }
            if (m_hasFrame) { frame = m_pending; t = m_pendingTime; m_hasFrame = false; m_pending = QImage(); }
            regions = m_regions;
        }
        if (reconfigure) {
            shutdown();
            for (const auto& c : cfg) { Active a; a.cfg = c; active.push_back(std::move(a)); }
        }
        if (!frame.isNull()) {
            for (auto& a : active) {
                if (!a.cfg.enabled || a.failed) continue;
                QImage img = frame;
                if (a.cfg.region >= 0 && a.cfg.region < regions.size()) img = regions[a.cfg.region].process(frame, QSize());
                if (img.format() != QImage::Format_RGBA8888) img = img.convertToFormat(QImage::Format_RGBA8888);
                if (!a.sink || img.width() != a.w || img.height() != a.h) {   // (re)open on first frame or size change
                    if (a.sink) a.sink->close();
                    QString why;
                    a.sink = create(a.cfg.type, m_plugins, &why);
                    if (!a.sink || !a.sink->open(a.cfg, img.width(), img.height(), m_fps, &why)) {
                        a.failed = true; a.error = why; a.sink.reset();
                        FaultRegistry::instance().report(FaultSeverity::Warning, "streams", QString("%1 stream '%2': %3").arg(a.cfg.type, a.cfg.name, why), "Check that the runtime/driver is installed; other outputs are unaffected.");
                        continue;
                    }
                    a.w = img.width(); a.h = img.height();
                    logInfo("streams", QString("%1 stream '%2' started (%3x%4)").arg(a.cfg.type, a.cfg.name).arg(a.w).arg(a.h));
                }
                if (a.sink->send(img, t)) ++a.frames;
            }
        }
        QVector<StreamStatus> st;
        for (auto& a : active) {
            StreamStatus s; s.type = a.cfg.type; s.name = a.cfg.name; s.width = a.w; s.height = a.h; s.frames = a.frames;
            s.state = !a.cfg.enabled ? "off" : a.failed ? "error: " + a.error : a.sink ? "running" : "waiting for frame";
            s.connections = a.sink ? a.sink->connections() : -1;
            st << s;
        }
        QMutexLocker l(&m_mutex);
        for (int i = 0; i < st.size() && i < m_status.size(); ++i) if (m_status[i].type == st[i].type && m_status[i].name == st[i].name) st[i].dropped = m_status[i].dropped;
        m_status = st;
    }
    shutdown();
}

} // namespace lf
