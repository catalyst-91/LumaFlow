#include "MediaPlayer.h"
#include "core/Log.h"
#include <QFileInfo>
#include <QBuffer>
#include <QDirIterator>
#include <sqlite3.h>
#include <chrono>
#include <cmath>
#ifdef LF_HAVE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#endif

namespace lf {

static bool isImageFile(const QString& p) {
    static const QStringList ext{"png", "jpg", "jpeg", "bmp", "gif", "webp", "tif", "tiff", "svg"};
    return ext.contains(QFileInfo(p).suffix().toLower());
}

#ifdef LF_HAVE_FFMPEG
struct Decoder {
    AVFormatContext* fmt = nullptr; AVCodecContext* ctx = nullptr; SwsContext* sws = nullptr;
    int stream = -1; AVFrame* frame = nullptr; AVPacket* pkt = nullptr; double tb = 0;
    int outW = 0, outH = 0;
    ~Decoder() { if (sws) sws_freeContext(sws); if (frame) av_frame_free(&frame); if (pkt) av_packet_free(&pkt); if (ctx) avcodec_free_context(&ctx); if (fmt) avformat_close_input(&fmt); }
    bool open(const QString& path, int maxW, MediaInfo& info) {
        if (avformat_open_input(&fmt, path.toUtf8().constData(), nullptr, nullptr) < 0) { info.error = "cannot open container"; return false; }
        if (avformat_find_stream_info(fmt, nullptr) < 0) { info.error = "no stream info"; return false; }
        const AVCodec* codec = nullptr;
        stream = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (stream < 0 || !codec) { info.error = "no video stream"; return false; }
        ctx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(ctx, fmt->streams[stream]->codecpar);
        ctx->thread_count = 2;
        if (avcodec_open2(ctx, codec, nullptr) < 0) { info.error = "cannot open codec"; return false; }
        frame = av_frame_alloc(); pkt = av_packet_alloc();
        tb = av_q2d(fmt->streams[stream]->time_base);
        info.width = ctx->width; info.height = ctx->height; info.codec = codec->name;
        const AVRational fr = av_guess_frame_rate(fmt, fmt->streams[stream], nullptr);
        info.fps = fr.den ? av_q2d(fr) : 25.0;
        info.duration = fmt->duration > 0 ? fmt->duration / double(AV_TIME_BASE) : 0;
        const AVPixFmtDescriptor* pd = av_pix_fmt_desc_get(ctx->pix_fmt);
        info.hasAlpha = pd && (pd->flags & AV_PIX_FMT_FLAG_ALPHA);
        outW = std::min(ctx->width, maxW); outH = int(std::lround(double(ctx->height) * outW / std::max(1, ctx->width)));
        outH = std::max(2, outH & ~1);
        info.isVideo = true; info.valid = true;
        return true;
    }
    // Returns false at end of stream.
    bool next(QImage& img, double& pts) {
        while (true) {
            const int r = avcodec_receive_frame(ctx, frame);
            if (r == 0) {
                if (!sws) sws = sws_getContext(frame->width, frame->height, AVPixelFormat(frame->format), outW, outH, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
                img = QImage(outW, outH, QImage::Format_RGBA8888);
                uint8_t* dst[4] = {img.bits(), nullptr, nullptr, nullptr}; int ls[4] = {int(img.bytesPerLine()), 0, 0, 0};
                sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dst, ls);
                const int64_t ts = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;
                pts = ts == AV_NOPTS_VALUE ? 0 : ts * tb;
                if (fmt->streams[stream]->start_time != AV_NOPTS_VALUE) pts -= fmt->streams[stream]->start_time * tb;
                return true;
            }
            if (r == AVERROR_EOF) return false;
            if (av_read_frame(fmt, pkt) < 0) { avcodec_send_packet(ctx, nullptr); continue; }
            if (pkt->stream_index == stream) avcodec_send_packet(ctx, pkt);
            av_packet_unref(pkt);
        }
    }
    void seek(double s) {
        const int64_t ts = int64_t(s / tb) + (fmt->streams[stream]->start_time != AV_NOPTS_VALUE ? fmt->streams[stream]->start_time : 0);
        av_seek_frame(fmt, stream, ts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(ctx);
    }
};
#else
struct Decoder { bool open(const QString&, int, MediaInfo& i) { i.error = "built without FFmpeg"; return false; } bool next(QImage&, double&) { return false; } void seek(double) {} };
#endif

struct MediaPlayer::Impl { std::unique_ptr<Decoder> dec; };

MediaInfo probeMedia(const QString& path) {
    MediaInfo i; i.path = path;
    if (!QFileInfo::exists(path)) { i.error = "file not found"; return i; }
    if (isImageFile(path)) {
        QImage img(path);
        if (img.isNull()) { i.error = "unsupported image"; return i; }
        i.valid = true; i.width = img.width(); i.height = img.height(); i.hasAlpha = img.hasAlphaChannel(); i.codec = QFileInfo(path).suffix();
        return i;
    }
    Decoder d; d.open(path, 64, i);
    return i;
}

QImage mediaThumbnail(const QString& path, int maxSide) {
    if (isImageFile(path)) { QImage img(path); return img.isNull() ? img : img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation); }
    MediaInfo i; Decoder d;
    if (!d.open(path, maxSide, i)) return {};
    if (i.duration > 2) d.seek(i.duration * 0.25);
    QImage img; double pts;
    return d.next(img, pts) ? img.copy() : QImage();
}

MediaPlayer::MediaPlayer() : d(new Impl) {}
MediaPlayer::~MediaPlayer() { close(); }

bool MediaPlayer::open(const QString& path, int maxWidth) {
    close();
    m_maxWidth = maxWidth;
    m_info = MediaInfo{}; m_info.path = path;
    if (!QFileInfo::exists(path)) { m_info.error = "file not found"; logWarn("media", "missing media: " + path); return false; }
    if (isImageFile(path)) {
        m_still = QImage(path);
        if (m_still.isNull()) { m_info.error = "unsupported image"; return false; }
        if (m_still.width() > maxWidth) m_still = m_still.scaledToWidth(maxWidth, Qt::SmoothTransformation);
        m_still = m_still.convertToFormat(QImage::Format_RGBA8888);
        m_info.valid = true; m_info.width = m_still.width(); m_info.height = m_still.height(); m_info.hasAlpha = m_still.hasAlphaChannel();
        m_current = m_still;
        return true;
    }
    d->dec = std::make_unique<Decoder>();
    if (!d->dec->open(path, maxWidth, m_info)) { logWarn("media", path + ": " + m_info.error); d->dec.reset(); return false; }
    // Short clips are fully cached (enables exact reverse playback and seamless loops)
    const double frames = m_info.duration * m_info.fps;
    const double bytes = frames * d->dec->outW * d->dec->outH * 4.0;
    m_cacheAll = frames > 0 && frames <= 900 && bytes < 768.0 * 1024 * 1024;
    m_run = true; m_pos = 0;
    m_thread = std::thread([this] { decodeLoop(); });
    return true;
}

void MediaPlayer::close() {
    m_run = false; m_cond.wakeAll();
    if (m_thread.joinable()) m_thread.join();
    QMutexLocker l(&m_mutex);
    m_queue.clear(); m_cache.clear(); m_still = QImage(); m_current = QImage(); m_info = MediaInfo{}; d->dec.reset(); m_cacheAll = false;
}

void MediaPlayer::setReverse(bool r) {
    if (r && !m_cacheAll && m_info.isVideo) { logWarn("media", "reverse playback needs a clip short enough for the frame cache (<= 900 frames)"); return; }
    m_reverse = r;
}

void MediaPlayer::seek(double s) { m_pos = s; if (!m_cacheAll) { m_seekTo = s; m_cond.wakeAll(); } }

void MediaPlayer::decodeLoop() {
    Decoder* dec = d->dec.get();
    if (m_cacheAll) {
        QImage img; double pts;
        std::vector<Frame> all;
        while (m_run && dec->next(img, pts)) all.push_back({pts, img});
        QMutexLocker l(&m_mutex); m_cache = std::move(all);
        if (!m_cache.empty()) m_info.duration = std::max(m_info.duration, m_cache.back().pts + 1.0 / std::max(1.0, m_info.fps));
        return;
    }
    const size_t kMax = 12;     // bounded queue
    while (m_run) {
        const double sk = m_seekTo.exchange(-1);
        if (sk >= 0) { dec->seek(sk); QMutexLocker l(&m_mutex); m_queue.clear(); }
        { QMutexLocker l(&m_mutex); while (m_run && m_queue.size() >= kMax && m_seekTo < 0) m_cond.wait(&m_mutex, 50); }
        if (!m_run) break;
        if (m_seekTo >= 0) continue;
        QImage img; double pts;
        if (!dec->next(img, pts)) {
            if (m_loop) { dec->seek(m_in); continue; }
            QMutexLocker l(&m_mutex); m_cond.wait(&m_mutex, 50); continue;
        }
        QMutexLocker l(&m_mutex); m_queue.push_back({pts, img});
    }
}

QImage MediaPlayer::advance(double dt) {
    if (!m_info.valid) return {};
    if (!m_info.isVideo) return m_still;
    const double dur = m_info.duration > 0 ? m_info.duration : 1e9;
    const double in = std::clamp(m_in, 0.0, dur), out = m_out > in ? std::min(m_out, dur) : dur;
    if (m_playing) m_pos += dt * m_speed * (m_reverse ? -1.0 : 1.0);
    if (m_pos >= out) { if (m_loop) { m_pos = in + std::fmod(m_pos - in, std::max(1e-3, out - in)); if (!m_cacheAll) seek(m_pos); } else m_pos = out; }
    if (m_pos < in) { if (m_loop) m_pos = out - std::fmod(in - m_pos, std::max(1e-3, out - in)); else m_pos = in; }
    QMutexLocker l(&m_mutex);
    if (m_cacheAll) {
        if (!m_cache.empty()) {
            const size_t idx = std::min(m_cache.size() - 1, size_t(std::max(0.0, m_pos * m_info.fps)));
            m_current = m_cache[idx].img;
        }
    } else {
        // drop frames that are already late, keep the latest one not in the future
        while (m_queue.size() > 1 && m_queue[1].pts <= m_pos) { m_queue.pop_front(); ++m_dropped; }
        if (!m_queue.empty() && m_queue.front().pts <= m_pos + 0.5 / std::max(1.0, m_info.fps)) { m_current = m_queue.front().img; if (m_queue.size() > 1) { m_queue.pop_front(); } }
        if (!m_queue.empty() && m_queue.front().pts > m_pos + 2.0) { l.unlock(); seek(m_pos); }       // stale after loop
        m_cond.wakeAll();
    }
    return m_current;
}

QImage MediaPlayer::currentFrame() const { QMutexLocker l(&m_mutex); return m_current; }
int MediaPlayer::queuedFrames() const { QMutexLocker l(&m_mutex); return m_cacheAll ? int(m_cache.size()) : int(m_queue.size()); }

// ---------------------------------------------------------------- library
#define DB static_cast<sqlite3*>(m_db)

bool MediaLibrary::open(const QString& path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path.toUtf8().constData(), &db) != SQLITE_OK) { logError("media", "cannot open media DB"); return false; }
    m_db = db;
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS media(path TEXT PRIMARY KEY, w INT, h INT, duration REAL, fps REAL, video INT, alpha INT, codec TEXT, thumb BLOB, added TEXT DEFAULT CURRENT_TIMESTAMP)", nullptr, nullptr, nullptr);
    return true;
}

MediaLibrary::~MediaLibrary() { if (m_db) sqlite3_close(DB); }

bool MediaLibrary::add(const QString& path, QString* err) {
    const MediaInfo i = probeMedia(path);
    if (!i.valid) { if (err) *err = i.error; return false; }
    QByteArray png; QBuffer buf(&png); buf.open(QIODevice::WriteOnly); mediaThumbnail(path).save(&buf, "PNG");
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(DB, "INSERT OR REPLACE INTO media(path,w,h,duration,fps,video,alpha,codec,thumb) VALUES(?,?,?,?,?,?,?,?,?)", -1, &st, nullptr);
    const QByteArray p = path.toUtf8(), c = i.codec.toUtf8();
    sqlite3_bind_text(st, 1, p.constData(), -1, SQLITE_TRANSIENT); sqlite3_bind_int(st, 2, i.width); sqlite3_bind_int(st, 3, i.height);
    sqlite3_bind_double(st, 4, i.duration); sqlite3_bind_double(st, 5, i.fps); sqlite3_bind_int(st, 6, i.isVideo); sqlite3_bind_int(st, 7, i.hasAlpha);
    sqlite3_bind_text(st, 8, c.constData(), -1, SQLITE_TRANSIENT); sqlite3_bind_blob(st, 9, png.constData(), int(png.size()), SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(st) == SQLITE_DONE; sqlite3_finalize(st);
    return ok;
}

bool MediaLibrary::remove(const QString& path) {
    sqlite3_stmt* st = nullptr; sqlite3_prepare_v2(DB, "DELETE FROM media WHERE path=?", -1, &st, nullptr);
    const QByteArray p = path.toUtf8(); sqlite3_bind_text(st, 1, p.constData(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(st) == SQLITE_DONE; sqlite3_finalize(st); return ok;
}

QVector<MediaInfo> MediaLibrary::items() const {
    QVector<MediaInfo> out; if (!m_db) return out;
    sqlite3_stmt* st = nullptr; sqlite3_prepare_v2(DB, "SELECT path,w,h,duration,fps,video,alpha,codec FROM media ORDER BY path", -1, &st, nullptr);
    while (sqlite3_step(st) == SQLITE_ROW) {
        MediaInfo i; i.path = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(st, 0)));
        i.width = sqlite3_column_int(st, 1); i.height = sqlite3_column_int(st, 2); i.duration = sqlite3_column_double(st, 3); i.fps = sqlite3_column_double(st, 4);
        i.isVideo = sqlite3_column_int(st, 5); i.hasAlpha = sqlite3_column_int(st, 6); i.codec = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(st, 7)));
        i.valid = QFileInfo::exists(i.path); if (!i.valid) i.error = "missing";
        out << i;
    }
    sqlite3_finalize(st); return out;
}

QImage MediaLibrary::thumbnail(const QString& path) const {
    if (!m_db) return {};
    sqlite3_stmt* st = nullptr; sqlite3_prepare_v2(DB, "SELECT thumb FROM media WHERE path=?", -1, &st, nullptr);
    const QByteArray p = path.toUtf8(); sqlite3_bind_text(st, 1, p.constData(), -1, SQLITE_TRANSIENT);
    QImage img;
    if (sqlite3_step(st) == SQLITE_ROW) img.loadFromData(static_cast<const uchar*>(sqlite3_column_blob(st, 0)), sqlite3_column_bytes(st, 0), "PNG");
    sqlite3_finalize(st); return img;
}

QStringList MediaLibrary::missing() const { QStringList m; for (auto& i : items()) if (!i.valid) m << i.path; return m; }

int MediaLibrary::relink(const QString& dir) {
    QMap<QString, QString> byName;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); byName.insert(it.fileName(), it.filePath()); }
    int n = 0;
    for (const auto& p : missing()) { const QString f = QFileInfo(p).fileName(); if (byName.contains(f) && add(byName[f])) { remove(p); ++n; } }
    return n;
}

} // namespace lf
