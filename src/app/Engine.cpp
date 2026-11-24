#include "Engine.h"
#include "core/Log.h"
#include "storage/ProjectIO.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QFont>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QUuid>
#include <cmath>

namespace lf {

QJsonObject ConsoleLink::toJson() const {
    return {{"name", name}, {"enabled", enabled}, {"universe", universe}, {"address", address}, {"mode", personalityModeName(mode)}, {"oscHost", oscHost}, {"oscPort", oscPort}};
}
ConsoleLink ConsoleLink::fromJson(const QJsonObject& o) {
    ConsoleLink c; c.name = o["name"].toString("grandMA3"); c.enabled = o["enabled"].toBool(); c.universe = o["universe"].toInt(1);
    c.address = o["address"].toInt(1); c.mode = personalityModeFromName(o["mode"].toString("Standard")); c.oscHost = o["oscHost"].toString(); c.oscPort = o["oscPort"].toInt();
    return c;
}

static quint64 hashString(const QString& s) {
    const QByteArray h = QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Md5);
    quint64 v; memcpy(&v, h.constData(), 8); return v;
}

Engine::Engine(const EngineOptions& opt, QObject* parent) : QObject(parent), m_opt(opt) {
    if (m_opt.dataDir.isEmpty()) m_opt.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (m_opt.dataDir.isEmpty()) m_opt.dataDir = QDir::home().filePath(".lumaflow");
    QDir().mkpath(m_opt.dataDir);
    Log::instance().setFile(QDir(m_opt.dataDir).filePath("lumaflow.log"));
    m_session = std::make_unique<SessionGuard>(m_opt.dataDir);
    m_effects.loadBuiltin();
    QDir().mkpath(QDir(m_opt.dataDir).filePath("effects"));
    if (!m_opt.safeMode) m_effects.loadDirectory(QDir(m_opt.dataDir).filePath("effects"));
    m_mapping = std::make_unique<MappingEngine>(&m_program.params);
    m_net = std::make_unique<NetworkIO>();
    m_mediaLib.open(QDir(m_opt.dataDir).filePath("media.sqlite"));
    m_videoOut.canvasWidth = opt.canvasWidth; m_videoOut.canvasHeight = opt.canvasHeight;
    m_consoles << ConsoleLink{"grandMA3", false, 1, 1, PersonalityMode::Standard32, {}, 0}
               << ConsoleLink{"MagicQ", false, 2, 1, PersonalityMode::Standard32, {}, 0};
    // external control slots (MIDI/OSC/DMX input nodes in the node editor)
    for (int i = 1; i <= 8; ++i) {
        ParameterDef d; d.id = QString("ext.%1").arg(i); d.name = QString("External %1").arg(i); d.group = "External"; d.defaultValue = 0;
        m_program.params.define(d); m_preview.params.define(d); m_outgoing.params.define(d);
    }
    updateIndexRanges();
    // default look
    setLayerEffect(0, "plasma");
    setLayerEffect(1, "particle_cloud"); m_program.params.set("layer2.blend", int(BlendMode::Additive));
    setLayerEffect(2, "energy_rings"); m_program.params.set("layer3.blend", int(BlendMode::Screen));
    setLayerEffect(3, "kaleidoscope");
    m_preview.copyFrom(m_program);
    m_outgoing.copyFrom(m_program);
    for (int i = 0; i < 64; ++i) { /* reserve */ }
    m_paramListener = m_program.params.addListener([this](const QString& id, double v, const QString& src) { onParam(id, v, src); });
    m_mapping->setActionHandler([this](const QString& a, double v, ControlSource s) { executeAction(a, v, s); });
    m_mapping->setFeedbackSender([this](const ControlMapping& m, double n) { sendFeedback(m, n); });
    m_spatial.resize(1);
    m_spatial[0].fx.name = "Spatial Effect 1";
    m_plugins.setPluginDirs({QDir(m_opt.dataDir).filePath("plugins"), QDir(QCoreApplication::applicationDirPath()).filePath("plugins"),
                             QDir(QCoreApplication::applicationDirPath()).filePath("../lib/lumaflow/plugins")});
    const QString probe = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
        "lumaflow-cli.exe"
#else
        "lumaflow-cli"
#endif
    );
    m_plugins.setProbeExecutable(probe);
    m_streams = std::make_unique<StreamOutputManager>(&m_plugins);
    m_showClock.start();
}

Engine::~Engine() { shutdown(); }

bool Engine::start() {
    if (!m_opt.safeMode) {
        m_plugins.scanAndLoad(&m_effects, false);
        updateIndexRanges();
    } else logWarn("engine", "SAFE MODE: plugins, custom effects, network output and remote control are disabled");
    if (m_opt.startNetwork) {
        connect(m_net.get(), &NetworkIO::oscReceived, this, &Engine::handleOsc, Qt::QueuedConnection);
        connect(m_net.get(), &NetworkIO::dmxInput, this, &Engine::handleDmxInput, Qt::QueuedConnection);
        NetworkConfig nc = m_net->config();
        if (m_opt.safeMode) { nc.artnetOut = nc.sacnOut = false; m_net->applyConfig(nc); }
        m_net->start();
    }
    m_midi = midi::Backend::createDefault();
    if (m_midi->available()) m_midi->openVirtual("LUMAFLOW");
    if (m_opt.startRenderer) {
        m_render = std::make_unique<RenderThread>([this](double dt) { return buildRenderInput(dt); }, [this](const RenderOutput& o) { onRenderOutput(o); });
        m_render->setTargetFps(m_opt.fps);
        if (m_opt.safeMode) m_render->setQualityMode(QualityMode::Low);
        m_render->start();
        m_render->watchdog().start(2000,
            [](int ms) { FaultRegistry::instance().report(FaultSeverity::Critical, "renderer", QString("render thread stalled for %1 ms").arg(ms), "Outputs hold the last frame. If the stall persists, disable the most recently changed effect or restart in safe mode."); },
            [] { FaultRegistry::instance().resolve("renderer", "render thread stalled"); logInfo("renderer", "render thread recovered"); });
    }
    connect(&m_tick, &QTimer::timeout, this, &Engine::controlTick);
    m_tick.setTimerType(Qt::PreciseTimer);
    m_tick.start(16);
    m_session->begin(m_projectPath);
    m_loadedAt = showTime();
    return true;
}

void Engine::shutdown() {
    if (m_tick.isActive()) m_tick.stop();
    if (m_render) { m_render->watchdog().stop(); m_render->requestStop(); m_render->wait(5000); m_render.reset(); }
    m_recorder.finish();
    if (m_streams) m_streams->stop();
    if (m_audioIn) { m_audioIn->stop(); m_audioIn.reset(); }
    m_remote.stop();
    m_sync.stop();
    if (m_net) m_net->stop();
    if (m_session) m_session->end();
}

void Engine::updateIndexRanges() {
    const int ne = int(m_effects.ids().size()), ns = std::max<int>(1, int(m_scenes.size()));
    m_program.updateIndexRanges(ne, ns); m_preview.updateIndexRanges(ne, ns); m_outgoing.updateIndexRanges(ne, ns);
}

// ---------------------------------------------------------------- layers
void Engine::setLayerEffect(int layer, const QString& id, bool pv) {
    if (!m_effects.contains(id)) { logWarn("engine", "unknown effect " + id); return; }
    QMutexLocker l(&m_state);
    Composition& c = pv ? m_preview : m_program;
    if (layer < 0 || layer >= c.layerCount()) return;
    const bool changed = c.layers[layer].effectId != id || c.layers[layer].type != SourceType::Effect;
    c.layers[layer].type = SourceType::Effect;
    c.layers[layer].effectId = id;
    if (changed) applyEffectDefaults(layer, pv);
    c.params.set(Composition::pid(layer, "effect"), m_effects.indexOf(id), "internal-sync");
    ++m_showRevision;
    l.unlock();
    emit showStateChanged();
}

void Engine::applyEffectDefaults(int layer, bool pv) {
    Composition& c = pv ? m_preview : m_program;
    const EffectDef e = m_effects.get(c.layers[layer].effectId);
    for (int i = 0; i < 4; ++i) c.params.set(Composition::pid(layer, QString("p%1").arg(i + 1).toLatin1().constData()), e.params[i].defaultValue, "defaults");
    c.layers[layer].colorA = e.colorA; c.layers[layer].colorB = e.colorB;
}

void Engine::setLayerMedia(int layer, const QString& path, bool pv) {
    QMutexLocker l(&m_state);
    Composition& c = pv ? m_preview : m_program;
    if (layer < 0 || layer >= c.layerCount()) return;
    c.layers[layer].type = QFileInfo(path).suffix().toLower().contains(QRegularExpression("png|jpg|jpeg|bmp|gif|webp|svg|tif")) ? SourceType::Image : SourceType::Media;
    c.layers[layer].mediaPath = path;
    ++m_showRevision;
    l.unlock();
    m_mediaLib.add(path);
    emit showStateChanged();
}

void Engine::setLayerText(int layer, const QString& text) { QMutexLocker l(&m_state); if (layer >= 0 && layer < m_program.layerCount()) { m_program.layers[layer].text = text; ++m_showRevision; } }
void Engine::setLayerSource(int layer, const LayerSource& src, bool pv) { QMutexLocker l(&m_state); Composition& c = pv ? m_preview : m_program; if (layer >= 0 && layer < c.layerCount()) { c.layers[layer] = src; ++m_showRevision; } l.unlock(); emit showStateChanged(); }
void Engine::setLayerColors(int layer, const std::array<double, 3>& a, const std::array<double, 3>& b) { QMutexLocker l(&m_state); if (layer >= 0 && layer < m_program.layerCount()) { m_program.layers[layer].colorA = a; m_program.layers[layer].colorB = b; ++m_showRevision; } }
std::array<double, 3> Engine::layerColorA(int layer) const { QMutexLocker l(&m_state); return m_program.layers.value(layer).colorA; }
std::array<double, 3> Engine::layerColorB(int layer) const { QMutexLocker l(&m_state); return m_program.layers.value(layer).colorB; }

bool Engine::validateShader(const QString& glsl, QString* log) {
    if (!m_render || !m_render->isRunning()) { if (log) *log = "renderer not running - cannot validate"; return false; }
    return m_render->validateEffect(glsl, log);
}

bool Engine::addCustomEffect(const EffectDef& e, QString* err) {
    if (m_opt.safeMode) { if (err) *err = "custom effects are disabled in safe mode"; return false; }
    QString log;
    QString glsl = e.glsl;
    if (!validateShader(glsl, &log)) {
        // automatic repair attempt for common mistakes
        const QString repaired = EffectAssistant::repairGlsl(glsl, log);
        QString log2;
        if (repaired == glsl || !validateShader(repaired, &log2)) { if (err) *err = log; return false; }
        glsl = repaired; logInfo("effects", "custom effect " + e.id + " compiled after automatic repair");
    }
    EffectDef d = e; d.glsl = glsl;
    if (d.source.isEmpty() || d.source == "builtin") d.source = "custom";
    m_effects.add(d);
    if (m_render) m_render->invalidateProgram(d.id);
    EffectLibrary::saveEffectFile(QDir(m_opt.dataDir).filePath("effects/" + d.id + ".lfeffect"), d);
    updateIndexRanges();
    ++m_showRevision;
    emit showStateChanged();
    return true;
}

bool Engine::compileNodeEffect(const NodeGraph& g, const QString& id, QString* err, EffectDef* out) {
    const CompileResult r = g.compile();
    if (!r.ok) { if (err) *err = r.errors.join("\n"); return false; }
    EffectDef e; e.id = id; e.name = g.name; e.category = "Custom"; e.source = "custom"; e.glsl = r.glsl; e.nodeGraph = g.toJson();
    e.description = QString("Node graph (%1 nodes)").arg(r.nodeCount);
    for (int i = 0; i < 4; ++i) { e.params[i].name = r.exposedParams.value(i, "-"); e.params[i].defaultValue = 0.5; }
    e.colorA = {0.1, 0.6, 1.0}; e.colorB = {1.0, 0.2, 0.6};
    if (out) *out = e;
    return addCustomEffect(e, err);
}

QImage Engine::textImage(const QString& text) {
    auto it = m_textCache.find(text);
    if (it != m_textCache.end()) return *it;
    QFont font("Sans", 96, QFont::Bold);
    QFontMetrics fm(font);
    const int w = std::max(64, fm.horizontalAdvance(text) + 40), h = fm.height() + 20;
    QImage img(w, h, QImage::Format_RGBA8888); img.fill(Qt::transparent);
    QPainter p(&img); p.setRenderHint(QPainter::TextAntialiasing); p.setFont(font); p.setPen(Qt::white);
    p.drawText(img.rect(), Qt::AlignCenter, text); p.end();
    if (m_textCache.size() > 32) m_textCache.clear();
    m_textCache.insert(text, img);
    return img;
}

// ---------------------------------------------------------------- render input
LayerSnapshot Engine::snapshotLayer(Composition& c, int i, double dt, std::array<double, 2>& clock, QHash<QString, std::shared_ptr<MediaPlayer>>& players, bool advance) {
    LayerSnapshot s;
    const LayerSource& src = c.layers[i];
    s.visible = src.enabled;
    s.opacity = float(c.lp(i, "opacity"));
    const double speed = c.lp(i, "speed");
    if (advance) clock[0] += dt * speed;
    s.time = float(std::fmod(clock[0], 100000.0));
    s.scale = float(c.lp(i, "scale")); s.rotation = float(c.lp(i, "rotation"));
    s.posX = float(c.lp(i, "posx")); s.posY = float(c.lp(i, "posy"));
    s.hue = float(c.lp(i, "hue")); s.hue2 = float(c.lp(i, "hue2"));
    s.saturation = float(c.lp(i, "saturation")); s.brightness = float(c.lp(i, "brightness")); s.contrast = float(c.lp(i, "contrast"));
    for (int k = 0; k < 4; ++k) s.p[k] = float(c.lp(i, QString("p%1").arg(k + 1).toLatin1().constData()));
    s.blend = BlendMode(std::clamp(int(c.lp(i, "blend")), 0, 4));
    s.mask = src.mask; s.maskSize = float(src.maskSize); s.maskFeather = float(src.maskFeather);
    s.audioAmount = float(c.lp(i, "audio") * m_program.params.value("audio.reactivity") * 2.0);
    s.colorA = {float(src.colorA[0]), float(src.colorA[1]), float(src.colorA[2])};
    s.colorB = {float(src.colorB[0]), float(src.colorB[1]), float(src.colorB[2])};
    if (src.type == SourceType::Effect) {
        const EffectDef e = m_effects.get(src.effectId);
        s.programKey = e.id; s.glsl = e.glsl; s.glslHash = hashString(e.glsl);
        if (e.id == "text") s.texture = textImage(src.text);
        if (e.id == "logo" && !src.mediaPath.isEmpty()) {
            auto& pl = players[src.mediaPath];
            if (!pl) { pl = std::make_shared<MediaPlayer>(); pl->open(src.mediaPath); }
            s.texture = pl->currentFrame();
        }
        // photosensitive safety: cap strobe-effect flash rate (rate = 1 + p1*11 Hz)
        if (e.id == "strobe" && m_safety.limiterEnabled) s.p[0] = std::min<float>(s.p[0], float(std::max(0.0, (m_safety.maxStrobeHz - 1.0) / 11.0)));
        s.feedback = e.id == "feedback";
    } else {
        s.programKey = "__media";
        auto& pl = players[src.mediaPath];
        if (!pl) {
            pl = std::make_shared<MediaPlayer>();
            if (!pl->open(src.mediaPath)) FaultRegistry::instance().report(FaultSeverity::Warning, "media", "cannot open media: " + src.mediaPath, "Relink the file in Media Library > Relink.");
        }
        pl->setSpeed(speed); pl->setLoop(src.mediaLoop); pl->setInOut(src.mediaIn, src.mediaOut); pl->setReverse(src.mediaReverse);
        s.texture = advance ? pl->advance(dt) : pl->currentFrame();
        if (s.texture.isNull()) s.visible = false;
    }
    return s;
}

RenderInput Engine::buildRenderInput(double dt) {
    RenderInput in;
    QMutexLocker l(&m_state);
    in.width = m_videoOut.canvasWidth; in.height = m_videoOut.canvasHeight;
    auto build = [&](Composition& c, std::array<std::array<double, 2>, kMaxLayers>& clocks, QHash<QString, std::shared_ptr<MediaPlayer>>& players, bool advance) {
        CompositionSnapshot cs;
        bool anySolo = false; for (auto& ls : c.layers) anySolo |= ls.solo;
        for (int i = 0; i < c.layerCount(); ++i) {
            LayerSnapshot s = snapshotLayer(c, i, dt, clocks[i], players, advance);
            if (anySolo && !c.layers[i].solo) s.visible = false;
            cs.layers << s;
        }
        return cs;
    };
    in.program = build(m_program, m_clockP, m_playersP, true);
    in.preview = build(m_preview, m_clockV, m_playersV, true);
    in.transition = float(transitionProgress());
    if (in.transition < 1.0f) in.outgoing = build(m_outgoing, m_clockO, m_playersO, true);
    if (m_testPattern >= 0) {
        CompositionSnapshot tp; LayerSnapshot s; const EffectDef e = m_effects.get("test_pattern");
        s.visible = true; s.programKey = e.id; s.glsl = e.glsl; s.glslHash = hashString(e.glsl); s.p = {float(m_testPattern) / 3.99f + 0.001f, 0.5f, 0, 0};
        s.time = float(showTime());
        tp.layers << s; in.program = tp; in.transition = 1.0f;
    }
    ParameterRegistry& p = m_program.params;
    in.master.intensity = float(p.value("master.intensity"));
    in.master.strobe = float(p.value("master.strobe"));
    in.master.saturation = float(p.value("master.saturation"));
    in.master.tint = {float(p.value("master.red")), float(p.value("master.green")), float(p.value("master.blue"))};
    in.master.blackout = m_blackout || p.value("master.blackout") > 0.5;
    in.master.time = float(showTime());
    const AudioFeatures af = m_audio.features();
    in.audio = {af.bands[1], af.bands[3], af.bands[5], std::min(1.0f, af.level * 4.0f)};
    in.beat = af.beatPulse;
    for (int i = 0; i < 32; ++i) in.spectrum[i] = af.spectrum.size() >= 64 ? std::max(af.spectrum[i * 2], af.spectrum[i * 2 + 1]) : 0.f;
    for (int i = 0; i < 8; ++i) in.ext[i] = float(p.value(QString("ext.%1").arg(i + 1)));
    in.timecode = float(m_tcSource == "internal" ? m_timeline.position() : m_tcSeconds);
    in.safety = m_safety;
    in.calibration = m_calibration;
    return in;
}

void Engine::onRenderOutput(const RenderOutput& out) {
    // runs on the render thread: pixel mapping + spatial effects -> DMX (bounded CPU work)
    QMap<int, DmxFrame> dmx;
    {
        QMutexLocker l(&m_state);
        if (m_pixelTest > 0) m_pixelMap.renderTestPattern(m_pixelTest, out.time, m_patch, dmx);
        else if (!out.program.isNull()) m_pixelMap.render(out.program, m_patch, dmx);
        const double master = m_program.params.value("master.intensity") * ((m_blackout || m_program.params.value("master.blackout") > 0.5) ? 0.0 : 1.0);
        for (const auto& slot : m_spatial) if (slot.enabled) {
            ColorCalibration cal; cal.maxBrightness = slot.level * master;
            SpatialEngine::render(m_patch, slot.fx, out.time, dmx, cal);
        }
        if (m_blackout) for (auto& f : dmx) f.fill(0);
    }
    for (auto it = dmx.begin(); it != dmx.end(); ++it) m_net->setOutput(it.key(), it.value());
    if (m_recorder.recording() && !out.program.isNull()) m_recorder.addFrame(out.program);
    if (!out.program.isNull()) m_streams->submit(out.program, out.time);   // non-blocking mailbox
    {
        QMutexLocker l(&m_frameMutex);
        m_lastProgram = out.program; m_lastPreview = out.preview; m_lastDmx = dmx; m_lastStats = out.stats;
    }
    QMetaObject::invokeMethod(this, "frameReady", Qt::QueuedConnection);
}

QImage Engine::lastProgramFrame() const { QMutexLocker l(&m_frameMutex); return m_lastProgram; }
QImage Engine::lastPreviewFrame() const { QMutexLocker l(&m_frameMutex); return m_lastPreview; }
QMap<int, DmxFrame> Engine::lastDmxOutput() const { QMutexLocker l(&m_frameMutex); return m_lastDmx; }

} // namespace lf

namespace lf {
void Engine::applyStreamConfigs(const QVector<StreamConfig>& c) {
    QVector<StreamConfig> cfg = c;
    if (m_opt.safeMode) for (auto& x : cfg) x.enabled = false;   // safe mode: no stream outputs
    m_streams->setRegions(m_videoOut.outputs);
    m_streams->setFrameRate(m_render ? m_render->targetFps() : m_opt.fps);
    m_streams->setConfigs(cfg);
    ++m_showRevision;
}
} // namespace lf
