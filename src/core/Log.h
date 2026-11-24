#pragma once
// Thread-safe diagnostic logging with an in-memory ring buffer (for the UI
// log/recovery panels) and an optional rotating log file.
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QVector>
#include <functional>

namespace lf {

enum class LogLevel { Debug, Info, Warning, Error, Fatal };

struct LogEntry {
    QDateTime time;
    LogLevel level;
    QString category;
    QString message;
};

class Log {
public:
    static Log& instance();
    void write(LogLevel level, const QString& category, const QString& message);
    QVector<LogEntry> recent(int max = 500) const;
    void setFile(const QString& path);
    void setEcho(bool echo) { m_echo = echo; }
    void setMinLevel(LogLevel l) { m_min = l; }
    static QString levelName(LogLevel l);
    quint64 sequence() const;
private:
    Log() = default;
    mutable QMutex m_mutex;
    QVector<LogEntry> m_ring;
    int m_head = 0;
    quint64 m_seq = 0;
    QString m_file;
    bool m_echo = true;
    LogLevel m_min = LogLevel::Info;
};

inline void logDebug(const QString& c, const QString& m) { Log::instance().write(LogLevel::Debug, c, m); }
inline void logInfo(const QString& c, const QString& m) { Log::instance().write(LogLevel::Info, c, m); }
inline void logWarn(const QString& c, const QString& m) { Log::instance().write(LogLevel::Warning, c, m); }
inline void logError(const QString& c, const QString& m) { Log::instance().write(LogLevel::Error, c, m); }

} // namespace lf
