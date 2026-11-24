#include "Recovery.h"
#include "core/Log.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QCoreApplication>
#include <chrono>

namespace lf {

FaultRegistry& FaultRegistry::instance() { static FaultRegistry r; return r; }

quint64 FaultRegistry::report(FaultSeverity s, const QString& sub, const QString& msg, const QString& rec) {
    QMutexLocker l(&m_mutex);
    for (auto& f : m_faults) if (!f.resolved && f.subsystem == sub && f.message == msg) { ++f.count; f.time = QDateTime::currentDateTime(); ++m_rev; return f.id; }
    Fault f; f.id = m_next++; f.time = QDateTime::currentDateTime(); f.severity = s; f.subsystem = sub; f.message = msg; f.recommendation = rec;
    m_faults << f;
    if (m_faults.size() > 500) m_faults.removeFirst();
    ++m_rev;
    l.unlock();
    Log::instance().write(s >= FaultSeverity::Error ? LogLevel::Error : LogLevel::Warning, sub, msg);
    return f.id;
}

void FaultRegistry::resolve(const QString& sub, const QString& prefix) {
    QMutexLocker l(&m_mutex);
    for (auto& f : m_faults) if (!f.resolved && f.subsystem == sub && f.message.startsWith(prefix)) { f.resolved = true; ++m_rev; }
}

QVector<Fault> FaultRegistry::faults(bool all) const {
    QMutexLocker l(&m_mutex);
    if (all) return m_faults;
    QVector<Fault> v; for (auto& f : m_faults) if (!f.resolved) v << f; return v;
}

int FaultRegistry::activeCount(FaultSeverity min) const {
    QMutexLocker l(&m_mutex); int n = 0;
    for (auto& f : m_faults) if (!f.resolved && f.severity >= min) ++n;
    return n;
}

void FaultRegistry::clearResolved() { QMutexLocker l(&m_mutex); m_faults.erase(std::remove_if(m_faults.begin(), m_faults.end(), [](auto& f) { return f.resolved; }), m_faults.end()); ++m_rev; }

void RenderWatchdog::start(int timeoutMs, std::function<void(int)> onStall, std::function<void()> onRecover) {
    stop();
    m_run = true;
    m_thread = std::thread([this, timeoutMs, onStall, onRecover] {
        quint64 last = m_beats; auto lastChange = std::chrono::steady_clock::now();
        while (m_run) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            const quint64 b = m_beats;
            const auto now = std::chrono::steady_clock::now();
            if (b != last) {
                last = b; lastChange = now;
                if (m_stalled) { m_stalled = false; if (onRecover) onRecover(); }
            } else {
                const int ms = int(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastChange).count());
                if (ms > timeoutMs && !m_stalled) { m_stalled = true; if (onStall) onStall(ms); }
            }
        }
    });
}

void RenderWatchdog::stop() { m_run = false; if (m_thread.joinable()) m_thread.join(); }

SessionGuard::SessionGuard(const QString& dir) : m_dir(dir) {
    QDir().mkpath(dir);
    QDir().mkpath(backupDir());
    QFile lock(QDir(dir).filePath("session.lock"));
    if (lock.exists() && lock.open(QIODevice::ReadOnly)) { m_crashed = true; m_info = QJsonDocument::fromJson(lock.readAll()).object(); }
}

void SessionGuard::begin(const QString& project) { updateProject(project); }

void SessionGuard::updateProject(const QString& project) {
    QFile lock(QDir(m_dir).filePath("session.lock"));
    if (lock.open(QIODevice::WriteOnly | QIODevice::Truncate))
        lock.write(QJsonDocument(QJsonObject{{"pid", qint64(QCoreApplication::applicationPid())}, {"project", project},
                                             {"started", QDateTime::currentDateTime().toString(Qt::ISODate)}}).toJson());
}

void SessionGuard::end() { QFile::remove(QDir(m_dir).filePath("session.lock")); }
QString SessionGuard::autosavePath() const { return QDir(m_dir).filePath("autosave.lumaflow"); }
QString SessionGuard::lastKnownGoodPath() const { return QDir(m_dir).filePath("last-known-good.lumaflow"); }
QString SessionGuard::backupDir() const { return QDir(m_dir).filePath("backups"); }

} // namespace lf
