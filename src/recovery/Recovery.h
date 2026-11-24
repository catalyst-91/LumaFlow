#pragma once
// Reliability services: fault registry (feeds the Recovery dashboard),
// rendering watchdog, crash-recovery session guard, autosave scheduler and
// last-known-good state.
#include <QString>
#include <QDateTime>
#include <QVector>
#include <QMutex>
#include <QJsonObject>
#include <atomic>
#include <thread>
#include <functional>

namespace lf {

enum class FaultSeverity { Info, Warning, Error, Critical };

struct Fault {
    quint64 id = 0;
    QDateTime time;
    FaultSeverity severity = FaultSeverity::Warning;
    QString subsystem;           // "renderer", "network", "shader", "media", "plugin", ...
    QString message;
    QString recommendation;
    bool resolved = false;
    int count = 1;
};

class FaultRegistry {
public:
    static FaultRegistry& instance();
    quint64 report(FaultSeverity s, const QString& subsystem, const QString& msg, const QString& recommendation = {});
    void resolve(const QString& subsystem, const QString& messagePrefix = {});
    QVector<Fault> faults(bool includeResolved = false) const;
    int activeCount(FaultSeverity min = FaultSeverity::Warning) const;
    void clearResolved();
    quint64 revision() const { return m_rev; }
private:
    mutable QMutex m_mutex;
    QVector<Fault> m_faults;
    quint64 m_next = 1;
    std::atomic<quint64> m_rev{0};
};

// Monitors a heartbeat counter from the render thread. If no frame completes
// within the timeout the stall callback is invoked (from the watchdog thread).
class RenderWatchdog {
public:
    ~RenderWatchdog() { stop(); }
    void start(int timeoutMs, std::function<void(int stalledMs)> onStall, std::function<void()> onRecover);
    void stop();
    void beat() { m_beats.fetch_add(1, std::memory_order_relaxed); }
    bool stalled() const { return m_stalled; }
    quint64 beats() const { return m_beats; }
private:
    std::atomic<quint64> m_beats{0};
    std::atomic<bool> m_run{false}, m_stalled{false};
    std::thread m_thread;
};

// Crash detection: a lock file is created at startup and removed on clean
// shutdown. If present at startup, the previous session crashed.
class SessionGuard {
public:
    explicit SessionGuard(const QString& dataDir);
    bool previousSessionCrashed() const { return m_crashed; }
    QJsonObject previousSessionInfo() const { return m_info; }
    void begin(const QString& projectPath);
    void updateProject(const QString& projectPath);
    void end();                                   // clean shutdown
    QString autosavePath() const;
    QString lastKnownGoodPath() const;
    QString backupDir() const;
    QString dataDir() const { return m_dir; }
private:
    QString m_dir;
    bool m_crashed = false;
    QJsonObject m_info;
};

} // namespace lf
