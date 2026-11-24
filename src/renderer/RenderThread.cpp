#include "RenderThread.h"
#include "core/Log.h"
#include <QElapsedTimer>
#include <chrono>

namespace lf {

RenderThread::RenderThread(InputFn in, OutputFn out) : m_in(std::move(in)), m_out(std::move(out)) { setObjectName("lf-render"); }
RenderThread::~RenderThread() { requestStop(); wait(5000); }

void RenderThread::requestStop() { m_stop = true; m_jobCond.wakeAll(); }

bool RenderThread::validateEffect(const QString& glsl, QString* log, int timeoutMs) {
    if (!isRunning()) { if (log) *log = "renderer not running"; return false; }
    Job j; j.glsl = glsl; j.validate = true;
    QMutexLocker l(&m_jobMutex);
    m_jobs.push_back(&j);
    QElapsedTimer t; t.start();
    while (!j.done && t.elapsed() < timeoutMs) m_jobCond.wait(&m_jobMutex, 50);
    if (!j.done) { m_jobs.erase(std::remove(m_jobs.begin(), m_jobs.end(), &j), m_jobs.end()); if (log) *log = "validation timed out"; return false; }
    if (log) *log = j.log;
    return j.ok;
}

void RenderThread::invalidateProgram(const QString& key) {
    auto* j = new Job; j->key = key; j->validate = false;
    QMutexLocker l(&m_jobMutex); m_jobs.push_back(j);
}

void RenderThread::run() {
    GpuRenderer r;
    QString err;
    if (!r.initialize(&err)) {
        m_initError = err; logError("renderer", "GPU renderer failed to initialise: " + err);
        FaultRegistry::instance().report(FaultSeverity::Critical, "renderer", "GPU renderer failed: " + err, "Update the graphics driver or start in safe mode (--safe-mode).");
        return;
    }
    m_glInfo = r.glInfo();
    m_initOk = true;
    QElapsedTimer clock; clock.start();
    qint64 last = clock.nsecsElapsed();
    int slowFrames = 0, fastFrames = 0;
    while (!m_stop) {
        // jobs (validation / invalidation)
        {
            QMutexLocker l(&m_jobMutex);
            while (!m_jobs.empty()) {
                Job* j = m_jobs.front(); m_jobs.pop_front();
                if (j->validate) { j->ok = r.validateEffect(j->glsl, &j->log); j->done = true; }
                else { r.invalidateProgram(j->key); delete j; }
            }
            m_jobCond.wakeAll();
        }
        const double budgetMs = 1000.0 / std::max(1.0, double(m_targetFps));
        const qint64 now = clock.nsecsElapsed();
        const double dt = (now - last) / 1e9;
        last = now;
        if (m_paused) { m_watchdog.beat(); QThread::msleep(20); continue; }
        if (r.contextLost()) {
            FaultRegistry::instance().report(FaultSeverity::Critical, "renderer", "GPU context lost - recreating", "If this repeats, check the GPU driver.");
            r.releaseResources();
            if (!r.initialize(&err)) { QThread::msleep(500); continue; }
            FaultRegistry::instance().resolve("renderer", "GPU context lost");
        }
        RenderInput in = m_in(dt);
        switch (QualityMode(m_quality)) {
        case QualityMode::Low: m_scale = 0.5; break;
        case QualityMode::Medium: m_scale = 0.75; break;
        case QualityMode::High: m_scale = 1.0; break;
        case QualityMode::Adaptive: break;
        }
        in.qualityScale = m_scale;
        RenderOutput out = r.render(in);
        m_watchdog.beat();
        if (m_out) m_out(out);
        const double spent = (clock.nsecsElapsed() - now) / 1e6;
        if (QualityMode(m_quality) == QualityMode::Adaptive) {
            if (spent > budgetMs * 1.05) { if (++slowFrames > 10) { m_scale = std::max(0.35, m_scale - 0.1); slowFrames = 0; } fastFrames = 0; }
            else if (spent < budgetMs * 0.6) { if (++fastFrames > 60) { m_scale = std::min(1.0, m_scale + 0.05); fastFrames = 0; } slowFrames = 0; }
        }
        const double sleepMs = budgetMs - spent;
        if (sleepMs > 0.5) QThread::usleep((unsigned long)(sleepMs * 1000));
    }
    r.releaseResources();
}

} // namespace lf
