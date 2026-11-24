#include "Log.h"
#include <QFile>
#include <QTextStream>
#include <cstdio>

namespace lf {

static constexpr int kRingSize = 4096;

Log& Log::instance() { static Log log; return log; }

QString Log::levelName(LogLevel l) {
    switch (l) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Fatal: return "FATAL";
    }
    return "?";
}

void Log::setFile(const QString& path) {
    QMutexLocker lock(&m_mutex);
    m_file = path;
    QFile f(path);
    if (f.exists() && f.size() > 8 * 1024 * 1024) {      // simple rotation
        QFile::remove(path + ".1");
        QFile::rename(path, path + ".1");
    }
}

void Log::write(LogLevel level, const QString& category, const QString& message) {
    if (level < m_min) return;
    LogEntry e{QDateTime::currentDateTime(), level, category, message};
    QMutexLocker lock(&m_mutex);
    if (m_ring.size() < kRingSize) m_ring.push_back(e);
    else { m_ring[m_head] = e; m_head = (m_head + 1) % kRingSize; }
    ++m_seq;
    const QString line = QStringLiteral("%1 [%2] %3: %4")
        .arg(e.time.toString(Qt::ISODateWithMs), levelName(level), category, message);
    if (m_echo) { std::fprintf(stderr, "%s\n", line.toLocal8Bit().constData()); }
    if (!m_file.isEmpty()) {
        QFile f(m_file);
        if (f.open(QIODevice::Append | QIODevice::Text)) { QTextStream(&f) << line << '\n'; }
    }
}

QVector<LogEntry> Log::recent(int max) const {
    QMutexLocker lock(&m_mutex);
    QVector<LogEntry> out;
    const int n = m_ring.size();
    for (int i = 0; i < n; ++i) out.push_back(m_ring[(m_head + i) % n]);
    if (out.size() > max) out = out.mid(out.size() - max);
    return out;
}

quint64 Log::sequence() const { QMutexLocker lock(&m_mutex); return m_seq; }

} // namespace lf
