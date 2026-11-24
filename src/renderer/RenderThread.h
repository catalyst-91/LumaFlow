#pragma once
// Dedicated render thread with frame pacing, adaptive quality and a
// watchdog heartbeat. The engine supplies frame snapshots through a callback
// and receives finished frames through another; neither callback may block.
#include "GpuRenderer.h"
#include "recovery/Recovery.h"
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <functional>
#include <atomic>
#include <deque>

namespace lf {

enum class QualityMode { Low, Medium, High, Adaptive };

class RenderThread : public QThread {
    Q_OBJECT
public:
    using InputFn = std::function<RenderInput(double dt)>;
    using OutputFn = std::function<void(const RenderOutput&)>;
    RenderThread(InputFn in, OutputFn out);
    ~RenderThread() override;
    void setTargetFps(double fps) { m_targetFps = fps; }
    double targetFps() const { return m_targetFps; }
    void setQualityMode(QualityMode m) { m_quality = m; }
    QualityMode qualityMode() const { return m_quality; }
    void requestStop();
    // Synchronous shader validation executed on the render thread (isolated program).
    bool validateEffect(const QString& glsl, QString* log, int timeoutMs = 5000);
    void invalidateProgram(const QString& key);
    bool initialised() const { return m_initOk; }
    QString initError() const { return m_initError; }
    QString glInfo() const { return m_glInfo; }
    RenderWatchdog& watchdog() { return m_watchdog; }
    void setPaused(bool p) { m_paused = p; }
protected:
    void run() override;
private:
    InputFn m_in; OutputFn m_out;
    std::atomic<double> m_targetFps{60.0};
    std::atomic<QualityMode> m_quality{QualityMode::Adaptive};
    std::atomic<bool> m_stop{false}, m_initOk{false}, m_paused{false};
    QString m_initError, m_glInfo;
    double m_scale = 1.0;
    RenderWatchdog m_watchdog;
    struct Job { QString glsl; QString key; bool validate; bool done = false; bool ok = false; QString log; };
    QMutex m_jobMutex; QWaitCondition m_jobCond;
    std::deque<Job*> m_jobs;
};

} // namespace lf
