#include "Output.h"
#include "core/Log.h"
#include <QProcess>
#include <QStandardPaths>
#include <QJsonArray>
#include <QPainter>
#include <QTransform>
#include <QCoreApplication>
#include <QDir>

namespace lf {

QJsonObject OutputRegion::toJson() const {
    return {{"name", name}, {"screen", screen}, {"source", QJsonArray{source.x(), source.y(), source.width(), source.height()}},
            {"rotation", rotation}, {"flipH", flipH}, {"flipV", flipV}, {"customW", customSize.width()}, {"customH", customSize.height()},
            {"enabled", enabled}, {"blackout", blackout}};
}
OutputRegion OutputRegion::fromJson(const QJsonObject& o) {
    OutputRegion r; r.name = o["name"].toString("Output"); r.screen = o["screen"].toInt(-1);
    const auto s = o["source"].toArray(); if (s.size() == 4) r.source = QRectF(s[0].toDouble(), s[1].toDouble(), s[2].toDouble(), s[3].toDouble());
    r.rotation = o["rotation"].toInt(); r.flipH = o["flipH"].toBool(); r.flipV = o["flipV"].toBool();
    r.customSize = QSize(o["customW"].toInt(), o["customH"].toInt()); r.enabled = o["enabled"].toBool(true); r.blackout = o["blackout"].toBool();
    return r;
}

QImage OutputRegion::process(const QImage& program, QSize target) const {
    if (program.isNull()) return {};
    if (blackout) { QImage b(target.isEmpty() ? QSize(16, 16) : target, QImage::Format_RGB32); b.fill(Qt::black); return b; }
    const QRect src(int(source.x() * program.width()), int(source.y() * program.height()), std::max(1, int(source.width() * program.width())), std::max(1, int(source.height() * program.height())));
    QImage img = program.copy(src);
    if (rotation || flipH || flipV) {
        QTransform t; t.rotate(rotation); t.scale(flipH ? -1 : 1, flipV ? -1 : 1);
        img = img.transformed(t, Qt::SmoothTransformation);
    }
    const QSize out = !customSize.isEmpty() ? customSize : target;
    if (!out.isEmpty() && img.size() != out) img = img.scaled(out, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return img;
}

QJsonObject VideoOutputConfig::toJson() const {
    QJsonArray a; for (auto& o : outputs) a << o.toJson();
    return {{"canvasWidth", canvasWidth}, {"canvasHeight", canvasHeight}, {"outputs", a}};
}
VideoOutputConfig VideoOutputConfig::fromJson(const QJsonObject& o) {
    VideoOutputConfig c; c.canvasWidth = o["canvasWidth"].toInt(1920); c.canvasHeight = o["canvasHeight"].toInt(1080);
    for (auto v : o["outputs"].toArray()) c.outputs << OutputRegion::fromJson(v.toObject());
    return c;
}

Recorder::~Recorder() { finish(); }

QString Recorder::findFfmpeg() {
    const QString local = QDir(QCoreApplication::applicationDirPath()).filePath("ffmpeg");
    if (QFile::exists(local)) return local;
    return QStandardPaths::findExecutable("ffmpeg");
}

bool Recorder::start(const QString& path, int w, int h, double fps, const QString& audio, QString* err) {
    finish();
    const QString exe = findFfmpeg();
    if (exe.isEmpty()) { if (err) *err = "ffmpeg executable not found (install FFmpeg to enable recording)"; return false; }
    m_w = w & ~1; m_h = h & ~1; m_frames = 0;
    QStringList args{"-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgba", "-s", QString("%1x%2").arg(m_w).arg(m_h), "-r", QString::number(fps), "-i", "-"};
    if (!audio.isEmpty()) args << "-i" << audio << "-c:a" << "aac" << "-shortest";
    args << "-c:v" << "libx264" << "-preset" << "veryfast" << "-crf" << "18" << "-pix_fmt" << "yuv420p" << "-movflags" << "+faststart" << path;
    m_proc = new QProcess;
    m_proc->start(exe, args);
    if (!m_proc->waitForStarted(5000)) { if (err) *err = "failed to start ffmpeg"; delete m_proc; m_proc = nullptr; return false; }
    logInfo("output", QString("Recording %1x%2@%3 to %4").arg(m_w).arg(m_h).arg(fps).arg(path));
    return true;
}

bool Recorder::addFrame(const QImage& in) {
    QMutexLocker l(&m_mutex);
    if (!m_proc) return false;
    QImage img = in.size() == QSize(m_w, m_h) ? in : in.scaled(m_w, m_h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (img.format() != QImage::Format_RGBA8888) img = img.convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < m_h; ++y) m_proc->write(reinterpret_cast<const char*>(img.constScanLine(y)), m_w * 4);
    if (m_proc->bytesToWrite() > 64 * 1024 * 1024) m_proc->waitForBytesWritten(2000);   // bounded buffering
    ++m_frames;
    return true;
}

bool Recorder::finish(QString* err) {
    QMutexLocker l(&m_mutex);
    if (!m_proc) return true;
    m_proc->closeWriteChannel();
    const bool ok = m_proc->waitForFinished(600000) && m_proc->exitCode() == 0;
    if (!ok && err) *err = QString::fromUtf8(m_proc->readAllStandardError());
    delete m_proc; m_proc = nullptr;
    return ok;
}

} // namespace lf
