#pragma once
// Network / GPU-sharing video stream outputs: NDI (all platforms, runtime
// loaded dynamically), Spout (Windows, DirectX 11 via SpoutDX), Syphon (macOS,
// Metal) and output-module plugins. Frames are delivered on a dedicated
// worker thread through a latest-frame mailbox, so a slow or blocked sink can
// never stall the render thread (frames are dropped instead).
#include "Output.h"
#include <QObject>
#include <QImage>
#include <QJsonObject>
#include <QMutex>
#include <QWaitCondition>
#include <QVector>
#include <QStringList>
#include <memory>
#include <thread>
#include <atomic>

namespace lf {
class PluginHost;

struct StreamConfig {
    QString type = "ndi";            // "ndi", "spout", "syphon", "plugin:<plugin id>/<output id>"
    QString name = "LUMAFLOW Program";
    bool enabled = false;
    int region = -1;                 // -1 = whole program canvas, else index into video output regions
    QString groups;                  // NDI groups (comma separated, empty = default)
    QJsonObject toJson() const;
    static StreamConfig fromJson(const QJsonObject& o);
};

struct StreamStatus {
    QString type, name;
    QString state;                   // "off", "running", "error: ...", "unavailable: ..."
    quint64 frames = 0, dropped = 0;
    int connections = -1;            // receivers connected (NDI), -1 = unknown
    int width = 0, height = 0;
};

class VideoSink {
public:
    virtual ~VideoSink() = default;
    virtual bool open(const StreamConfig& cfg, int width, int height, double fps, QString* err) = 0;
    virtual bool send(const QImage& rgba8888, double timeSeconds) = 0;   // called on the stream thread
    virtual void close() = 0;
    virtual int connections() { return -1; }
};

struct SinkAvailability { QString type, label; bool available = false; QString detail; };

class StreamOutputManager {
public:
    explicit StreamOutputManager(PluginHost* plugins = nullptr);
    ~StreamOutputManager();
    void setConfigs(const QVector<StreamConfig>& c);       // thread-safe; (re)opens sinks on the stream thread
    QVector<StreamConfig> configs() const;
    void setRegions(const QVector<OutputRegion>& r);       // crop regions (shared with video outputs)
    void setFrameRate(double fps) { m_fps = fps; }
    // Render thread: hand over the newest program frame. Never blocks for long.
    void submit(const QImage& program, double timeSeconds);
    QVector<StreamStatus> status() const;
    QVector<SinkAvailability> availability() const;
    void stop();
    static std::unique_ptr<VideoSink> create(const QString& type, PluginHost* plugins, QString* why);
    static SinkAvailability probe(const QString& type, PluginHost* plugins);
private:
    void run();
    PluginHost* m_plugins;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    QVector<StreamConfig> m_cfg;
    QVector<OutputRegion> m_regions;
    bool m_cfgDirty = false;
    QImage m_pending; double m_pendingTime = 0; bool m_hasFrame = false;
    QVector<StreamStatus> m_status;
    std::atomic<bool> m_run{true};
    std::atomic<double> m_fps{60.0};
    std::thread m_thread;
};

// NDI runtime discovery (exposed for diagnostics and tests).
QString findNdiRuntime(QStringList* searched = nullptr);

} // namespace lf
