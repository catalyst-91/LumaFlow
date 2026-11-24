#pragma once
// Media playback: images (Qt image formats) and video (FFmpeg/libav).
// Decoding happens on a background thread into a bounded frame queue so the
// renderer never waits for disk or decoder. Supports speed, looping, in/out
// points, seeking and reverse playback (exact for clips that fit the frame
// cache, otherwise unavailable and reported).
#include <QImage>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <QJsonObject>
#include <deque>
#include <thread>
#include <atomic>
#include <memory>

namespace lf {

struct MediaInfo {
    QString path;
    bool valid = false, isVideo = false, hasAlpha = false;
    int width = 0, height = 0;
    double duration = 0, fps = 0;
    QString codec;
    QString error;
};

MediaInfo probeMedia(const QString& path);
QImage mediaThumbnail(const QString& path, int maxSide = 160);

class MediaPlayer {
public:
    MediaPlayer();
    ~MediaPlayer();
    bool open(const QString& path, int maxWidth = 1920);
    void close();
    MediaInfo info() const { return m_info; }
    bool isOpen() const { return m_info.valid; }

    void setSpeed(double s) { m_speed = s; }
    void setLoop(bool l) { m_loop = l; }
    void setReverse(bool r);
    bool reverseSupported() const { return m_cacheAll; }
    void setInOut(double in, double out) { m_in = in; m_out = out; }
    void seek(double seconds);
    void play() { m_playing = true; }
    void pause() { m_playing = false; }
    bool playing() const { return m_playing; }
    double position() const { return m_pos; }
    // Advances the playback clock by dt (wall seconds) and returns the frame to show. Never blocks.
    QImage advance(double dt);
    QImage currentFrame() const;
    int queuedFrames() const;
    quint64 droppedFrames() const { return m_dropped; }
private:
    void decodeLoop();
    struct Frame { double pts; QImage img; };
    MediaInfo m_info;
    QImage m_still;
    int m_maxWidth = 1920;
    std::atomic<double> m_speed{1.0};
    std::atomic<bool> m_loop{true}, m_reverse{false}, m_playing{true}, m_run{false};
    double m_in = 0, m_out = 0;
    double m_pos = 0;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    std::deque<Frame> m_queue;
    std::vector<Frame> m_cache;       // all frames, when the clip is short enough
    bool m_cacheAll = false;
    std::atomic<double> m_seekTo{-1};
    QImage m_current;
    std::thread m_thread;
    std::atomic<quint64> m_dropped{0};
    struct Impl; std::unique_ptr<Impl> d;
};

// Media library index stored in SQLite (path, metadata, thumbnail PNG).
class MediaLibrary {
public:
    bool open(const QString& dbPath);
    ~MediaLibrary();
    bool add(const QString& path, QString* err = nullptr);
    bool remove(const QString& path);
    QVector<MediaInfo> items() const;
    QImage thumbnail(const QString& path) const;
    QStringList missing() const;                       // items whose files no longer exist
    int relink(const QString& searchDir);              // fixes missing items by filename
private:
    void* m_db = nullptr;
};

} // namespace lf
