#pragma once
// Video output configuration (display outputs / LED processor regions) and
// recording / offline rendering through an external FFmpeg encoder process.
#include <QString>
#include <QRectF>
#include <QVector>
#include <QJsonObject>
#include <QImage>
#include <QMutex>
#include <memory>

class QProcess;

namespace lf {

struct OutputRegion {
    QString name = "Output";
    int screen = -1;                 // -1 = windowed, else screen index for borderless fullscreen
    QRectF source{0, 0, 1, 1};       // normalised crop of the program canvas
    int rotation = 0;                // 0/90/180/270
    bool flipH = false, flipV = false;
    QSize customSize;                // optional LED processor resolution (0 = native screen)
    bool enabled = true;
    bool blackout = false;
    int testPattern = -1;            // per-output identify pattern (-1 off)
    QJsonObject toJson() const;
    static OutputRegion fromJson(const QJsonObject& o);
    // Applies crop/rotation/flip/scale to a program frame (used by windows and NDI-like sinks).
    QImage process(const QImage& program, QSize target) const;
};

struct VideoOutputConfig {
    int canvasWidth = 1920, canvasHeight = 1080;
    QVector<OutputRegion> outputs;
    QJsonObject toJson() const;
    static VideoOutputConfig fromJson(const QJsonObject& o);
};

// Records frames to an MP4 (H.264) by piping raw RGBA into `ffmpeg`.
class Recorder {
public:
    ~Recorder();
    static QString findFfmpeg();
    bool start(const QString& path, int w, int h, double fps, const QString& audioWav = {}, QString* err = nullptr);
    bool addFrame(const QImage& rgba);       // thread-safe, non-blocking write to the pipe buffer
    bool finish(QString* err = nullptr);
    bool recording() const { return m_proc != nullptr; }
    quint64 frames() const { return m_frames; }
private:
    QProcess* m_proc = nullptr;
    int m_w = 0, m_h = 0;
    quint64 m_frames = 0;
    QMutex m_mutex;
};

} // namespace lf
